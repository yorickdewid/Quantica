#ifndef SLAY_H_INCLUDED
#define SLAY_H_INCLUDED

#include <config.h>
#include <common.h>

#include "dstype.h"
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

// DEPRECATED by slay_result_t
struct slay_result {
	void *slay;
	int items;
	bool table;
};

typedef struct {
	unsigned int items;
	schema_t schema;
} slay_result_t;

void *slay_put(marshall_t *marshall, size_t *len, slay_result_t *rs);
marshall_t *slay_get(void *data, void *parent);
char *str_schema(schema_t schema);

#endif // SLAY_H_INCLUDED
