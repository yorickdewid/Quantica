#ifndef SLAY_MARSHALL_H_INCLUDED
#define SLAY_MARSHALL_H_INCLUDED

#include <config.h>
#include <common.h>

#include "base.h"
#include "marshall.h"

typedef enum {
	SCHEMA_FIELD,
	SCHEMA_ARRAY,
	SCHEMA_OBJECT,
	SCHEMA_TABLE,
	SCHEMA_SET,
	SCHEMA_INDEX
} schema_t;

struct value_slay {
	uint16_t val_type;
	__be64 size;
	__be64 namesize;
};

struct row_slay {
	uint64_t elements;
	uint8_t schema;
};

typedef struct {
	unsigned int items;
	schema_t schema;
} slay_result_t;

void *slay_put(base_t *base, marshall_t *marshall, size_t *len, slay_result_t *rs);
void slay_update_row(void *data, slay_result_t *rs);
marshall_t *slay_get(base_t *base, void *data, void *parent, bool descent);
marshall_type_t slay_get_type(void *data);
schema_t slay_get_schema(void *data);
char *slay_get_strschema(void *data);

#endif // SLAY_MARSHALL_H_INCLUDED
