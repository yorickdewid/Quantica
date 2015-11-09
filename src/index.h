#ifndef INDEX_H_INCLUDED
#define INDEX_H_INCLUDED

#include "quid.h"
#include "marshall.h"
#include "slay.h"

typedef struct {
	quid_t index;
	unsigned long index_elements;
} index_result_t;

int index_create_btree(const char *element, marshall_t *marshall, schema_t schema, index_result_t *result);

#endif // CORE_H_INCLUDED
