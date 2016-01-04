#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <common.h>
#include <error.h>
#include "zmalloc.h"
#include "hashtable.h"

/* dictionary initialization code used in both DictCreate and grow */
hashtable_t *alloc_hashtable(int size) {
	hashtable_t *d;

	d = (hashtable_t *)tree_zmalloc(sizeof(hashtable_t), NULL);
	zassert(d != 0);
	d->size = size;
	d->n = 0;
	d->table = (struct item **)tree_zcalloc(d->size, sizeof(struct item *), d);
	zassert(d->table != 0);

	for (int i = 0; i < d->size; ++i)
		d->table[i] = NULL;

	return d;
}

void free_hashtable(hashtable_t *d) {
	int i;
	struct item *e;
	struct item *next;

	for (i = 0; i < d->size; ++i) {
		for (e = d->table[i]; e != 0; e = next) {
			next = e->next;
			tree_zfree(e);
		}
	}
	tree_zfree(d);
}

static unsigned long int hash_generate(const char *s) {
	unsigned const char *us;
	unsigned long int h = 0;

	for (us = (unsigned const char *)s; *us; ++us)
		h = h * MULTIPLIER + *us;

	return h;
}

static void hashtable_grow(hashtable_t **d) {
	hashtable_t *d2;	/* new dictionary we'll create */
	struct item *e;
	hashtable_t *_d = (hashtable_t *)*d;

	d2 = alloc_hashtable(_d->size * GROWTH_FACTOR);
	for (int i = 0; i < _d->size; i++) {
		for (e = _d->table[i]; e != 0; e = e->next) {
			/* note: this recopies everything */
			/* a more efficient implementation would
			 * patch out the strdups inside hashtable_put
			 * to avoid this problem */
			hashtable_put(&d2, e->key, e->value);
		}
	}

	/* the hideous part */
	/* We'll swap the guts of d and d2 */
	/* then call free on d2 */
	free_hashtable(*d);
	*d = d2;
}

/* insert a new key-value pair into an existing dictionary */
void hashtable_put(hashtable_t **d, const char *key, const char *value) {
	struct item *e;
	unsigned long int h;
	hashtable_t *_d = (hashtable_t *)*d;

	zassert(key);
	zassert(value);

	/* hashtable_grow table if there is not enough room */
	if (_d->n >= _d->size * MAX_LOAD_FACTOR) {
		hashtable_grow(d);
		_d = (hashtable_t *)*d;
	}

	e = (struct item *)tree_zmalloc(sizeof(struct item), NULL);
	zassert(e);
	h = hash_generate(key) % _d->size;
	e->key = tree_zstrdup(key, e);
	e->value = tree_zstrdup(value, e);
	e->next = _d->table[h];
	_d->table[h] = e;
	_d->n++;
}

/* return the most recently inserted value associated with a key */
/* or 0 if no matching key is present */
const char *hashtable_get(hashtable_t *d, const char *key) {
	struct item *e;

	for (e = d->table[hash_generate(key) % d->size]; e != 0; e = e->next) {
		if (!strcmp(e->key, key)) {
			/* got it */
			return e->value;
		}
	}

	return 0;
}

#ifdef DEBUG
void hashtable_dump(hashtable_t *d) {
	struct item *e;

	for (int i = 0; i < d->size; i++) {
		int x = 0;
		for (e = d->table[i]; e != 0; e = e->next) {
			printf("Location %d:%d key: %s, value: %s\n", i, x, e->key, e->value);
			x++;
		}
	}
}
#endif

/* delete the most recently inserted record with the given key */
/* if there is no such record, has no effect */
void hashtable_delete(hashtable_t *d, const char *key) {
	struct item **prev;			/* what to change when item is deleted */
	struct item *e;				/* what to delete */

	for (prev = &(d->table[hash_generate(key) % d->size]);
	        *prev != 0;
	        prev = &((*prev)->next)) {
		if (!strcmp((*prev)->key, key)) {
			/* got it */
			e = *prev;
			*prev = e->next;
			tree_zfree(e);
			return;
		}
	}
}
