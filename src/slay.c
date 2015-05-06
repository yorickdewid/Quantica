#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dstype.h"
#include "slay.h"
#include "quid.h"
#include "json_parse.h"
#include "json_encode.h"
#include "core.h"
#include "zmalloc.h"

void *slay_parse_object(char *data, size_t data_len, size_t *slay_len) {
	void *slay = NULL;

	json_value *json = json_parse(data, data_len);
	if (!strcmp(json->u.object.values[0].name, "schema")) {
		if (!strcmp(json->u.object.values[0].value->u.string.ptr, "ARRAY")) {
			if (!strcmp(json->u.object.values[1].name, "data")) {
				slay = create_row(SCHEMA_ARRAY, json->u.object.values[1].value->u.array.length, data_len, slay_len);
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
		slay = create_row(SCHEMA_FIELD, 1, data_len, slay_len);

		void *next = (void *)(((uint8_t *)slay)+sizeof(struct row_slay));
		slay_wrap(next, data, data_len, DT_JSON);
	}

	json_value_free(json);
	return slay;
}

void *slay_parse_quid(char *data, size_t *slay_len) {
	quid_t pu;
	void *slay = create_row(SCHEMA_FIELD, 1, sizeof(quid_t), slay_len);

	strtoquid(data, &pu);
	void *next = (void *)(((uint8_t *)slay)+sizeof(struct row_slay));
	slay_wrap(next, (void *)&pu, sizeof(quid_t), DT_QUID);

	return slay;
}

void *slay_parse_text(char *data, size_t data_len, size_t *slay_len) {
	void *slay = create_row(SCHEMA_FIELD, 1, data_len, slay_len);

	void *next = (void *)(((uint8_t *)slay)+sizeof(struct row_slay));
	slay_wrap(next, data, data_len, DT_TEXT);
	return slay;
}

void *slay_bool(bool boolean, size_t *slay_len) {
	void *slay = create_row(SCHEMA_FIELD, 1, 0, slay_len);

	void *next = (void *)(((uint8_t *)slay)+sizeof(struct row_slay));
	slay_wrap(next, NULL, 0, boolean ? DT_BOOL_T : DT_BOOL_F);
	return slay;
}

void *slay_char(char *data, size_t *slay_len) {
	void *slay = create_row(SCHEMA_FIELD, 1, 1, slay_len);

	((uint8_t *)data)[1] = '\0';
	void *next = (void *)(((uint8_t *)slay)+sizeof(struct row_slay));
	slay_wrap(next, data, 1, DT_CHAR);
	return slay;
}

void *slay_integer(char *data, size_t data_len, size_t *slay_len) {
	void *slay = create_row(SCHEMA_FIELD, 1, data_len, slay_len);

	void *next = (void *)(((uint8_t *)slay)+sizeof(struct row_slay));
	slay_wrap(next, data, data_len, DT_INT);
	return slay;
}

void *slay_get_data(void *data) {
	uint64_t elements;
	schema_t schema;
	void *slay = get_row(data, &schema, &elements);
	void *next = (void *)(((uint8_t *)slay)+sizeof(struct row_slay));

	char *buf = NULL;
	switch (schema) {
		case SCHEMA_FIELD: {
			size_t val_len;
			dstype_t val_dt;

			void *val_data = slay_unwrap(next, &val_len, &val_dt);
			next = (void *)(((uint8_t *)next)+sizeof(struct value_slay)+val_len);
			switch (val_dt) {
				case DT_NULL:
					buf = zstrdup(str_null());
					break;
				case DT_BOOL_T:
					buf = zstrdup(str_bool(TRUE));
					break;
				case DT_BOOL_F:
					buf = zstrdup(str_bool(FALSE));
					break;
				case DT_INT:
				case DT_FLOAT:
					val_data = (void *)realloc(val_data, val_len+1);
					((uint8_t *)val_data)[val_len] = '\0';
					buf = zstrdup(val_data);
					break;
				case DT_CHAR:
				case DT_TEXT: {
					val_data = (char *)realloc(val_data, val_len+1);
					((uint8_t *)val_data)[val_len] = '\0';
					char *escdata = stresc(val_data);
					buf = zmalloc(val_len+3);
					snprintf(buf, val_len+3, "\"%s\"", escdata);
					zfree(escdata);
					break;
				}
				case DT_JSON:
					val_data = (void *)realloc(val_data, val_len+1);
					((uint8_t *)val_data)[val_len] = '\0';
					buf = zstrdup(val_data);
					break;
				case DT_QUID: {
					buf = _db_get((quid_t *)val_data);
					break;
				}
			}

			zfree(val_data);
			break;
		}
		case SCHEMA_ARRAY: {
			size_t val_len;
			dstype_t val_dt;
			unsigned int i;
			json_value *arr = json_array_new(0);
			for (i=0; i<elements; ++i) {
				void *val_data = slay_unwrap(next, &val_len, &val_dt);
				next = (void *)(((uint8_t *)next)+sizeof(struct value_slay)+val_len);

				val_data = (char *)zrealloc(val_data, val_len+1);
				((char *)val_data)[val_len] = '\0';
				json_array_push(arr, json_string_new(val_data));
				zfree(val_data);
			}

			buf = malloc(json_measure(arr));
			json_serialize(buf, arr);
			json_builder_free(arr);
			break;
		}
		case SCHEMA_OBJECTS:
		case SCHEMA_TABLE:
			/* Not implemented */
			break;
	}

	return buf;
}

void *create_row(schema_t schema, uint64_t el, size_t data_len, size_t *len) {
	struct row_slay *row = zcalloc(1, sizeof(struct row_slay)+(el * sizeof(struct value_slay))+data_len);
	row->elements = el;
	row->schema = schema;
	*len = sizeof(struct row_slay)+(el * sizeof(struct value_slay))+data_len;
	return (void *)row;
}

void *get_row(void *val, schema_t *schema, uint64_t *el) {
	struct row_slay *row = (struct row_slay *)val;
	*el = row->elements;
	*schema = row->schema;
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
