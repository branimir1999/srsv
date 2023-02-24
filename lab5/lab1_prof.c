#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>

#include "lab1.h"

#define K 2

/* simulated inputs */
static struct input input[] = 
{
	{ .id =  1, .period = {1,0},  .first_run = {0,0}, 0},
	{ .id =  2, .period = {1,0},  .first_run = {0,100000000}},
	{ .id =  3, .period = {1,0},  .first_run = {0,300000000}},
	{ .id =  4, .period = {1,0},  .first_run = {0,500000000}},
	{ .id =  5, .period = {1,0},  .first_run = {0,700000000}},
	{ .id =  6, .period = {1,0},  .first_run = {0,900000000}},
	{ .id =  7, .period = {2,0},  .first_run = {0,400000000}},
	{ .id =  8, .period = {2,0},  .first_run = {0,800000000}},
	{ .id =  9, .period = {2,0},  .first_run = {1,400000000}},
	{ .id = 10, .period = {2,0},  .first_run = {1,800000000}},
	{ .id = 11, .period = {5,0},  .first_run = {0,200000000}},
	{ .id = 12, .period = {5,0},  .first_run = {2,200000000}},
	{ .id = 13, .period = {5,0},  .first_run = {4,200000000}},
	{ .id = 14, .period = {10,0}, .first_run = {2,600000000}},
	{ .id = 15, .period = {10,0}, .first_run = {4,600000000}},
	{ .id = 16, .period = {10,0}, .first_run = {6,600000000}},
	{ .id = 17, .period = {20,0}, .first_run = {8,600000000}},
	{ .id = 18, .period = {20,0}, .first_run = {18,600000000}},
#if 1
	/* lets get some overruns */
	{ .id = 21, .period = {1,0}, .first_run = {0,0}, 0},
	{ .id = 22, .period = {1,0}, .first_run = {0,100000000}},
	{ .id = 23, .period = {1,0}, .first_run = {0,300000000}},
	{ .id = 24, .period = {1,0}, .first_run = {0,500000000}},
	{ .id = 25, .period = {1,0}, .first_run = {0,700000000}},
	{ .id = 26, .period = {1,0}, .first_run = {0,900000000}},
	{ .id = 27, .period = {1,0}, .first_run = {0,400000000}},
	{ .id = 28, .period = {1,0}, .first_run = {0,800000000}},
	{ .id = 29, .period = {1,0}, .first_run = {1,400000000}},
	{ .id = 30, .period = {1,0}, .first_run = {1,800000000}},
	{ .id = 31, .period = {1,0}, .first_run = {0,200000000}},
	{ .id = 32, .period = {1,0}, .first_run = {2,200000000}},
	{ .id = 33, .period = {1,0}, .first_run = {4,200000000}},
	{ .id = 34, .period = {1,0}, .first_run = {2,600000000}},
	{ .id = 35, .period = {1,0}, .first_run = {4,600000000}},
	{ .id = 36, .period = {1,0}, .first_run = {6,600000000}},
	{ .id = 37, .period = {1,0}, .first_run = {8,600000000}},
	{ .id = 38, .period = {1,0}, .first_run = {18,600000000}},
#endif
};
#define INPUTS (sizeof(input)/sizeof(struct input))

static struct stat stat;	/* overall stat */
static struct timespec t0;	/* time of the start of simulation */
static pthread_mutex_t print_lock;
static int done = 0;
static pthread_t tid[INPUTS+1];

static void init();
static void *controller (void *x);
static void *input_simulator (void *x);
static void collect_and_print_stat();

int main ()
{
	unsigned long i;

	init();

	PRINT("Initialization completed\n");

	if (pthread_create(&tid[0], NULL, controller, (void *) &input)) {
		perror("pthread_create failed");
		exit(1);
	}
	for (i = 1; i <= INPUTS; i++) {
		if (pthread_create(&tid[i], NULL, input_simulator, (void *) &input[i-1])) {
			perror("pthread_create failed");
			exit(1);
		}
	}

	PRINT("Threads created\n");

	for (i = 0; i <= INPUTS; i++)
		pthread_join(tid[i], NULL);

	collect_and_print_stat();

	PRINT("Simulation completed\n");

	return 0;
}

static long simulate_processing_time()
{
	struct timespec proc_time;
	
	long processing = random() % 100;
	if (processing < 20)
		processing = 30;
	else if (processing < 20+50)
		processing = 50;
	else if (processing < 20+50+20)
		processing = 80;
	else
		processing = 120;

	proc_time.tv_sec = 0;
	proc_time.tv_nsec = processing * 1000000;
	clock_nanosleep(CLOCK_MONOTONIC, 0, &proc_time, NULL);

	return processing;
}

