#ifndef MARSHALL_H_INCLUDED
#define MARSHALL_H_INCLUDED

#include <config.h>
#include <common.h>

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
	size_t name_len;
	void *data;
	size_t data_len;
	struct marshall **child;
	unsigned int size;
	marshall_type_t type;
} marshall_t;

bool marshall_type_hasdata(marshall_type_t type);
bool marshall_type_hasdescent(marshall_type_t type);
unsigned int marshall_get_count(marshall_t *obj, int depth, unsigned _depth);
marshall_type_t autoscalar(const char *data, size_t len);
char *marshall_strdata(marshall_t *obj, size_t *len);
marshall_t *marshall_filter(marshall_t *element, marshall_t *marshall, void *parent);
marshall_t *marshall_merge(marshall_t *newobject, marshall_t *marshall);
marshall_t *marshall_separate(marshall_t *filterobject, marshall_t *marshall);
char *marshall_get_strtype(marshall_type_t type);

#endif // MARSHALL_H_INCLUDED
