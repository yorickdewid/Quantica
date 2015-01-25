#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <unistd.h>
#include <sys/time.h>
#include <ctype.h>

#include "quid.h"

static int rnd_seed = RND_SEED_CYCLE;

static void format_quid(struct quid *, unsigned short, cuuid_time_t);
static void get_current_time(cuuid_time_t *);

/* Retrieve system time */
void get_system_time(cuuid_time_t *uid_time) {
	struct timeval tv;
	unsigned long long int result = EPOCH_DIFF;
	gettimeofday(&tv, NULL);
	result += tv.tv_sec;
	result *= 10000000LL;
	result += tv.tv_usec * 10;
	*uid_time = result;
}

/* Get hardware tick count */
double get_tick_count(void) {
	struct timespec now;

	if (clock_gettime(CLOCK_MONOTONIC, &now))
		return 0;

	return now.tv_sec * 1000.0 + now.tv_nsec / 1000000.0;
}

/* Create true random as prescribed by the IEEE */
static unsigned short true_random(void) {
	static int rnd_seed_count = 0;
	cuuid_time_t time_now;

	if (!rnd_seed_count) {
		get_system_time(&time_now);
		time_now = time_now / UIDS_PER_TICK;
		srand((unsigned int)(((time_now >> 32) ^ time_now) & 0xffffffff));
	}

	if(rnd_seed_count == rnd_seed)
		rnd_seed_count = 0;
	else
		rnd_seed_count++;

	return (rand()+get_tick_count());
}

/* Construct QUID */
void quid_create(struct quid *uid) {
	cuuid_time_t timestamp;
	unsigned short clockseq;

	get_current_time(&timestamp);
	clockseq = true_random();

	format_quid(uid, clockseq, timestamp);
}

/*
 * Format QUID from the timestamp, clocksequence, and node ID
 * Structure succeeds version 3
 */
void format_quid(struct quid* uid, unsigned short clock_seq, cuuid_time_t timestamp){
	int i;

	uid->time_low = (unsigned long)(timestamp & 0xffffffff);
	uid->time_mid = (unsigned short)((timestamp >> 32) & 0xffff);

	uid->time_hi_and_version = (unsigned short)((timestamp >> 48) & 0xFFF);
	uid->time_hi_and_version ^= 0x80;
	uid->time_hi_and_version |= 0xa000;

	uid->clock_seq_low = (clock_seq & 0xff);
	uid->clock_seq_hi_and_reserved = (clock_seq & 0x3f00) >> 8;
	uid->clock_seq_hi_and_reserved |= 0x80;

	for(i=0; i<4; ++i)
		uid->node[i] = true_random();
	uid->node[4] = (true_random() & 0xf0);
	uid->node[5] = (true_random() & 0xff);
}

/* Get current time including cpu clock */
void get_current_time(cuuid_time_t *timestamp) {
	static int inited = 0;
	static cuuid_time_t time_last;
	static unsigned short ids_this_tick;
	cuuid_time_t time_now;

	if (!inited) {
		get_system_time(&time_now);
		ids_this_tick = UIDS_PER_TICK;
		inited = 1;
	}

	for(;;) {
		get_system_time(&time_now);

		if (time_last != time_now) {
			ids_this_tick = 0;
			time_last = time_now;
			break;
		}

		if (ids_this_tick < UIDS_PER_TICK) {
			ids_this_tick++;
			break;
		}
	}

	*timestamp = time_now + ids_this_tick;
}

