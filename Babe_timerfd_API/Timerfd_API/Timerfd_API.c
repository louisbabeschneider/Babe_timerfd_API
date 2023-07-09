/*
 * Timerfd_API.c
 *
 *  Created on: May 16, 2022
 *      Author: Louis SCHNEIDER (louis babe)
 */
#include "../Timerfd_API/Timerfd_API.h"
static const char *const timerfd_api_logtag = "Timerfd_API";
static int nb_process_timer_running = 0;
static pthread_mutex_t timer_mutex;

/* brief - Init timer api mutex for thread-safe
 */
void timerfd_api_init_mutex(void) {
	if(pthread_mutex_init(&timer_mutex, NULL) != 0){
		printf( "line.%d : Error, failed to create timerfd_api mutex", __LINE__);
	}
}

/* brief - Destroy timer api mutex
*/
void timerfd_api_destroy_mutex(void) {
	pthread_mutex_destroy(&timer_mutex);
	printf("timer API mutex is DONE");
	if(nb_process_timer_running != 0){
		printf( "line.%d : ERROR, Not all timers have been stopped, %d have been forgotten :(", __LINE__, nb_process_timer_running);
	}
}

/* brief - Start new timer and malloc S_TIMER_INFO struct
 * return timer file descriptor if malloc OK, and NULL otherwise */
int timerfd_api_start_timer(S_MANAGER_TIMER_INFO * thread_timer_info, time_usec interval_usec, time_handler handler, bool is_periodic, U_TIMEOUT_REASON user_data) {
	S_TIMER_INFO *new_node = NULL;
	int retfd = 0;
	struct itimerspec new_value;
	pthread_mutex_lock(&timer_mutex);
	if ((nb_process_timer_running < MAX_PROCESS_TIMER_NB) && (thread_timer_info->nb_thread_timer_running < thread_timer_info->max_timer_fd)) {
		new_node = (S_TIMER_INFO*) malloc(sizeof(S_TIMER_INFO));
		if (new_node != NULL) {
			new_node->fd = timerfd_create(CLOCK_REALTIME, 0);
			if (new_node->fd != -1) {
				new_node->callback = handler;
				new_node->user_data = user_data;
				new_node->interval = interval_usec;
				new_node->is_periodic = is_periodic;
				new_node->count = 0;
				new_value.it_value.tv_sec = interval_usec / MILLION;
				new_value.it_value.tv_nsec = (interval_usec % MILLION) * 1000;
				if (is_periodic == true) {
					new_value.it_interval.tv_sec = interval_usec / MILLION;
					new_value.it_interval.tv_nsec = (interval_usec % MILLION) * 1000;
				} else {
					new_value.it_interval.tv_sec = 0;
					new_value.it_interval.tv_nsec = 0;
				}
				if(timerfd_settime(new_node->fd, 0, &new_value, NULL) == 0){
					/*Inserting the timer node into the stack list*/
					new_node->next = thread_timer_info->thread_timer_stack;
					thread_timer_info->thread_timer_stack = new_node;
					printf("+++ Create node timerfd (fd:%d)", new_node->fd);
					retfd = new_node->fd;
					nb_process_timer_running++;
					thread_timer_info->nb_thread_timer_running++;
					thread_timer_info->is_fd_change = true;
				} else {
					printf( "line.%d : ERROR, timerfd_settime failed, stderr : (%s)",__LINE__, strerror(errno));
				}
			} else {
				printf( "line.%d : ERROR, timerfd_create failed, free new_node, stderr : (%s)", __LINE__, strerror(errno));
				free(new_node);
			}
		} else {
			printf( "line.%d : ERROR, malloc failed\n", __LINE__);

		}
	} else {
		printf( "line.%d : ERROR, Maximum number of timers allowed reached !! nb of timers in thread :%d (max:%d), in process :%d (max:%d) \n",
		__LINE__, thread_timer_info->nb_thread_timer_running, thread_timer_info->max_timer_fd, nb_process_timer_running, MAX_PROCESS_TIMER_NB);
	}
	pthread_mutex_unlock(&timer_mutex);
	return retfd;
}

/* brief - Close file descriptor timer and free node struct timer if timer present in thread_timer_stack stack
 */
