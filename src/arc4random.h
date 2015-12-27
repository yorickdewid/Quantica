#ifndef ARC4RANDOM_H_INCLUDED
#define ARC4RANDOM_H_INCLUDED

#include <stdint.h>

#ifdef LINUX
void arc4random_stir();
void arc4random_addrandom(uint8_t *dat, int datlen);
uint32_t arc4random();
uint32_t arc4random_uniform(uint32_t range);
#endif // LINUX

#endif // ARC4RANDOM_H_INCLUDED

