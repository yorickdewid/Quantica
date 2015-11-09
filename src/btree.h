#ifndef BTREE_H_INCLUDED
#define BTREE_H_INCLUDED

#include <stdio.h>

#define INDEX_SIZE 4
#define INDEX_MSIZE (INDEX_SIZE/2)

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
	unsigned long long int valset;
} item_t;

typedef struct {
	int cnt;
	item_t items[INDEX_SIZE];
	long int ptr[INDEX_SIZE + 1];
} node_t;

typedef struct {
	long int root;
	long int freelist;
	node_t rootnode;
	FILE *fp;
} btree_t;

status_t btree_insert(btree_t *index, char *key, size_t key_size, long long int offset);
status_t btree_get(btree_t *index, char *key);
status_t btree_delete(btree_t *index, char *key);
void btree_print();
void btree_init(btree_t *index, char *name);
void btree_close(btree_t *index);

#endif // INDEX_H_INCLUDED

