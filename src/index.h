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
	int64_t key;
	uint64_t offset;
} item_t;

typedef struct {
	int cnt;
	item_t items[INDEX_SIZE];
	long int ptr[INDEX_SIZE+1];
} node_t;

status_t index_insert(int64_t key);
status_t index_get(int64_t key);
status_t index_delete(int64_t key);
void index_print(long int t);
void index_print_root();
void index_init(char *treefilnam);
void index_close();

#endif // INDEX_H_INCLUDED

