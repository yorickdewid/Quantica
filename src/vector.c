#include <stdlib.h>

#include "vector.h"
#include "zmalloc.h"

vector_t *alloc_vector(size_t sz) {
	vector_t *v = (vector_t *)tree_zmalloc(sizeof(vector_t), NULL);
	v->buffer = (void **)tree_zmalloc(sz * sizeof(void *), v);
	v->size = 0;
	v->alloc_size = sz;

	return v;
}

void free_vector(vector_t *v) {
	zfree(v->buffer);
}

void vector_append(vector_t *v, void *item) {
	if(v->size == v->alloc_size) {
		v->alloc_size = v->alloc_size * 2;
		v->buffer = (void **)tree_zrealloc(v->buffer, v->alloc_size *sizeof(void *));
	}

	v->buffer[v->size] = item;
	v->size++;
}

void *vector_at(vector_t *v, unsigned int idx) {
	return idx >= v->size ? NULL : v->buffer[idx];
}
