#ifndef INDEX_H_INCLUDED
#define INDEX_H_INCLUDED

#include "quid.h"
#include "marshall.h"
#include "pager.h"
#include "slay_marshall.h"

typedef struct {
	quid_t index;
	unsigned long index_elements;
	unsigned int element;
} index_result_t;

typedef struct {
	char *key;
	size_t key_len;
	unsigned long long int value;
} index_keyval_t;

int index_btree_create_table(base_t *base, char *squid, const char *element, marshall_t *marshall, index_result_t *result);
int index_btree_create_set(base_t *base, char *squid, const char *element, marshall_t *marshall, index_result_t *result);
marshall_t *index_btree_all(base_t *base, quid_t *key, bool descent);

#endif // CORE_H_INCLUDED
