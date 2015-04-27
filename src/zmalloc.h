#ifndef ZMALLOC_H_INCLUDED
#define ZMALLOC_H_INCLUDED

#include <stdlib.h>

#define zmalloc(sz) malloc(sz)
#define zcalloc(n, sz) calloc(n, sz)
#define zrealloc(ptr, sz) realloc(ptr, sz)
#define zstrdup(str) strdup(str)
#define zfree(sz) free(sz)

void *tree_zmalloc(size_t size, void *parent);
void *tree_zcalloc(size_t num, size_t size, void *parent);
void *tree_zrealloc(void *mem, size_t size);
char *tree_zstrdup(const char *str, void *parent);
void *tree_zfree(void *mem);
void *tree_get_parent(void *mem);
void tree_set_parent(void *mem, void *parent);
void tree_steal(void *mem, void *parent);

#endif /* ZMALLOC_H_INCLUDED */

