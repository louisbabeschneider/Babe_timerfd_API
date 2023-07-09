/*
 * Timerfd_API.h
 *
 *  Created on: May 16, 2022
 *      Author: Louis SCHNEIDER (louis babe)
 */

#ifndef TIMERFD_API_H_
#define TIMERFD_API_H_
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <sys/timerfd.h>
#include <pthread.h>
#include <poll.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>

#define MAX_PROCESS_TIMER_NB 100
#define MILLION 1000000
#define BILLION 1000000000
typedef unsigned long int time_usec;
typedef unsigned long long int time_nsec;

typedef enum {
	UNKNOWN_TIMEOUT_REASON,
	PERIODIC_CALL
} E_TIMEOUT_DEFAULT_REASON;

typedef enum {
	UNKNOWN_INTERFACE_TO_REASON,
	INTERFACE_TO_NO_NODEJS_CLIENT
} E_TIMEOUT_INTERFACE_REASON;

typedef union U_TIMEOUT_REASON{
	E_TIMEOUT_DEFAULT_REASON default_reason;
	E_TIMEOUT_INTERFACE_REASON interface_reason;
}U_TIMEOUT_REASON;


typedef void (*time_handler)(size_t timer_id, U_TIMEOUT_REASON user_data);

typedef struct S_TIMER_INFO{
    int                  fd;
    time_handler         callback;
    U_TIMEOUT_REASON     user_data;
    unsigned int         count;
    unsigned int         interval;
    bool                 is_periodic;
    long int             next_interval_value;
    struct S_TIMER_INFO  * next;
}S_TIMER_INFO;

typedef struct {
	S_TIMER_INFO * thread_timer_stack;
	int nb_thread_timer_running;
	int max_timer_fd;
	bool is_fd_change;
}S_MANAGER_TIMER_INFO;

S_TIMER_INFO * timerfd_api_get_node_from_fd(S_MANAGER_TIMER_INFO * thread_timer_info, int fd);
int timerfd_api_start_timer(S_MANAGER_TIMER_INFO * thread_timer_info, time_usec interval_usec, time_handler handler, bool is_periodic, U_TIMEOUT_REASON user_data);
void timerfd_api_stop_timer(S_MANAGER_TIMER_INFO * thread_timer_info, S_TIMER_INFO *timer_struct);
void timerfd_api_stop_timer_from_fd(S_MANAGER_TIMER_INFO * thread_timer_info, int fd);
void timerfd_api_stop_all_timers(S_MANAGER_TIMER_INFO * thread_timer_info);
int timerfd_api_check_stack_timer_and_update_poll_fd(S_MANAGER_TIMER_INFO *thread_timer_info, struct pollfd * poll_fd);
void thread_manager__process_timer_fd(S_MANAGER_TIMER_INFO * thread_timer_info_stack, int fd);
time_usec timerfd_api_gettime(int fd);
void timerfd_api_settime_and_restart(int fd, time_usec new_interval);
void timerfd_api_settime_to_zero(int fd);
void timerfd_api_settime_at_next_cycle(int fd, time_usec new_interval);
void timerfd_api_settime_now(int fd, time_usec new_interval);
void timerfd_api_init_mutex(void);
void timerfd_api_destroy_mutex(void);
#endif /* TIMERFD_API_H_ */
