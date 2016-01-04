#ifndef QUID_H_INCLUDED
#define QUID_H_INCLUDED

#include <config.h>
#include <common.h>

#include "marshall.h"

#define QUID_LENGTH 		38
#define SHORT_QUID_LENGTH	14
#define QUID_VERSION		3

/*
 * Identifier structure
 */
struct quid {
	unsigned long time_low;						/* Time lover half */
	unsigned short time_mid;					/* Time middle half */
	unsigned short time_hi_and_version;			/* Time upper half and structure version */
	unsigned char clock_seq_hi_and_reserved;	/* Clock sequence */
	unsigned char clock_seq_low;				/* Clock sequence lower half */
	unsigned char node[6];						/* Node allocation, filled with random memory data */
} __attribute__((packed));

/*
 * Identifier structure
 */
struct quid_short {
	unsigned char node[6];						/* Node allocation, filled with random memory data */
} __attribute__((packed));

typedef unsigned long long int cuuid_time_t;
typedef struct quid quid_t;
typedef struct quid_short quid_short_t;

/*
 * Create new QUID
 */
void quid_create(quid_t *);
void quid_short_create(quid_short_t *uid);

marshall_t *quid_decode(quid_t *uid);

void quid_shorttostr(char *s, quid_short_t *u);

/*
 * Compare to QUID keys
 */
int quidcmp(const quid_t *a, const quid_t *b);
int quid_shortcmp(const quid_short_t *a, const quid_short_t *b);

/*
 * Convert QUID key to string
 */
void quidtostr(char *s, quid_t *u);

/*
 * Convert string to QUID key
 */
void strtoquid(const char *s, quid_t *u);

char strquid_format(const char *s);

#endif // QUID_H_INCLUDED
