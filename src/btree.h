#ifndef BTREE_H_INCLUDED
#define BTREE_H_INCLUDED

#include <stdio.h>

#include "vector.h"

#define INDEX_SIZE 4
#define INDEX_MSIZE (INDEX_SIZE/2)

#define DEFAULT_RESULT_SIZE		10

typedef struct base base_t;

typedef enum {
	INSERTNOTCOMPLETE,
	SUCCESS,
	DUPLICATEKEY,
	UNDERFLOW,
	NOTFOUND
} status_t;

typedef struct {
	char key[64];
	unsigned char key_size;
	uint64_t valset;
} item_t;

typedef struct {
	int cnt;
	item_t items[INDEX_SIZE];
	long long ptr[INDEX_SIZE + 1];
} node_t;

typedef struct {
	long long root;
	long long freelist;
	node_t rootnode;
	uint64_t offset;
	bool unique_keys;
} btree_t;

status_t btree_insert(base_t *base, btree_t *index, char *key, size_t key_size, long long int offset);
status_t btree_get(base_t *base, btree_t *index, char *key, vector_t **result);
status_t btree_delete(base_t *base, btree_t *index, char *key);
vector_t *btree_get_all(base_t *base, btree_t *index);

#ifdef DEBUG
void btree_print(base_t *base, btree_t *index);
#endif

void btree_set_unique(btree_t *index, bool unique);
uint64_t btree_create(base_t *base, btree_t *index);
void btree_open(base_t *base, btree_t *index, uint64_t offset);
void btree_close(base_t *base, btree_t *index);

#endif // INDEX_H_INCLUDED
