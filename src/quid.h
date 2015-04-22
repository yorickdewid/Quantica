#ifndef QUID_H_INCLUDED
#define QUID_H_INCLUDED

#define UIDS_PER_TICK 1024			/* Generate identifiers per tick interval */
#define EPOCH_DIFF 11644473600LL	/* Conversion needed for EPOCH to UTC */
#define RND_SEED_CYCLE 4096			/* Generate new random seed after interval */
#define QUID_LENGTH 38
#define QUID_SHORT_LENGTH 36

/*
 * Identifier structure
 */
struct quid {
	unsigned long time_low;				/* Time lover half */
	unsigned short time_mid;			/* Time middle half */
	unsigned short time_hi_and_version;		/* Time upper half and structure version */
	unsigned char clock_seq_hi_and_reserved;	/* Clock sequence */
	unsigned char clock_seq_low;			/* Clock sequence lower half */
	unsigned char node[6];				/* Node allocation, filled with random memory data */
} __attribute__((packed));

typedef unsigned long long int cuuid_time_t;
typedef struct quid quid_t;

/*
 * Create new QUID
 */
void quid_create(quid_t *);

/*
 * Compare to QUID keys
 */
int quidcmp(const quid_t *a, const quid_t *b);

/*
 * Convert QUID key to string
 */
void quidtostr(char *s, quid_t *u);

/*
 * Convert string to QUID key
 */
void strtoquid(const char *s, quid_t *u);

int8_t strquid_format(const char *s);

#endif // QUID_H_INCLUDED
