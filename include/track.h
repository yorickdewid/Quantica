#ifndef TRACK_H_INCLUDED
#define TRACK_H_INCLUDED

#include <stdint.h>

#define NO_ERROR 0x0
#define QUID_NOTFOUND 0x1
#define QUID_EXIST 0x2
#define VAL_EMPTY 0x4
#define DB_EMPTY 0x8
#define FILE_IDXIO 0xa
#define FILE_DBIO 0xb

/*
 * Trace error through global structure
 */
struct etrace {
	uint8_t code;
};

#endif // TRACK_H_INCLUDED
