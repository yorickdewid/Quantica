#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include <common.h>
#include "hashtable.h"

/* dictionary initialization code used in both DictCreate and grow */
hashtable_t *alloc_hashtable(int size) {
	hashtable_t *d;
	int i;

	d = malloc(sizeof(hashtable_t));
	assert(d != 0);
	d->size = size;
	d->n = 0;
	d->table = malloc(sizeof(struct item *) * d->size);
	assert(d->table != 0);

	for(i=0; i<d->size; ++i) {
		d->table[i] = 0;
	}

	return d;
}

void free_hashtable(hashtable_t *d) {
	int i;
	struct item *e;
	struct item *next;

	for(i=0; i<d->size; ++i) {
		for(e=d->table[i]; e!=0; e=next) {
			next = e->next;

			free(e->key);
			free(e->value);
			free(e);
		}
	}

	free(d->table);
	free(d);
}

static unsigned long int hash_generate(const char *s) {
	unsigned const char *us;
	unsigned long int h = 0;

	for(us = (unsigned const char *) s; *us; ++us) {
		h = h * MULTIPLIER + *us;
	}

	return h;
}

static void hashtable_grow(hashtable_t *d) {
	hashtable_t *d2;            /* new dictionary we'll create */
	hashtable_t swap;   /* temporary structure for brain transplant */
	int i;
	struct item *e;

	d2 = alloc_hashtable(d->size * GROWTH_FACTOR);
	for(i = 0; i < d->size; i++) {
		for(e = d->table[i]; e != 0; e = e->next) {
			/* note: this recopies everything */
			/* a more efficient implementation would
			 * patch out the strdups inside hashtable_put
			 * to avoid this problem */
			hashtable_put(d2, e->key, e->value);
		}
	}

	/* the hideous part */
	/* We'll swap the guts of d and d2 */
	/* then call DictDestroy on d2 */
	swap = *d;
	*d = *d2;
	*d2 = swap;

	free_hashtable(d2);
}

/* insert a new key-value pair into an existing dictionary */
void hashtable_put(hashtable_t *d, const char *key, const char *value) {
	struct item *e;
	unsigned long int h;

	assert(key);
	assert(value);

	e = malloc(sizeof(struct item));
	assert(e);

	h = hash_generate(key) % d->size;
	e->key = strdup(key);
	e->value = strdup(value);
	e->next = d->table[h];
	d->table[h] = e;
	d->n++;

	/* hashtable_grow table if there is not enough room */
	if(d->n >= d->size * MAX_LOAD_FACTOR) {
		hashtable_grow(d);
	}
}

/* return the most recently inserted value associated with a key */
/* or 0 if no matching key is present */
const char *hashtable_get(hashtable_t *d, const char *key) {
	struct item *e;

	for(e=d->table[hash_generate(key) % d->size]; e!= 0; e=e->next) {
		if(!strcmp(e->key, key)) {
			/* got it */
			return e->value;
		}
	}

	return 0;
}

/* delete the most recently inserted record with the given key */
/* if there is no such record, has no effect */
void hashtable_delete(hashtable_t *d, const char *key) {
	struct item **prev;          /* what to change when item is deleted */
	struct item *e;              /* what to delete */

	for(prev = &(d->table[hash_generate(key) % d->size]);
		*prev != 0;
		prev = &((*prev)->next)) {
		if(!strcmp((*prev)->key, key)) {
			/* got it */
			e = *prev;
			*prev = e->next;

			free(e->key);
			free(e->value);
			free(e);

			return;
		}
	}
}
