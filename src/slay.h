#ifndef SLAY_H_INCLUDED
#define SLAY_H_INCLUDED

#include <config.h>
#include <common.h>

#include "dstype.h"

typedef enum {
	SCHEMA_FIELD,
	SCHEMA_ARRAY,
	SCHEMA_ASOCARRAY,
	SCHEMA_TABLE
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

void *slay_parse_object(char *data, size_t data_len, size_t *slay_len, int *items);
void *slay_parse_quid(char *data, size_t *slay_len);
void *slay_parse_text(char *data, size_t data_len, size_t *slay_len);
void *slay_bool(bool boolean, size_t *slay_len);
void *slay_null(size_t *slay_len);
void *slay_char(char *data, size_t *slay_len);
void *slay_float(char *data, size_t data_len, size_t *slay_len);
void *slay_integer(char *data, size_t data_len, size_t *slay_len);
void *slay_put_data(char *data, size_t data_len, size_t *len, int *items);
void *slay_get_data(void *data);
void *create_row(schema_t schema, uint64_t el, size_t data_len, size_t *len);
void *get_row(void *arrp, schema_t *schema, uint64_t *el);
uint8_t *slay_wrap(void *arrp, void *name, size_t namelen, void *data, size_t len, dstype_t dt);
void *slay_unwrap(void *value_slay, void **name, size_t *namelen, size_t *len, dstype_t *dt);

#endif // SLAY_H_INCLUDED
