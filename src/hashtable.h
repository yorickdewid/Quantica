#ifndef HASHTABLE_H_INCLUDED
#define HASHTABLE_H_INCLUDED

#define MULTIPLIER 97
#define GROWTH_FACTOR 2
#define MAX_LOAD_FACTOR 1

struct item {
	struct item *next;
	char *key;
	char *value;
};

struct hashtable {
	int size;			/* size of the pointer table */
	int n;				/* number of elements stored */
	struct item **table;
};

typedef struct hashtable hashtable_t;

/* create a new empty dictionary */
hashtable_t *alloc_hashtable(int size);

/* destroy a dictionary */
void free_hashtable(hashtable_t *);

/* insert a new key-value pair into an existing dictionary */
void hashtable_put(hashtable_t **, const char *key, const char *value);

/* return the most recently inserted value associated with a key */
/* or 0 if no matching key is present */
const char *hashtable_get(hashtable_t *, const char *key);

#ifdef DEBUG
void hashtable_dump(hashtable_t *d);
#endif

/* delete the most recently inserted record with the given key */
/* if there is no such record, has no effect */
void hashtable_delete(hashtable_t *, const char *key);

#endif // HASHTABLE_H_INCLUDED
