#ifndef SLAY_H_INCLUDED
#define SLAY_H_INCLUDED

#include <config.h>
#include <common.h>

#include "dstype.h"
#include "marshall.h"

#define movetodata_row(row) (void *)(((uint8_t *)row)+sizeof(struct row_slay))
#define next_row(next) (void *)(((uint8_t *)next)+sizeof(struct value_slay)+val_len+namelen)

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

void slay_parse_object(char *data, size_t data_len, size_t *slay_len, struct slay_result *rs);
void *slay_parse_quid(char *data, size_t data_len, size_t *slay_len);
void *slay_parse_text(char *data, size_t data_len, size_t *slay_len);
void *slay_bool(bool boolean, size_t *slay_len);
void *slay_null(size_t *slay_len);
void *slay_char(char *data, size_t *slay_len);
void *slay_float(char *data, size_t data_len, size_t *slay_len);
void *slay_integer(char *data, size_t data_len, size_t *slay_len);
void *slay_put(marshall_t *marshall, size_t *len, slay_result_t *rs);
void slay_put_data(char *data, size_t data_len, size_t *len, struct slay_result *rs);
marshall_t *slay_get(void *data, void *parent);
void *slay_get_data(void *data, dstype_t *dt);
void *create_row(schema_t schema, uint64_t el, size_t data_len, size_t *len);
void *get_row(void *arrp, schema_t *schema, uint64_t *el);
uint8_t *slay_wrap(void *arrp, void *name, size_t namelen, void *data, size_t len, dstype_t dt);
void *slay_unwrap(void *value_slay, void **name, size_t *namelen, size_t *len, dstype_t *dt);
char *str_schema(schema_t schema);

#endif // SLAY_H_INCLUDED