static void *controller (void *x)
{
	struct input *inp = x;
	unsigned long i;
	struct timespec reply_time;
	long proc_time;

	while (!done) {
		for (i = 0; !done && i < INPUTS; i++) {
			if (inp[i].state != inp[i].last_state) {
				/* action required */
				proc_time = simulate_processing_time(&proc_time);
				PRINT("[CNT] Input %d processing completed; consumed %ld msec of proc time\n",
					inp[i].id, proc_time);

				/* set reply */
				inp[i].reply = inp[i].state;
				clock_gettime(CLOCK_MONOTONIC, &inp[i].last_replay);
				inp[i].last_state = inp[i].state;

				/* "as seen by controller" statistic */
				inp[i].stat.cnt_runs++;
				reply_time = inp[i].last_replay;
				TIMESPEC_SUB(reply_time, inp[i].last_change);
				TIMESPEC_ADD(inp[i].stat.cnt_sum_reply_times, reply_time);

				if (TIMESPEC_GT(reply_time, inp[i].stat.cnt_max_reply_time))
					inp[i].stat.cnt_max_reply_time = reply_time;

				if (TIMESPEC_GT(reply_time, inp[i].period)) {
					inp[i].stat.cnt_overruns++;
					TIMESPEC_SUB(reply_time, inp[i].period);
					PRINT("[CNT] Input %d: deadline overrun by %ld:%06ld\n",
						inp[i].id, reply_time.tv_sec, reply_time.tv_nsec);
				}
			}
		}
	}

	return NULL;
}

static void *input_simulator (void *x)
{
	struct input *inp = x;
	struct timespec wakeup = inp->first_run, msec = {0, 1000000}, reply_time, delay;

	while (!done) {
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &wakeup, NULL);

		inp->reply = 0;
		inp->state = 100 + random() % 1000;
		clock_gettime(CLOCK_MONOTONIC, &inp->last_change);
		PRINT("[SIM] Input %d state changed to %d\n", inp->id, inp->state);

		while (!done && inp->reply != inp->state)
			clock_nanosleep(CLOCK_MONOTONIC, 0, &msec, NULL);
		if (done)
			break;
		
		inp->stat.runs++;

		clock_gettime(CLOCK_MONOTONIC, &reply_time);
		TIMESPEC_SUB(reply_time, inp->last_change);
		PRINT("[SIM] Input %d reply got after %ld:%06ld\n", inp->id, 
			reply_time.tv_sec, reply_time.tv_nsec);
		TIMESPEC_ADD(inp->stat.sum_reply_times, reply_time);

		if (TIMESPEC_GT(reply_time, inp->stat.max_reply_time))
			inp->stat.max_reply_time = reply_time;

		if (TIMESPEC_GT(reply_time, inp->period)) {
			inp->stat.overruns++;
			TIMESPEC_SUB(reply_time, inp->period);
			PRINT("[SIM] Input %d deadline overrun by %ld:%06ld\n", inp->id,
				reply_time.tv_sec, reply_time.tv_nsec);
		}

		double rnd = 1 + drand48() * (K-1);
		double nsec = rnd * (inp->period.tv_sec * 1e9 + inp->period.tv_nsec);
		delay.tv_sec = nsec / 1000000000;
		delay.tv_nsec = nsec - delay.tv_sec * 1e9;

		PRINT("[SIM] Input %d sleeping for %ld:%06ld (%ld:%06ld)\n", inp->id, 
			delay.tv_sec, delay.tv_nsec, inp->period.tv_sec, inp->period.tv_nsec);
		TIMESPEC_ADD(wakeup, delay);

		/* on bigger overrun "reset" next activation time */
		clock_gettime(CLOCK_MONOTONIC, &delay);
		if (TIMESPEC_GT(delay, wakeup))
			wakeup = delay;
	}


	return NULL;
}

static void interrupt(int sig) 
{
	unsigned long i;

    pthread_mutex_lock(&print_lock);
	if (done) {
	    pthread_mutex_unlock(&print_lock);
		return;
	}

	done = 1;
    pthread_mutex_unlock(&print_lock);

	PRINT("Signal %d caught, starting termination\n", sig);

	for (i = 0; i <= INPUTS; i++)
		if (pthread_self() != tid[i])
			pthread_kill(tid[i], SIGTERM);
}

