#ifndef INDEX_H_INCLUDED
#define INDEX_H_INCLUDED

#define M 2
#define MM 4
#define NIL (-1L)

typedef enum {
	INSERTNOTCOMPLETE,
	SUCCESS,
	DUPLICATEKEY,
	UNDERFLOW,
	NOTFOUND
} status_t;

typedef struct {
	int cnt;
	int key[MM];
	long int ptr[MM+1];
} node_t;

status_t index_insert(int x);
status_t index_get(int x);
status_t index_delete(int x);
void index_print(long int t);

#endif // INDEX_H_INCLUDED