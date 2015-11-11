#ifndef ZMALLOC_H_INCLUDED
#define ZMALLOC_H_INCLUDED

#include <stdlib.h>

#define zmalloc(sz) malloc(sz)
#define zcalloc(n, sz) calloc(n, sz)
#define zstrdup(str) strdup(str)
#define zstrndup(str, sz) strndup(str, sz)
#define zfree(sz) free(sz)

void *tree_zmalloc(size_t size, void *parent);
void *tree_zcalloc(size_t num, size_t size, void *parent);
void *tree_zrealloc(void *mem, size_t size);
char *tree_zstrdup(const char *str, void *parent);
char *tree_zstrndup(const char *str, size_t n, void *parent);
void *tree_zfree(void *mem);
void *tree_get_parent(void *mem);
void tree_set_parent(void *mem, void *parent);
void tree_steal(void *mem, void *parent);
void *zrealloc(void *ptr, size_t sz);

long long *zlldup(long long const *src, size_t len);
unsigned long long *zlludup(unsigned long long const *src, size_t len);
long *zldup(long const *src, size_t len);
unsigned long *zludup(unsigned long const *src, size_t len);
int *zidup(int const *src, size_t len);
unsigned int *ziudup(unsigned int const *src, size_t len);

#endif /* ZMALLOC_H_INCLUDED */