static void init() 
{
	unsigned long i;
	struct timespec zero = {0, 0};

	clock_gettime(CLOCK_MONOTONIC, &t0);
	for (i = 0; i < INPUTS; i++) {
		TIMESPEC_ADD(input[i].first_run, t0);
		/* rest of the "reset" code might be unecessary
		   since static allocation is used
		   but just in case (e.g. if malloc is to be used) */
		input[i].state = 0;
		input[i].reply = 0;
		input[i].last_state = 0;
		input[i].last_change = zero;
		input[i].last_replay = zero;

		input[i].stat.runs = 0;
		input[i].stat.overruns = 0;
		input[i].stat.sum_reply_times = zero;
		input[i].stat.max_reply_time = zero;

		input[i].stat.cnt_runs = 0;
		input[i].stat.cnt_overruns = 0;
		input[i].stat.cnt_sum_reply_times = zero;
		input[i].stat.cnt_max_reply_time = zero;
	}

	stat.runs = 0;
	stat.overruns = 0;
	stat.sum_reply_times = zero;
	stat.max_reply_time = zero;

	stat.cnt_runs = 0;
	stat.cnt_overruns = 0;
	stat.cnt_sum_reply_times = zero;
	stat.cnt_max_reply_time = zero;

	srandom(t0.tv_nsec);
	srand48(t0.tv_nsec);

	struct sigaction act;
	act.sa_handler = interrupt;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	if (sigaction(SIGINT, &act, NULL) || sigaction(SIGTERM, &act, NULL)) {
		perror("sigaction failed");
		exit(1);
	}
}

static void collect_and_print_stat() 
{
	unsigned long i;
	double avg;

	for (i = 0; i < INPUTS; i++) {
		PRINT("\n");
		PRINT("Input (sim) %d\n", input[i].id);
		PRINT("\truns: %lu\n", input[i].stat.runs);
		PRINT("\toverruns: %lu\n", input[i].stat.overruns);
		PRINT("\tmax reply time: %ld:%06ld\n", input[i].stat.max_reply_time.tv_sec, 
			input[i].stat.max_reply_time.tv_nsec/1000);
		avg = input[i].stat.sum_reply_times.tv_nsec + 1e9 * input[i].stat.sum_reply_times.tv_sec;
		if (input[i].stat.runs)
			avg = avg / input[i].stat.runs / 1e9;
		PRINT("\tavg reply time: %f\n", avg);

		stat.runs += input[i].stat.runs;
		stat.overruns += input[i].stat.overruns;
		TIMESPEC_ADD(stat.sum_reply_times, input[i].stat.sum_reply_times);
		if (TIMESPEC_GT(input[i].stat.max_reply_time, stat.max_reply_time))
			stat.max_reply_time = input[i].stat.max_reply_time;

		PRINT("Input (cnt) %d\n", input[i].id);
		PRINT("\truns: %lu\n", input[i].stat.cnt_runs);
		PRINT("\toverruns: %lu\n", input[i].stat.cnt_overruns);
		PRINT("\tmax reply time: %ld:%06ld\n", input[i].stat.cnt_max_reply_time.tv_sec, 
			input[i].stat.cnt_max_reply_time.tv_nsec/1000);
		avg = input[i].stat.cnt_sum_reply_times.tv_nsec + 1e9 * input[i].stat.cnt_sum_reply_times.tv_sec;
		if (input[i].stat.cnt_runs)
			avg = avg / input[i].stat.cnt_runs / 1e9;
		PRINT("\tavg reply time: %.6f\n", avg);

		stat.cnt_runs += input[i].stat.cnt_runs;
		stat.cnt_overruns += input[i].stat.cnt_overruns;
		TIMESPEC_ADD(stat.cnt_sum_reply_times, input[i].stat.cnt_sum_reply_times);
		if (TIMESPEC_GT(input[i].stat.cnt_max_reply_time, stat.cnt_max_reply_time))
			stat.cnt_max_reply_time = input[i].stat.cnt_max_reply_time;
	}

	PRINT("\n");
	PRINT("Overall stats (sim)\n");
	PRINT("\truns: %lu\n", stat.runs);
	PRINT("\toverruns: %lu\n", stat.overruns);
	PRINT("\tmax reply time: %ld:%06ld\n", stat.max_reply_time.tv_sec, 
		stat.max_reply_time.tv_nsec/1000);
	avg = stat.sum_reply_times.tv_nsec + 1e9 * stat.sum_reply_times.tv_sec;
	if (stat.runs)
		avg = avg / stat.runs / 1e9;
	PRINT("\tavg reply time: %.6f\n", avg);

	PRINT("Overall stats (cnt)\n");
	PRINT("\truns: %lu\n", stat.cnt_runs);
	PRINT("\toverruns: %lu\n", stat.cnt_overruns);
	PRINT("\tmax reply time: %ld:%06ld\n", stat.cnt_max_reply_time.tv_sec, 
		stat.cnt_max_reply_time.tv_nsec/1000);
	avg = stat.cnt_sum_reply_times.tv_nsec + 1e9 * stat.cnt_sum_reply_times.tv_sec;
	if (stat.cnt_runs)
		avg = avg / stat.cnt_runs /1e9;
	PRINT("\tavg reply time: %.6f\n", avg);
}

