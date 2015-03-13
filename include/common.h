#ifndef UTIL_H_INCLUDED
#define UTIL_H_INCLUDED

#include <config.h>

#define TRUE    1
#define FALSE   0

#define NOLOCK 0x0
#define LOCK 0x1

char from_hex(char ch);
char *strtolower(char *str);

#endif // UTIL_H_INCLUDED