void timerfd_api_stop_timer(S_MANAGER_TIMER_INFO * thread_timer_info , S_TIMER_INFO *timer_struct) {
	S_TIMER_INFO *node_tmp = NULL;
	S_TIMER_INFO *node_to_free = timer_struct;
	bool timer_active = false;

	/* First check if there is timer to free*/
	if (thread_timer_info->thread_timer_stack != NULL) {
		/* First check if pointer is not NULL */
		if (node_to_free != NULL) {
			/* if node to free is the head of the stack */
			if (node_to_free == thread_timer_info->thread_timer_stack) {
				thread_timer_info->thread_timer_stack = thread_timer_info->thread_timer_stack->next;
				timer_active = true;
			} else {
				node_tmp = thread_timer_info->thread_timer_stack;
				while ((node_tmp->next != node_to_free)
						&& (node_tmp->next != NULL)) {
					node_tmp = node_tmp->next;
				}
				/* if node_to_free is founded in stack list*/
				if ((node_tmp->next != NULL)) {
					node_tmp->next = node_tmp->next->next;
					timer_active = true;
				}
			}
			/* If timer is active -> close fd and free struct */
			if (timer_active) {
				printf("--- Delete node timerfd (fd:%d)", node_to_free->fd);
				/* update nb thread/process timer running, and update flag */
				pthread_mutex_lock(&timer_mutex);
				nb_process_timer_running--;
				pthread_mutex_unlock(&timer_mutex);
				thread_timer_info->nb_thread_timer_running--;
				thread_timer_info->is_fd_change = true;
				timerfd_api_settime_to_zero(node_to_free->fd);
				close(node_to_free->fd);
				free(node_to_free);
			} else {
				printf( "line.%d : Error : Timer is not active !\n",
				__LINE__);
			}
		} else {
			printf( "line.%d : Error, parameter pointer is NULL\n",
			__LINE__);
		}
	} else {
		printf( "line.%d : Error, No more timer existing !\n", __LINE__);
	}

}

/* brief - Close file descriptor timer and free the memory of the fd's node given in parameter
 * (if present in stack given in parameter)
 */
void timerfd_api_stop_timer_from_fd(S_MANAGER_TIMER_INFO * thread_timer_info_ptr, int fd) {
	S_TIMER_INFO * tmp_ptr_timer_node = NULL;
	tmp_ptr_timer_node = timerfd_api_get_node_from_fd(thread_timer_info_ptr, fd);
	if(tmp_ptr_timer_node != NULL){
		timerfd_api_stop_timer(thread_timer_info_ptr, tmp_ptr_timer_node);
	} else{
		printf( "line.%d : Error, Timer not founded in stack !\n", __LINE__);
	}
}

/* brief - Close all file descriptor timers and free the memory of all the nodes of the stack given in parameter
 */
void timerfd_api_stop_all_timers(S_MANAGER_TIMER_INFO * thread_timer_info) {
	int incr = 0;
	int nb_timer_to_stop = thread_timer_info->nb_thread_timer_running;

	if (thread_timer_info->thread_timer_stack == NULL ) {
		printf("All thead's stack timer are already stopped \n");
	} else {

		while ((incr < thread_timer_info->max_timer_fd) && (thread_timer_info->thread_timer_stack != NULL)){
			timerfd_api_stop_timer(thread_timer_info, thread_timer_info->thread_timer_stack);
			incr++;
		}
		if (thread_timer_info->thread_timer_stack != NULL){
			printf( "line.%d : ERROR, incr (=%d) stop node timer is greater than max thread timer number allowed(%d) : "
					"coding problem, thread timer stack is not NULL at end of %s() function call\n", __LINE__,incr , thread_timer_info->max_timer_fd, __FUNCTION__);
		} else if(incr == nb_timer_to_stop){
			printf("API call stopped %d timer.", incr);
		} else if(incr > nb_timer_to_stop){
			printf( "line.%d : ERROR, stop and delete (=%d) node, more than expected (=%d)\n", __LINE__,incr , nb_timer_to_stop);
		} else if(incr < nb_timer_to_stop){
			printf( "line.%d : ERROR, stop and delete (=%d) node, less than expected (=%d)\n", __LINE__,incr , nb_timer_to_stop);
		}
	}
}

