#include <common.h>
#include <config.h>

#include "jenhash.h"

#define hashsize(n) (1U << (n))
#define hashmask(n) (hashsize(n) - 1)

static const unsigned int initval = 98634924;

#define mix(a,b,c) {				\
	a -= b; a -= c; a ^= (c >> 13);	\
	b -= c; b -= a; b ^= (a << 8);	\
	c -= a; c -= b; c ^= (b >> 13);	\
	a -= b; a -= c; a ^= (c >> 12);	\
	b -= c; b -= a; b ^= (a << 16);	\
	c -= a; c -= b; c ^= (b >> 5);	\
	a -= b; a -= c; a ^= (c >> 3);	\
	b -= c; b -= a; b ^= (a << 10);	\
	c -= a; c -= b; c ^= (b >> 15);	\
}

unsigned int jen_hash(unsigned char *k, size_t length) {
	unsigned int a, b;
	unsigned int c = initval;
	unsigned int len = length;

	a = b = 0x9e3779b9;

	while (len >= 12) {
		a += (k[0] + ((unsigned int)k[1] << 8) + ((unsigned int)k[2] << 16) + ((unsigned int)k[3] << 24));
		b += (k[4] + ((unsigned int)k[5] << 8) + ((unsigned int)k[6] << 16) + ((unsigned int)k[7] << 24));
		c += (k[8] + ((unsigned int)k[9] << 8) + ((unsigned int)k[10] << 16) + ((unsigned int)k[11] << 24));

		mix(a, b, c);

		k += 12;
		len -= 12;
	}

	c += length;

	switch (len) {
		case 11: c += ((unsigned int)k[10] << 24);
		case 10: c += ((unsigned int)k[9] << 16);
		case 9: c += ((unsigned int)k[8] << 8);
		/* First byte of c reserved for length */
		case 8: b += ((unsigned int)k[7] << 24);
		case 7: b += ((unsigned int)k[6] << 16);
		case 6: b += ((unsigned int)k[5] << 8);
		case 5: b += k[4];
		case 4: a += ((unsigned int)k[3] << 24);
		case 3: a += ((unsigned int)k[2] << 16);
		case 2: a += ((unsigned int)k[1] << 8);
		case 1: a += k[0];
		default: break;
	}

	mix(a, b, c);

	return c;
}
