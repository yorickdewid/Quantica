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
} serialize_type_t;

typedef struct serialize {
	char *name;
	void *data;
	//TODO size_t data_len;
	struct serialize **child;
	unsigned int sz;
	serialize_type_t type;
} serialize_t;

typedef struct {
	//schema_t schema;	/* NOTUSED */
	serialize_type_t type;
	serialize_t *data;
} marshall_t;

unsigned int marshall_get_count(serialize_t *obj, int depth, unsigned _depth);
marshall_t *marshall_convert(char *data, size_t data_len);
char *marshall_object_serialize(serialize_t *obj);
//char *marshall_serialize(marshall_t *marshall);

#endif // MARSHALL_H_INCLUDED
