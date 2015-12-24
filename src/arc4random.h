#ifndef ARC4RANDOM_H_INCLUDED
#define ARC4RANDOM_H_INCLUDED

#include <stdint.h>

void arc4random_stir();
void arc4random_addrandom(uint8_t *dat, int datlen);
uint32_t arc4random();
int arc4random_range(int range);

#endif // ARC4RANDOM_H_INCLUDED

