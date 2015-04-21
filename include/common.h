#ifndef UTIL_H_INCLUDED
#define UTIL_H_INCLUDED

#include <stdint.h>
#include <config.h>
#include <unistd.h>

#ifdef __CHECKER__
#define FORCE	__attribute__((force))
#else
#define FORCE
#endif

#ifdef __CHECKER__
#define BITWISE	__attribute__((bitwise))
#else
#define BITWISE
#endif

#define O_BINARY 0

#define TRUE    1
#define FALSE   0

#define NOLOCK 0x0
#define LOCK 0x1

#define RSIZE(e) sizeof(e)/sizeof(e[0])
#define unused(v) (void)(v)

#ifdef LINUX
size_t strlcpy(char *dst, const char *src, size_t siz);
size_t strlcat(char *dst, const char *src, size_t siz);
#endif // LINUX

#ifdef OBSD
#define RANDOM() arc4random()
#else
#define RANDOM() rand()
#endif // OBSD

typedef uint8_t bool;

char from_hex(char ch);
char *strtolower(char *str);
char *strtoupper(char *str);
bool strisdigit(char *str);
char *itoa(long i);
char *strdup(const char *str);

typedef uint16_t BITWISE __be16; /* big endian, 16 bits */
typedef uint32_t BITWISE __be32; /* big endian, 32 bits */
typedef uint64_t BITWISE __be64; /* big endian, 64 bits */

__be32 to_be32(uint32_t x);
__be16 to_be16(uint16_t x);
__be64 to_be64(uint64_t x);

uint32_t from_be32(__be32 x);
uint16_t from_be16(__be16 x);
uint64_t from_be64(__be64 x);

int file_exists(const char *path);
char *get_version_string();
long get_version();

#endif // UTIL_H_INCLUDED
