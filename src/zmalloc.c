#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <common.h>
#include <error.h>
#include "zmalloc.h"

#define HEADER_SIZE (sizeof(void*) * 3)

#define  raw2usr(mem) (void*)((void**)(mem) + 3)
#define  usr2raw(mem) (void*)((void**)(mem) - 3)
#define    child(mem) (((void**)(mem))[-3])
#define     next(mem) (((void**)(mem))[-2])
#define     prev(mem) (((void**)(mem))[-1])
#define   parent(mem) prev(mem)
#define  is_root(mem) (!prev(mem))
#define is_first(mem) (next(prev(mem)) != (mem))

static void *tree_zmalloc_init(void *mem, void *parent) {
	if (!mem)
		return NULL;

	memset(mem, 0, HEADER_SIZE);
	mem = raw2usr(mem);

	tree_set_parent(mem, parent);
	return mem;
}

void *tree_zmalloc(size_t size, void *parent) {
	return tree_zmalloc_init(zmalloc(size + HEADER_SIZE), parent);
}

void *tree_zcalloc(size_t num, size_t size, void *parent) {
	return tree_zmalloc_init(zcalloc(num, size + HEADER_SIZE), parent);
}

void *tree_zrealloc(void *usr, size_t size) {
	void *mem = zrealloc(usr ? usr2raw(usr) : NULL, size + HEADER_SIZE);

	if (!usr || !mem)
		return tree_zmalloc_init(mem, NULL);

	mem = raw2usr(mem);
	/* If the buffer starting address changed, update all references. */
	if (mem != usr) {
		if (child(mem))
			parent(child(mem)) = mem;

		if (!is_root(mem)) {
			if (next(mem))
				prev(next(mem)) = mem;

			if (next(prev(mem)) == usr)
				next(prev(mem)) = mem;

			if (child(parent(mem)) == usr)
				child(parent(mem)) = mem;
		}
	}

	return mem;
}

char *tree_zstrdup(const char *str, void *parent) {
	size_t size;
	char *copy = NULL;

	size = strlen(str) + 1;
	if ((copy = tree_zmalloc(size, parent)) == NULL) {
		return NULL;
	}
	memcpy(copy, str, size);

	return copy;
}

char *tree_zstrndup(const char *str, size_t n, void *parent) {
	char *copy = NULL;

	if ((copy = tree_zmalloc(n + 1, parent)) == NULL) {
		return NULL;
	}
	memcpy(copy, str, n);
	copy[n] = '\0';

	return copy;
}

long long *zlldup(long long const *src, size_t len) {
	long long *p = zmalloc(len * sizeof(long long));
	memcpy(p, src, len * sizeof(long long));
	return p;
}

unsigned long long *zlludup(unsigned long long const *src, size_t len) {
	unsigned long long *p = zmalloc(len * sizeof(unsigned long long));
	memcpy(p, src, len * sizeof(unsigned long long));
	return p;
}

long *zldup(long const *src, size_t len) {
	long *p = zmalloc(len * sizeof(long));
	memcpy(p, src, len * sizeof(long));
	return p;
}

unsigned long *zludup(unsigned long const *src, size_t len) {
	unsigned long *p = zmalloc(len * sizeof(unsigned long));
	memcpy(p, src, len * sizeof(unsigned long));
	return p;
}

int *zidup(int const *src, size_t len) {
	int *p = zmalloc(len * sizeof(int));
	memcpy(p, src, len * sizeof(int));
	return p;
}

unsigned int *ziudup(unsigned int const *src, size_t len) {
	unsigned int *p = zmalloc(len * sizeof(unsigned int));
	memcpy(p, src, len * sizeof(unsigned int));
	return p;
}

static void __zfree(void *mem) {
	if (!mem)
		return;

	/* Fail if the tree hierarchy has cycles. */
	zassert(prev(mem));
	prev(mem) = NULL;

	__zfree(child(mem));
	__zfree(next(mem));
	zfree(usr2raw(mem));
}

void *tree_zfree(void *mem) {
	if (!mem)
		return NULL;

	tree_set_parent(mem, NULL);

	__zfree(child(mem));
	zfree(usr2raw(mem));

	return NULL;
}

void *tree_get_parent(void *mem) {
	if (!mem || is_root(mem))
		return NULL;

	while (!is_first(mem))
		mem = prev(mem);

	return parent(mem);
}

void tree_set_parent(void *mem, void *parent) {
	if (!mem)
		return;

	if (!is_root(mem)) {
		/* Remove node from old tree. */
		if (next(mem))
			prev(next(mem)) = prev(mem);

		if (!is_first(mem))
			next(prev(mem)) = next(mem);

		if (is_first(mem))
			child(parent(mem)) = next(mem);
	}

	next(mem) = prev(mem) = NULL;
	if (parent) {
		/* Insert node into new tree. */
		if (child(parent)) {
			next(mem) = child(parent);
			prev(child(parent)) = mem;
		}

		parent(mem) = parent;
		child(parent) = mem;
	}
}

void tree_steal(void *mem, void *parent) {
	if (!mem)
		return;

	tree_set_parent(mem, NULL);
	if (!child(mem))
		return;

	if (parent) {
		/* Insert mem children in front of the list of parent children. */
		if (child(parent)) {
			void *last = child(mem);
			while (next(last))
				last = next(last);

			prev(child(parent)) = last;
			next(last) = child(parent);
		}
		child(parent) = child(mem);
	}

	parent(child(mem)) = parent;
	child(mem) = NULL;
}

void *zrealloc(void *ptr, size_t sz) {
	void *_ptr = ptr;
	if ((ptr = realloc(_ptr, sz)) == NULL) {
		zfree(ptr);
		return NULL;
	}
	return ptr;
}
