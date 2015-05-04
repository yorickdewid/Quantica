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
	uint64_t elements;
};

void *slay_parse_object(char *data, size_t data_len, size_t *slay_len);
void *slay_parse_quid(char *data, size_t *slay_len);
void *slay_parse_text(char *data, size_t data_len, size_t *slay_len);
void *slay_bool(bool boolean, size_t *slay_len);
void *slay_char(char *data, size_t *slay_len);
void *slay_integer(char *data, size_t data_len, size_t *slay_len);
void *create_row(uint64_t el, size_t data_len, size_t *len);
void *get_row(void *arrp, uint64_t *el);
void slay_wrap(void *arrp, void *data, size_t len, dstype_t dt);
void *slay_unwrap(void *value_slay, size_t *len, dstype_t *dt);

#endif // SLAY_H_INCLUDED
