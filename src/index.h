#ifndef INDEX_H_INCLUDED
#define INDEX_H_INCLUDED

#define INDEX_SIZE 4
#define INDEX_MSIZE (INDEX_SIZE/2)
#define NIL -1

typedef enum {
	INSERTNOTCOMPLETE,
	SUCCESS,
	DUPLICATEKEY,
	UNDERFLOW,
	NOTFOUND
} status_t;

typedef struct {
	int cnt;
	int key[INDEX_SIZE];
	long int ptr[INDEX_SIZE+1];
} node_t;

status_t index_insert(int x);
status_t index_get(int x);
status_t index_delete(int x);
void index_print(long int t);

#endif // INDEX_H_INCLUDED

