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

#include "arc4random.h"
#include "quid.h"

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

/* Construct QUID */
void quid_create(quid_t *uid) {
	cuuid_time_t timestamp;
	unsigned short clockseq;

	get_current_time(&timestamp);
	clockseq = arc4random();

	format_quid(uid, clockseq, timestamp);
}

/*
 * Format QUID from the timestamp, clocksequence, and node ID
 * Structure succeeds version 3
 */
static void format_quid(quid_t *uid, unsigned short clock_seq, cuuid_time_t timestamp) {
	uid->time_low = (unsigned long)(timestamp & 0xffffffff);
	uid->time_mid = (unsigned short)((timestamp >> 32) & 0xffff);

	uid->time_hi_and_version = (unsigned short)((timestamp >> 48) & 0xFFF);
	uid->time_hi_and_version ^= 0x80;
	uid->time_hi_and_version |= 0xa000;

	uid->clock_seq_low = (clock_seq & 0xff);
	uid->clock_seq_hi_and_reserved = (clock_seq & 0x3f00) >> 8;
	uid->clock_seq_hi_and_reserved |= 0x80;

	int i;
	for (i = 0; i < 4; ++i)
		uid->node[i] = arc4random();
	uid->node[4] = (arc4random() & 0xf0);
	uid->node[5] = (arc4random() & 0xff);
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

	for (;;) {
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
	snprintf(s, QUID_LENGTH + 1, "{%.8x-%.4x-%.4x-%.2x%.2x-%.2x%.2x%.2x%.2x%.2x%.2x}"
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

/* Print QUID to short string */
void quidtoshortstr(char *s, quid_t *u) {
	snprintf(s, SHORT_QUID_LENGTH + 1, "{%.2x%.2x%.2x%.2x%.2x%.2x}"
	         , u->node[0]
	         , u->node[1]
	         , u->node[2]
	         , u->node[3]
	         , u->node[4]
	         , u->node[5]);
}

/* Convert string into QUID */
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

/* Determine QUID format or return -1 if invalid */
uint8_t strquid_format(const char *s) {
	size_t ssz = strlen(s);
	int phyp, nhyp = 0;
	if (ssz == QUID_LENGTH) {
		if (s[0] != '{' || s[QUID_LENGTH - 1] != '}')
			return 0;
		if (s[strspn(s, "{}-0123456789abcdefABCDEF")])
			return 0;

		char *pch = strchr(s, '-');
		while (pch != NULL) {
			nhyp++;
			phyp = pch - s;
			if (phyp != 9 && phyp != 14 && phyp != 19 && phyp != 24)
				return 0;
			pch = strchr(pch + 1, '-');
		}
		if (nhyp != 4)
			return 0;

		return 1;
	} else if (ssz == QUID_SHORT_LENGTH) {
		if (s[strspn(s, "-0123456789abcdefABCDEF")])
			return 0;

		char *pch = strchr(s, '-');
		while (pch != NULL) {
			nhyp++;
			phyp = pch - s;
			if (phyp != 8 && phyp != 13 && phyp != 18 && phyp != 23)
				return 0;
			pch = strchr(pch + 1, '-');
		}
		if (nhyp != 4)
			return 0;

		return 2;
	} else {
		return 0;
	}
}
