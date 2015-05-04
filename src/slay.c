#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dstype.h"
#include "slay.h"
#include "quid.h"
#include "json_parse.h"
#include "zmalloc.h"

void *slay_parse_object(char *data, size_t data_len, size_t *slay_len) {
	void *slay = NULL;

	json_value *json = json_parse(data, data_len);
	if (!strcmp(json->u.object.values[0].name, "schema")) {
		if (!strcmp(json->u.object.values[0].value->u.string.ptr, "ARRAY")) {
			if (!strcmp(json->u.object.values[1].name, "data")) {
				slay = create_row(json->u.object.values[1].value->u.array.length, data_len, slay_len);
				void *next = (void *)(((uint8_t *)slay)+sizeof(struct row_slay));

				unsigned int i;
				for(i=0; i<json->u.object.values[1].value->u.array.length; ++i) {
					slay_wrap(next, json->u.object.values[1].value->u.array.values[i]->u.string.ptr, json->u.object.values[1].value->u.array.values[i]->u.string.length, DT_TEXT);
					next = (void *)(((uint8_t *)next)+sizeof(struct value_slay)+json->u.object.values[1].value->u.array.values[i]->u.string.length);
				}
			}
		}
	}

	if (!slay) {
		slay = create_row(1, data_len, slay_len);

		void *next = (void *)(((uint8_t *)slay)+sizeof(struct row_slay));
		slay_wrap(next, data, data_len, DT_JSON);
	}

	json_value_free(json);
	return slay;
}

void *slay_parse_quid(char *data, size_t *slay_len) {
	quid_t pu;
	void *slay = create_row(1, sizeof(quid_t), slay_len);

	strtoquid(data, &pu);
	void *next = (void *)(((uint8_t *)slay)+sizeof(struct row_slay));
	slay_wrap(next, (void *)&pu, sizeof(quid_t), DT_QUID);

	return slay;
}

void *slay_parse_text(char *data, size_t data_len, size_t *slay_len) {
	void *slay = create_row(1, data_len, slay_len);

	void *next = (void *)(((uint8_t *)slay)+sizeof(struct row_slay));
	slay_wrap(next, data, data_len, DT_TEXT);
	return slay;
}

void *slay_bool(bool boolean, size_t *slay_len) {
	void *slay = create_row(1, 0, slay_len);

	void *next = (void *)(((uint8_t *)slay)+sizeof(struct row_slay));
	slay_wrap(next, NULL, 0, boolean ? DT_BOOL_T : DT_BOOL_F);
	return slay;
}

void *slay_char(char *data, size_t *slay_len) {
	void *slay = create_row(1, 1, slay_len);

	((uint8_t *)data)[1] = '\0';
	void *next = (void *)(((uint8_t *)slay)+sizeof(struct row_slay));
	slay_wrap(next, data, 1, DT_CHAR);
	return slay;
}

void *slay_integer(char *data, size_t data_len, size_t *slay_len) {
	void *slay = create_row(1, data_len, slay_len);

	void *next = (void *)(((uint8_t *)slay)+sizeof(struct row_slay));
	slay_wrap(next, data, data_len, DT_INT);
	return slay;
}

void *create_row(uint64_t el, size_t data_len, size_t *len) {
	struct row_slay *row = zcalloc(1, sizeof(struct row_slay)+(el * sizeof(struct value_slay))+data_len);
	row->elements = el;
	*len = sizeof(struct row_slay)+(el * sizeof(struct value_slay))+data_len;
	return (void *)row;
}

void *get_row(void *val, uint64_t *el) {
	struct row_slay *row = (struct row_slay *)val;
	*el = row->elements;
	return (void *)row;
}

void slay_wrap(void *arrp, void *data, size_t len, dstype_t dt) {
	size_t data_size = len;
	if (isdata(dt))
		data_size = 0;

	struct value_slay *slay = (struct value_slay *)arrp;
	slay->val_type = dt;
	slay->size = data_size;
	if (!slay->size)
		return;

	uint8_t *dest = ((uint8_t *)slay)+sizeof(struct value_slay);
	memcpy(dest, data, data_size);
}

void *slay_unwrap(void *arrp, size_t *len, dstype_t *dt) {
	struct value_slay *slay = (struct value_slay *)arrp;
	void *data = NULL;

	if (slay->size) {
		void *src = ((uint8_t *)arrp)+sizeof(struct value_slay);
		data = zmalloc(slay->size);
		memcpy(data, src, slay->size);
	}

	*dt = slay->val_type;
	*len = slay->size;

	return data;
}
