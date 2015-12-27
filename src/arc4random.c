#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <fcntl.h>
#include <string.h>

#include <config.h>
#include <common.h>
#include <log.h>

#include "arc4random.h"

#ifdef LINUX
typedef struct {
	uint8_t i;
	uint8_t j;
	uint8_t s[256];
} arc4_stream_t;

#define	RANDOMDEV	"/dev/urandom"

static arc4_stream_t rs;
static int rs_initialized;
static int rs_stired;

static inline uint8_t arc4_getbyte(arc4_stream_t *);
static void arc4_stir(arc4_stream_t *);

static inline void arc4_init(arc4_stream_t *as) {
	for (int n = 0; n < 256; ++n)
		as->s[n] = n;
	as->i = 0;
	as->j = 0;
}

static inline void arc4_addrandom(arc4_stream_t *as, uint8_t *dat, int datlen) {
	uint8_t si;

	as->i--;
	for (int n = 0; n < 256; ++n) {
		as->i = (as->i + 1);
		si = as->s[as->i];
		as->j = (as->j + si + dat[n % datlen]);
		as->s[as->i] = as->s[as->j];
		as->s[as->j] = si;
	}
}

static void arc4_stir(arc4_stream_t *as) {
	int fd;
	struct dat {
		struct timeval tv;
		pid_t pid;
		uint8_t rnd[128 - sizeof(struct timeval) - sizeof(pid_t)];
	};

	struct dat rdat;
	nullify(&rdat, sizeof(struct dat));

	gettimeofday(&rdat.tv, NULL);
	rdat.pid = getpid();
	fd = open(RANDOMDEV, O_RDONLY, 0);
	if (fd >= 0) {
		if (read(fd, rdat.rnd, sizeof(rdat.rnd)) != sizeof(rdat.rnd)) {
			lprint("[erro] Failed to read " RANDOMDEV "\n");
			close(fd);
			return;
		}
	}
	arc4_addrandom(as, (void *)&rdat, sizeof(rdat));

	for (int n = 0; n < 1024; ++n)
		arc4_getbyte(as);
}

static inline uint8_t arc4_getbyte(arc4_stream_t *as) {
	uint8_t si, sj;

	as->i = (as->i + 1);
	si = as->s[as->i];
	as->j = (as->j + si);
	sj = as->s[as->j];
	as->s[as->i] = sj;
	as->s[as->j] = si;

	return (as->s[(si + sj) & 0xff]);
}

static inline uint32_t arc4_getword(arc4_stream_t *as) {
	uint32_t val;

	val = arc4_getbyte(as) << 24;
	val |= arc4_getbyte(as) << 16;
	val |= arc4_getbyte(as) << 8;
	val |= arc4_getbyte(as);

	return val;
}

static void arc4_check_init() {
	if (!rs_initialized) {
		arc4_init(&rs);
		rs_initialized = 1;
	}
}

static void arc4_check_stir() {
	if (!rs_stired) {
		arc4_stir(&rs);
		rs_stired = 1;
	}
}

void arc4random_stir() {
	arc4_check_init();
	arc4_stir(&rs);
}

void arc4random_addrandom(uint8_t *dat, int datlen) {
	arc4_check_init();
	arc4_check_stir();
	arc4_addrandom(&rs, dat, datlen);
}

uint32_t arc4random() {
	uint32_t rnd;

	arc4_check_init();
	arc4_check_stir();
	rnd = arc4_getword(&rs);

	return rnd;
}

uint32_t arc4random_uniform(uint32_t range) {
	uint32_t rnd;

	arc4_check_init();
	arc4_check_stir();
	rnd = arc4_getword(&rs);
	rnd %= range;

	return rnd;
}
#endif // LINUX
