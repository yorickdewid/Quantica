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
	long long int key;
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

status_t index_insert(long long int key, long long int offset);
status_t index_get(long long int key);
status_t index_delete(long long int key);
void index_print(long int t);
void index_print_root();
void index_init(char *treefilnam);
void index_close();

#endif // INDEX_H_INCLUDED

