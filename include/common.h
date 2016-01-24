#ifndef COMMON_H_INCLUDED
#define COMMON_H_INCLUDED

#include <stdint.h>
#include <config.h>
#include <unistd.h>

#define PNULL	(void **)0

#define O_BINARY 0

#define TRUE    1
#define FALSE   0

#define NOLOCK 0x0
#define LOCK 0x1

#define RSIZE(e) sizeof(e)/sizeof(e[0])
#define unused(v) (void)(v)

#define _IN_
#define _OUT_

#ifdef LINUX
size_t strlcpy(char *dst, const char *src, size_t siz);
size_t strlcat(char *dst, const char *src, size_t siz);
#endif // LINUX

#if defined(__mc68000__) || defined (__sparc__) || defined (__PPC__) \
    || (defined(__mips__) && (defined(MIPSEB) || defined (__MIPSEB__)) ) \
    || defined(__hpux__)  /* should be replaced by the macro for the PA */
#define BIG_ENDIAN_HOST 1
#else
#define LITTLE_ENDIAN_HOST 1
#endif

#define zassert(e)  \
	((void) ((e) ? (void)0 : __zassert (#e, __FILE__, __LINE__)))

#define __zassert(e, file, line) \
	((void)printf("%s:%u: failed assertion `%s'\n", file, line, e), abort())

#define nullify(p, sz) \
	memset(p, 0, sz)

typedef _Bool bool;

char from_hex(char ch);
int8_t strisbool(char *str);
char *strtolower(char *str);
char *strtoupper(char *str);
bool strisdigit(char *str);
bool strisalpha(char *str);
bool strisualpha(char *str);
char *strdtrim(char *str);
char *strtoken(char *s, const char *delim);
bool strismatch(const char *str, const char *tok);
int strccnt(const char *str, char c);
char *str_bool(bool b);
char *str_null();
int antoi(const char *str, size_t num);
char *itoa(long i);
char *strdup(const char *str);
char *strndup(const char *str, size_t n);
char *stresc(char *src, size_t *_len);
char *strsep(char ** stringp, const char *delim);
int zprintf(const char *fmt, ...);

#ifdef DEBUG
void hexdump(char *desc, void *addr, int len);
#endif

int file_access_exists(const char *path);
int file_exists(const char *path);
size_t file_size(int fd);
char *get_version_string();
long get_version();
size_t page_align(size_t val);
char *unit_bytes(double size, char *buf);

#endif // COMMON_H_INCLUDED
