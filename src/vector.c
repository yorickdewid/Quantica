#include <stdlib.h>

#include "vector.h"
#include "zmalloc.h"

vector_t *alloc_vector(size_t sz) {
	vector_t *v = (vector_t *)tree_zmalloc(sizeof(vector_t), NULL);
	v->buffer = (void **)tree_zcalloc(sz, sizeof(void *), v);
	v->size = 0;
	v->alloc_size = sz;

	return v;
}

void vector_append(vector_t *v, void *item) {
	if (v->size == v->alloc_size) {
		v->alloc_size *= 2;
		v->buffer = (void **)tree_zrealloc(v->buffer, v->alloc_size * sizeof(void *));
	}

	v->buffer[v->size++] = item;
}

void *vector_at(vector_t *v, unsigned int idx) {
	return idx >= v->size ? NULL : v->buffer[idx];
}

void vector_append_str(vector_t *v, const char *str) {
	void *item = tree_zstrdup(str, v);
	if (v->size == v->alloc_size) {
		v->alloc_size = v->alloc_size * 2;
		v->buffer = (void **)tree_zrealloc(v->buffer, v->alloc_size * sizeof(void *));
	}

	v->buffer[v->size++] = item;
}
