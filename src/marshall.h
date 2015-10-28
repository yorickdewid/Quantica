#ifndef MARSHALL_H_INCLUDED
#define MARSHALL_H_INCLUDED

#include "slay.h"

#define marshall_free(v) tree_zfree(v);

typedef enum {
	MTYPE_NULL,
	MTYPE_TRUE,
	MTYPE_FALSE,
	MTYPE_INT,
	MTYPE_STRING,
	MTYPE_ARRAY,
	MTYPE_OBJECT
} serialize_type_t;

typedef struct serialize {
	char *name;
	void *data;
	struct serialize **child;
	unsigned int sz;
	serialize_type_t type;
} serialize_t;

typedef struct {
	schema_t schema;
	size_t size;
	serialize_t *data;
} marshall_t;

serialize_t *marshall_decode(char *data, size_t data_len, char *name, void *parent);
char *marshall_encode(serialize_t *obj);
void marshall_print(serialize_t *obj, int depth);

#endif // MARSHALL_H_INCLUDED
