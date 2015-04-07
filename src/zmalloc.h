#ifndef ZMALLOC_H_INCLUDED
#define ZMALLOC_H_INCLUDED

#include <stddef.h>

void *talloc(size_t size, void *parent);
void *tzalloc(size_t size, void *parent);
void *tree_zrealloc(void *mem, size_t size);
void *zfree(void *mem);
void *tree_get_parent(void *mem);
void tree_set_parent(void *mem, void *parent);
void tree_steal(void *mem, void *parent);

#endif /* ZMALLOC_H_INCLUDED */

