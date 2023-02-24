#pragma once

#include <time.h>

struct stat {
	/* as seen by input simulators (required for assignment) */
	unsigned long int runs;
	unsigned long int overruns;
	struct timespec sum_reply_times;
	struct timespec max_reply_time;

	/* as seen by controller (optional, just curious) */
	unsigned long int cnt_runs;
	unsigned long int cnt_overruns;
	struct timespec cnt_sum_reply_times;
	struct timespec cnt_max_reply_time;
};

struct input {
	const int id;
	const struct timespec period;
	struct timespec first_run;

	int state;
	struct timespec last_change;

	int reply;
	int last_state;
	struct timespec last_replay;

	struct stat stat;
};


#define TIMESPEC_ADD(A,B) /* A += B */ \
do {                                   \
    (A).tv_sec += (B).tv_sec;          \
    (A).tv_nsec += (B).tv_nsec;        \
    if ( (A).tv_nsec >= 1000000000 ) { \
        (A).tv_sec++;                  \
        (A).tv_nsec -= 1000000000;     \
    }                                  \
} while (0)

#define TIMESPEC_SUB(A,B) /* A -= B */ \
do {                                   \
    (A).tv_sec -= (B).tv_sec;          \
    (A).tv_nsec -= (B).tv_nsec;        \
    if ( (A).tv_nsec < 0 ) {           \
        (A).tv_sec--;                  \
        (A).tv_nsec += 1000000000;     \
    }                                  \
} while (0)

#define TIMESPEC_GT(A,B) /* A > B? */ \
( (A).tv_sec > (B).tv_sec || ((A).tv_sec == (B).tv_sec && (A).tv_nsec > (B).tv_nsec) )

#define PRINT(format, ...)                           \
do {                                                  \
    struct timespec _t_;                               \
    clock_gettime(CLOCK_MONOTONIC, &_t_);               \
    TIMESPEC_SUB(_t_, t0);                               \
    pthread_mutex_lock(&print_lock);                      \
    printf("[%03ld:%06ld] " format,                        \
        _t_.tv_sec % 100, _t_.tv_nsec/1000, ##__VA_ARGS__); \
    pthread_mutex_unlock(&print_lock);                       \
} while (0)

