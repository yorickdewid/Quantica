#include <stdver.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <ctype.h>
#include <time.h>

#include <config.h>
#include <common.h>
#include <error.h>
#include "arc4random.h"
#include "zmalloc.h"
#include "marshall.h"
#include "time.h"
#include "quid.h"

#define UIDS_PER_TICK	1024			/* Generate identifiers per tick interval */
#define EPOCH_DIFF		11644473600LL	/* Conversion needed for EPOCH to UTC */
#define RND_SEED_CYCLE	4096			/* Generate new random seed after interval */
#define QUID_VERSION_3	0xa000
#define QUID_SIGNATURE	0x80

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

/* Construct short QUID */
void quid_short_create(quid_short_t *uid) {
	for (int i = 0; i < 4; ++i) {
		uid->node[i] = arc4random();
	}
	uid->node[4] = (arc4random() & 0xf0);
	uid->node[5] = (arc4random() & 0xff);
}

marshall_t * quid_decode(quid_t *uid) {
	marshall_t *marshall = (marshall_t *)tree_zcalloc(1, sizeof(marshall_t), NULL);
	marshall->child = (marshall_t **)tree_zcalloc(4, sizeof(marshall_t *), marshall);
	marshall->type = MTYPE_OBJECT;
	marshall->size = 4;

	union {
		struct {
			unsigned int low;
			unsigned short middle;
			unsigned short high;
		} p;
		cuuid_time_t timestamp;
	} cv;

	cv.p.low = uid->time_low;
	cv.p.middle = uid->time_mid;
	cv.p.high = (uid->time_hi_and_version - QUID_VERSION_3) ^ QUID_SIGNATURE;
	cuuid_time_t ts = (cv.timestamp / 10000000LL) - EPOCH_DIFF;

	/* Timestamps outside this range are invalid for sure */
	if (ts < 1000000000 || ts > 2000000000) {
		error_throw("f0b867c41006", "Key malformed or invalid");
		marshall_free(marshall);
		return NULL;
	}

	if (!(uid->time_hi_and_version & QUID_VERSION_3)) {
		error_throw("f0b867c41006", "Key malformed or invalid");
		marshall_free(marshall);
		return NULL;
	}

	if (!(uid->clock_seq_hi_and_reserved & QUID_SIGNATURE)) {
		error_throw("f0b867c41006", "Key malformed or invalid");
		marshall_free(marshall);
		return NULL;
	}

	char buf[36];
	nullify(buf, 36);
	unixtostrf(buf, 36, (cv.timestamp / 10000000LL) - EPOCH_DIFF, ISO_8601_FORMAT);

	marshall->child[0] = tree_zcalloc(1, sizeof(marshall_t), marshall);
	marshall->child[0]->type = MTYPE_STRING;
	marshall->child[0]->name = "created";
	marshall->child[0]->name_len = 7;
	marshall->child[0]->data = tree_zstrdup(buf, marshall);
	marshall->child[0]->data_len = 36;

	marshall->child[1] = tree_zcalloc(1, sizeof(marshall_t), marshall);
	marshall->child[1]->type = MTYPE_INT;
	marshall->child[1]->name = "version";
	marshall->child[1]->name_len = 7;
	marshall->child[1]->data = tree_zstrdup(itoa(QUID_VERSION), marshall);
	marshall->child[1]->data_len = 1;

	marshall->child[2] = tree_zcalloc(1, sizeof(marshall_t), marshall);
	marshall->child[2]->type = MTYPE_TRUE;
	marshall->child[2]->name = "valid";
	marshall->child[2]->name_len = 5;

	char *mark = itoa(uid->node[4]);
	marshall->child[3] = tree_zcalloc(1, sizeof(marshall_t), marshall);
	marshall->child[3]->type = MTYPE_INT;
	marshall->child[3]->name = "checkpoint_mark";
	marshall->child[3]->name_len = 15;
	marshall->child[3]->data = tree_zstrdup(mark, marshall);
	marshall->child[3]->data_len = strlen(mark);

	return marshall;
}

/*
 * Format QUID from the timestamp, clocksequence, and node ID
 * Structure succeeds version 3
 */
static void format_quid(quid_t *uid, unsigned short clock_seq, cuuid_time_t timestamp) {
	uid->time_low = (unsigned long)(timestamp & 0xffffffff);
	uid->time_mid = (unsigned short)((timestamp >> 32) & 0xffff);

	uid->time_hi_and_version = (unsigned short)((timestamp >> 48) & 0xfff);
	uid->time_hi_and_version ^= QUID_SIGNATURE;
	uid->time_hi_and_version |= QUID_VERSION_3;

	uid->clock_seq_low = (clock_seq & 0xff);
	uid->clock_seq_hi_and_reserved = (clock_seq & 0x3f00) >> 8;
	uid->clock_seq_hi_and_reserved |= QUID_SIGNATURE;

	for (int i = 0; i < 4; ++i) {
		uid->node[i] = arc4random();
	}
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

/* Compare two identifiers */
int quid_shortcmp(const quid_short_t *a, const quid_short_t *b) {
	return memcmp(a, b, sizeof(quid_short_t));
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
void quid_shorttostr(char *s, quid_short_t *u) {
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
	} else if (ssz == (QUID_LENGTH - 2)) {
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

/*
 * Determine QUID format
 */
char strquid_format(const char *s) {
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
	} else if (ssz == (QUID_LENGTH - 2)) {
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
