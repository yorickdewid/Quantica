#ifdef LINUX
#if __STDC_VERSION__ >= 199901L
#define _XOPEN_SOURCE 700
#else
#define _XOPEN_SOURCE 500
#endif /* __STDC_VERSION__ */
#endif // LINUX

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <ctype.h>
#include <time.h>

#include <config.h>
#include <common.h>

#include "quid.h"

static int rnd_seed = RND_SEED_CYCLE;

static void format_quid(quid_t *, unsigned short, cuuid_time_t);
static void get_current_time(cuuid_time_t *);

/* Retrieve system time */
void get_system_time(cuuid_time_t *uid_time) {
	struct timeval tv;
	cuuid_time_t result = EPOCH_DIFF;
	gettimeofday(&tv, NULL);
	result += tv.tv_sec;
	result *= 10000000LL;
	result += tv.tv_usec * 10;
	*uid_time = result;
}

/* Get hardware tick count */
double get_tick_count() {
	struct timespec now;
	if (clock_gettime(CLOCK_MONOTONIC, &now))
		return 0;

	return now.tv_sec * 1000.0 + now.tv_nsec / 1000000.0;
}

/* Create true random as prescribed by the IEEE */
static unsigned short true_random() {
	static int rnd_seed_count = 0;

#ifndef OBSD
	cuuid_time_t time_now;
	if (!rnd_seed_count) {
		get_system_time(&time_now);
		time_now = time_now / UIDS_PER_TICK;
		srand((unsigned int)(((time_now >> 32) ^ time_now) & 0xffffffff));
	}
#endif // OBSD

	if(rnd_seed_count == rnd_seed) {
		rnd_seed_count = 0;
	} else {
		rnd_seed_count++;
	}

	return (RANDOM()+get_tick_count());
}

/* Construct QUID */
void quid_create(quid_t *uid) {
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
static void format_quid(quid_t *uid, unsigned short clock_seq, cuuid_time_t timestamp){
	uid->time_low = (unsigned long)(timestamp & 0xffffffff);
	uid->time_mid = (unsigned short)((timestamp >> 32) & 0xffff);

	uid->time_hi_and_version = (unsigned short)((timestamp >> 48) & 0xFFF);
	uid->time_hi_and_version ^= 0x80;
	uid->time_hi_and_version |= 0xa000;

	uid->clock_seq_low = (clock_seq & 0xff);
	uid->clock_seq_hi_and_reserved = (clock_seq & 0x3f00) >> 8;
	uid->clock_seq_hi_and_reserved |= 0x80;

	int i;
	for(i=0; i<4; ++i)
		uid->node[i] = true_random();
	uid->node[4] = (true_random() & 0xf0);
	uid->node[5] = (true_random() & 0xff);
}

/* Get current time including cpu clock */
static void get_current_time(cuuid_time_t *timestamp) {
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

/* Compare two identifiers */
int quidcmp(const quid_t *a, const quid_t *b) {
	return memcmp(a, b, sizeof(quid_t));
}

/* Print QUID to string */
void quidtostr(char *s, quid_t *u) {
	snprintf(s, 39, "{%.8x-%.4x-%.4x-%.2x%.2x-%.2x%.2x%.2x%.2x%.2x%.2x}"
			, (unsigned int)u->time_low
			, u->time_mid
			, u->time_hi_and_version
			, u->clock_seq_hi_and_reserved
			, u->clock_seq_low
			, u->node[0]
			, u->node[1]
			, u->node[2]
			, u->node[3]
			, u->node[4]
			, u->node[5]);
}

void strtoquid(const char *s, quid_t *u) {
	size_t ssz = strlen(s);
	if (ssz == QUID_LENGTH) {
		sscanf(s, "{%8lx-%4hx-%4hx-%2hx%2hx-%2x%2x%2x%2x%2x%2x}"
				, &u->time_low
				, &u->time_mid
				, &u->time_hi_and_version
				, (unsigned short int *)&u->clock_seq_hi_and_reserved
				, (unsigned short int *)&u->clock_seq_low
				, (unsigned int *)&u->node[0]
				, (unsigned int *)&u->node[1]
				, (unsigned int *)&u->node[2]
				, (unsigned int *)&u->node[3]
				, (unsigned int *)&u->node[4]
				, (unsigned int *)&u->node[5]);
	} else if (ssz == QUID_SHORT_LENGTH) {
		sscanf(s, "%8lx-%4hx-%4hx-%2hx%2hx-%2x%2x%2x%2x%2x%2x"
				, &u->time_low
				, &u->time_mid
				, &u->time_hi_and_version
				, (unsigned short int *)&u->clock_seq_hi_and_reserved
				, (unsigned short int *)&u->clock_seq_low
				, (unsigned int *)&u->node[0]
				, (unsigned int *)&u->node[1]
				, (unsigned int *)&u->node[2]
				, (unsigned int *)&u->node[3]
				, (unsigned int *)&u->node[4]
				, (unsigned int *)&u->node[5]);

	}
}
