#ifndef INDEX_H_INCLUDED
#define INDEX_H_INCLUDED

#include "quid.h"
#include "marshall.h"
#include "slay.h"

typedef struct {
	quid_t index;
	unsigned long index_elements;
} index_result_t;

typedef struct {
	char *key;
	size_t key_len;
	unsigned long long int value;
} index_keyval_t;

int index_btree_create(const char *element, marshall_t *marshall, schema_t schema, index_result_t *result);
marshall_t *index_btree_all(quid_t *key);

#endif // CORE_H_INCLUDED
