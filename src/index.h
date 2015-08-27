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
	uint16_t key;
	__be64 offset;
} item_t;

typedef struct {
	int cnt;
	// int key[INDEX_SIZE];
	item_t items[INDEX_SIZE];
	long int ptr[INDEX_SIZE+1];
} node_t;

status_t index_insert(int key);
status_t index_get(int key);
status_t index_delete(int key);
void index_print(long int t);
void index_init(char *treefilnam);
void index_close();

#endif // INDEX_H_INCLUDED

