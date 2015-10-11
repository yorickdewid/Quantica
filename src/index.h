#ifndef INDEX_H_INCLUDED
#define INDEX_H_INCLUDED

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

struct index {
	long int root;
	long int freelist;
};

status_t index_insert(char *key, size_t key_size, long long int offset);
status_t index_get(char *key);
status_t index_delete(char *key);
void index_print_root();
void index_init(char *treefilname);
void index_close();

#endif // INDEX_H_INCLUDED

