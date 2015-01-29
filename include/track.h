#ifndef TRACK_H_INCLUDED
#define TRACK_H_INCLUDED

#include <stdint.h>

#define QUID_NOTFOUND 0x1
#define QUID_EXIST 0x2
#define VAL_EMPTY 0x4

/*
 * Trace error through global structure
 */
struct etrace {
	uint8_t code;
};

#endif // TRACK_H_INCLUDED
