#ifndef INDEX_H_INCLUDED
#define INDEX_H_INCLUDED

#include "marshall.h"
#include "slay.h"

int index_create_btree(const char *key, marshall_t *marshall, schema_t schema);

#endif // CORE_H_INCLUDED