/* brief - This function searches for the timer corresponding to the fd given in the stack given (fd and stack are parameter data)
 * return -  Timer node pointer if exists, and NULL otherwise
 */
S_TIMER_INFO* timerfd_api_get_node_from_fd(S_MANAGER_TIMER_INFO *thread_timer_info, int fd) {
	S_TIMER_INFO *tmp_node = thread_timer_info->thread_timer_stack;
	S_TIMER_INFO *ret_node = NULL;
	int incr = 0;
	while ((tmp_node != NULL) && (incr < thread_timer_info->nb_thread_timer_running)) {
		if (tmp_node->fd == fd){
			ret_node =  tmp_node;
			break;
		} else {
			incr++;
			tmp_node = tmp_node->next;
		}
	}
	return ret_node;
}

/* brief - This function reads the thread's timer structure to see if the list of timer file descriptors is up to date.
 * The function clear the number of pollfd array elements corresponding to the max_thread_timer parameter.
 * And this function fill the pollfd array with the active fd timers (You must enter the address of the first element to be filled).
 * return - true if is_fd_change is true, and false otherwise
 */
int timerfd_api_check_stack_timer_and_update_poll_fd(S_MANAGER_TIMER_INFO *thread_timer_info, struct pollfd * poll_fd){
	int retval;
	if (thread_timer_info->is_fd_change == true){
		S_TIMER_INFO * tmp_timer_stack = thread_timer_info->thread_timer_stack;
		memset(poll_fd, 0, (sizeof(struct pollfd)* thread_timer_info->nb_thread_timer_running));
		int tmp_incr;
		for (tmp_incr = 0; tmp_incr < thread_timer_info->nb_thread_timer_running; tmp_incr++){
			if(tmp_timer_stack != NULL){
				poll_fd[tmp_incr].fd = tmp_timer_stack->fd;
				poll_fd[tmp_incr].events = POLLIN;
				poll_fd[tmp_incr].revents = 0;
				tmp_timer_stack = tmp_timer_stack->next;
			}
			else{
				printf( "line.%d : ERROR, Coding problem !! nb timer running and stack do not match",__LINE__);
			}
		}
		thread_timer_info->is_fd_change = false;
	}
	retval = thread_timer_info->nb_thread_timer_running;
	return retval;
}

/* brief - This function Call the handler function of timer corresponding to the fd giver in parameter
 * The function check if timer is periodic, if not the function stop and free the timer
 */
void thread_manager__process_timer_fd(S_MANAGER_TIMER_INFO * thread_timer_info_stack, int fd){
	uint64_t exp;
	S_TIMER_INFO * tmp_timer_node;
	int tmp;
	tmp = read(fd, &exp, sizeof(uint64_t));
	tmp_timer_node = timerfd_api_get_node_from_fd(thread_timer_info_stack, fd);
	if(tmp_timer_node != NULL){
	tmp_timer_node->callback((size_t) tmp_timer_node, tmp_timer_node->user_data);
		if(tmp_timer_node->is_periodic == false){
			timerfd_api_stop_timer(thread_timer_info_stack, tmp_timer_node);
		}
	}else{
		printf( "line.%d : ERROR, Try to process unknown timer file descriptor", __LINE__);
	}
}


/* brief - Return timer remaining time value in us
 */
time_usec timerfd_api_gettime(int fd){
	struct itimerspec current_time;
	time_usec current_time_usec = 0;
	if(timerfd_gettime(fd, &current_time) == 0){
		current_time_usec = (time_usec) (current_time.it_value.tv_nsec / 1000) + (current_time.it_value.tv_sec * MILLION);
		printf("------>> POUET >>> inter(s) = %ld, inter(ns) = %ld, val(s) = %ld, val(ns) = %ld\n",
				current_time.it_interval.tv_sec, current_time.it_interval.tv_nsec, current_time.it_value.tv_sec, current_time.it_value.tv_nsec);
	}else {
		printf( "line.%d : ERROR, timerfd_gettime failed, stderr : (%s)",__LINE__, strerror(errno));
	}

	return current_time_usec;
}

/* brief - Set new timer interval value from fd
 */
