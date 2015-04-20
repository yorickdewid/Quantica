#ifndef SLAY_H_INCLUDED
#define SLAY_H_INCLUDED

#include <config.h>
#include <common.h>

typedef enum datatype {
	DT_NULL,
	DT_INT,
	DT_FLOAT,
	DT_TEXT,
	DT_CHAR,
	DT_BOOL,
	DT_QUID,
} datatype_t;

struct value_slay {
	uint8_t val_type;
	__be64 size;
};

void *slay_wrap(void *data, size_t *len, datatype_t dt);
void slay_print(void *data);
void *slay_unwrap(void *value_slay, size_t *len, datatype_t *dt);

#endif // SLAY_H_INCLUDED
