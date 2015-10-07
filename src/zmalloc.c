#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
	char *copy;

	size = strlen(str) + 1;
	if ((copy = tree_zmalloc(size, parent)) == NULL) {
		return NULL;
	}
	memcpy(copy, str, size);

	return copy;
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

inline void *zrealloc(void *ptr, size_t sz) {
	void *_ptr = ptr;
	if ((ptr = realloc(_ptr, sz)) == NULL) {
		zfree(ptr);
		return NULL;
	}
	return ptr;
}
