#ifndef VECTOR_H_INCLUDED
#define VECTOR_H_INCLUDED

#include <string.h>

#define vector_free(v) tree_zfree(v);

typedef struct {
	void **buffer;
	unsigned int size;
	unsigned int alloc_size;
} vector_t;

vector_t *alloc_vector(size_t sz);
void vector_append(vector_t *v, void *item);
void *vector_at(vector_t *v, unsigned int idx);
void vector_append_str(vector_t *v, const char *str);

#endif // VECTOR_H_INCLUDED