void timerfd_api_settime_and_restart(int fd, time_usec new_interval){
	struct itimerspec new_timer_value;
	new_timer_value.it_interval.tv_sec = (int) (new_interval / MILLION);
	new_timer_value.it_interval.tv_nsec = (new_interval % MILLION) * 1000;
	new_timer_value.it_value.tv_sec = new_timer_value.it_interval.tv_sec;
	new_timer_value.it_value.tv_nsec = new_timer_value.it_interval.tv_nsec;
	if(timerfd_settime(fd, 0,(const struct itimerspec *)&new_timer_value, NULL) != 0){
		printf( "line.%d : ERROR, timerfd_settime failed, stderr : (%s)",__LINE__, strerror(errno));
	}
}


/* brief - Set timer value to 0 -> disarms the timer
 */
void timerfd_api_settime_to_zero(int fd){
	struct itimerspec new_timer_value;
	new_timer_value.it_interval.tv_sec = 0;
	new_timer_value.it_interval.tv_nsec = 0;
	new_timer_value.it_value.tv_sec = 0;
	new_timer_value.it_value.tv_nsec = 0;
	if(timerfd_settime(fd, 0,(const struct itimerspec *)&new_timer_value, NULL) != 0){
		printf( "line.%d : ERROR, timerfd_settime failed, stderr : (%s)",__LINE__, strerror(errno));
	}
}

/* brief - Set new timer interval value from fd after current interval value expires
 * Reserved for periodic timers
 */
void timerfd_api_settime_at_next_cycle(int fd, time_usec new_interval){
	struct itimerspec new_timer_value;
	if(timerfd_gettime(fd, &new_timer_value) == 0){
		new_timer_value.it_interval.tv_sec = (int) (new_interval / MILLION);
		new_timer_value.it_interval.tv_nsec = (new_interval % MILLION) * 1000;
		if(timerfd_settime(fd, 0,(const struct itimerspec *)&new_timer_value, NULL) != 0){
			printf( "line.%d : ERROR, timerfd_settime failed, stderr : (%s)",__LINE__, strerror(errno));
		}
	}
	else {
		printf( "line.%d : ERROR, timerfd_gettime failed, stderr : (%s)",__LINE__, strerror(errno));
	}
}

/* brief - Set new timer interval value :
 *  > if elapsed time is greater than desired interval value, we restart with new interval
 *  > else, we re-arm the timer with the value : elapsed time - desired interval
 */
void timerfd_api_settime_now(int fd, time_usec new_interval_usec){
	time_nsec new_interval_nsec = (time_nsec) (new_interval_usec * 1000);
	time_nsec elapsed_time_nsec;
	struct itimerspec tmp_timer_value;
	if(timerfd_gettime(fd, &tmp_timer_value) == 0){
		elapsed_time_nsec = ((((long int) tmp_timer_value.it_interval.tv_sec - tmp_timer_value.it_value.tv_sec) * BILLION)
				+ ((tmp_timer_value.it_interval.tv_nsec - tmp_timer_value.it_value.tv_nsec)));
		memset(&tmp_timer_value, 0, sizeof(struct itimerspec));

		if(elapsed_time_nsec < new_interval_nsec){
			tmp_timer_value.it_value.tv_sec = (int) ((new_interval_nsec - elapsed_time_nsec) / BILLION);
			tmp_timer_value.it_value.tv_nsec = ((new_interval_nsec - elapsed_time_nsec) % BILLION);
			tmp_timer_value.it_interval.tv_sec = (int) (new_interval_nsec / BILLION);
			tmp_timer_value.it_interval.tv_nsec = (new_interval_nsec % BILLION);
			if(timerfd_settime(fd, 0,(const struct itimerspec *)&tmp_timer_value, NULL) != 0){
			}
		}else{
			tmp_timer_value.it_value.tv_sec = 0;
			tmp_timer_value.it_value.tv_nsec = 0;
			tmp_timer_value.it_interval.tv_sec = (int) (new_interval_nsec / BILLION);
			tmp_timer_value.it_interval.tv_nsec = (new_interval_nsec % BILLION);
			if(timerfd_settime(fd, 0,(const struct itimerspec *)&tmp_timer_value, NULL) != 0){
			}
		}
	}
	else {
		printf( "line.%d : ERROR, timerfd_gettime failed, stderr : (%s)",__LINE__, strerror(errno));
	}
}

