#ifndef SLAY_H_INCLUDED
#define SLAY_H_INCLUDED

#include <config.h>
#include <common.h>

#include "dstype.h"

struct value_slay {
	uint16_t val_type;
	__be64 size;
};

struct row_slay {
	__be64 elements;
};

void *slay_wrap(void *data, size_t *len, dstype_t dt);
void slay_print(void *data);
void *slay_unwrap(void *value_slay, size_t *len, dstype_t *dt);

#endif // SLAY_H_INCLUDED
