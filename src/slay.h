#ifndef SLAY_H_INCLUDED
#define SLAY_H_INCLUDED

#include <config.h>
#include <common.h>

#include "marshall.h"

typedef enum {
	SCHEMA_FIELD,
	SCHEMA_ARRAY,
	SCHEMA_OBJECT,
	SCHEMA_TABLE,
	SCHEMA_SET
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

void *slay_put(marshall_t *marshall, size_t *len, slay_result_t *rs);
marshall_t *slay_get(void *data, void *parent, bool descent);
marshall_type_t slay_get_type(void *data);
schema_t slay_get_schema(void *data);
char *slay_get_strschema(void *data);

#endif // SLAY_H_INCLUDED
