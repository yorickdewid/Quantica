#ifndef MARSHALL_H_INCLUDED
#define MARSHALL_H_INCLUDED

#define marshall_free(v) tree_zfree(v);

typedef enum {
	DSTYPE_NULL,
	DSTYPE_TRUE,
	DICT_FALSE,
	DICT_INT,
	DICT_STR,
	DICT_ARR,
	DICT_OBJ
} serialize_type_t;

typedef struct serialize {
	char *name;
	void *data;
	struct serialize **child;
	unsigned int sz;
	serialize_type_t type;
} serialize_t;

serialize_t *marshall_decode(char *data, size_t data_len, char *name, void *parent);
char *marshall_encode(serialize_t *obj);
void marshall_print(serialize_t *obj, int depth);

#endif // MARSHALL_H_INCLUDED
