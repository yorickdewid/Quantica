#ifndef MARSHALL_H_INCLUDED
#define MARSHALL_H_INCLUDED

#define marshall_free(v) tree_zfree(v);

typedef enum {
	MTYPE_NULL,
	MTYPE_TRUE,
	MTYPE_FALSE,
	MTYPE_INT,
	MTYPE_FLOAT,
	MTYPE_STRING,
	MTYPE_QUID,
	MTYPE_ARRAY,
	MTYPE_OBJECT
} marshall_type_t;

typedef struct marshall {
	char *name;
	//TODO size_t name_len;
	void *data;
	//TODO size_t data_len;
	struct marshall **child;
	unsigned int size;
	marshall_type_t type;
} marshall_t;

unsigned int marshall_get_count(marshall_t *obj, int depth, unsigned _depth);
marshall_t *marshall_convert(char *data, size_t data_len);
char *marshall_serialize(marshall_t *obj);

#endif // MARSHALL_H_INCLUDED
