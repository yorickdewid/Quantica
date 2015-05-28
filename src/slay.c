#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <log.h>

#include "dstype.h"
#include "slay.h"
#include "quid.h"
#include "dict.h"
#include "json_encode.h"
#include "core.h"
#include "zmalloc.h"

#define movetodata_row(row) (void *)(((uint8_t *)row)+sizeof(struct row_slay))
#define next_row(next) (void *)(((uint8_t *)next)+sizeof(struct value_slay)+val_len+namelen)

json_value *parse_json(json_value *json) {
	unsigned int i = 0;
	switch (json->type) {
		case json_none:
			return json_null_new();
		case json_object: {
			json_value *obj = json_object_new(json->u.object.length);
			for(; i<json->u.object.length; ++i)
				json_object_push(obj, json->u.object.values[i].name, parse_json(json->u.object.values[i].value));
			return obj;
		}
		case json_array: {
			json_value *arr = json_array_new(json->u.array.length);
			for(; i<json->u.array.length; ++i)
				json_array_push(arr, parse_json(json->u.array.values[i]));
			return arr;
		}
		case json_integer:
			return json_integer_new(json->u.integer);
		case json_double:
			return json_double_new(json->u.dbl);
		case json_string:
			return json_string_new(json->u.string.ptr);
		case json_boolean:
			return json_boolean_new(json->u.boolean);
		case json_null:
			return json_null_new();
	}
	return json_null_new();
}

int dict_levelcount(dict_token_t *t, int depth, int level, int *cnt) {
	int i, j;
	depth++;
	if (depth == level)
		(*cnt)++;
	if (t->type == DICT_PRIMITIVE) {
		return 1;
	} else if (t->type == DICT_STRING) {
		return 1;
	} else if (t->type == DICT_OBJECT) {
		j = 0;
		for (i = 0; i < t->size; i++) {
			j += dict_levelcount(t+1+j, depth, level, cnt);
			j += dict_levelcount(t+1+j, depth, level, cnt);
		}
		return j+1;
	} else if (t->type == DICT_ARRAY) {
		j = 0;
		for (i = 0; i < t->size; i++) {
			j += dict_levelcount(t+1+j, depth, level, cnt);
		}
		return j+1;
	}
	return 0;
}

void *slay_parse_object(char *data, size_t data_len, size_t *slay_len, int *items) {
	void *slay = NULL;

	int i;
	int r;
	dict_parser p;
	dict_token_t t[data_len];

	dict_init(&p);
	r = dict_parse(&p, data, data_len, t, data_len);
	if (r < 1)
		lprintf("[erro] Failed to parse dict\n");

	if (t[0].type == DICT_ARRAY) {
		int cnt = 0;
		dict_levelcount(t, 0, 2, &cnt);
		*items = cnt;

		slay = create_row(SCHEMA_ARRAY, cnt, data_len, slay_len);
		void *next = movetodata_row(slay);
		for (i=1; i<r; ++i) {
			if (t[i].type == DICT_PRIMITIVE) {
				if (dict_cmp(data, &t[i], "null")) {
					next = slay_wrap(next, NULL, 0, NULL, 0, DT_NULL);
				} else if (dict_cmp(data, &t[i], "true")) {
					next = slay_wrap(next, NULL, 0, NULL, 0, DT_BOOL_T);
				} else if (dict_cmp(data, &t[i], "false")) {
					next = slay_wrap(next, NULL, 0, NULL, 0, DT_BOOL_F);
				} else {
					next = slay_wrap(next, NULL, 0, data+t[i].start, t[i].end - t[i].start, DT_INT);
				}
			} else if (t[i].type == DICT_STRING) {
				next = slay_wrap(next, NULL, 0, data+t[i].start, t[i].end - t[i].start, DT_TEXT);
			} else if (t[i].type == DICT_OBJECT) {
				next = slay_wrap(next, NULL, 0, data+t[i].start, t[i].end - t[i].start, DT_JSON);
				int x, j = 0;
				for (x=0; x<t[i].size; x++) {
					j += dict_levelcount(&t[i+1+j], 0, 0, NULL);
					j += dict_levelcount(&t[i+1+j], 0, 0, NULL);
				}
				i += j;
			} else if (t[i].type == DICT_ARRAY) {
				next = slay_wrap(next, NULL, 0, data+t[i].start, t[i].end - t[i].start, DT_JSON);
				int x, j = 0;
				for (x=0; x<t[i].size; x++) {
					j += dict_levelcount(&t[i+1+j], 0, 0, NULL);
				}
				i += j;
			}
		}
	} else if (t[0].type == DICT_OBJECT) {
		int cnt = 0;
		dict_levelcount(t, 0, 2, &cnt);
		cnt /= 2;
		*items = cnt;

		slay = create_row(SCHEMA_ASOCARRAY, cnt, data_len, slay_len);
		void *next = movetodata_row(slay);
		for (i=1; i<r; ++i) {
			if (i%2 == 0) {
				if (t[i].type == DICT_PRIMITIVE) {
					if (dict_cmp(data, &t[i], "null")) {
						next = slay_wrap(next, data+t[i-1].start, t[i-1].end - t[i-1].start, NULL, 0, DT_NULL);
					} else if (dict_cmp(data, &t[i], "true")) {
						next = slay_wrap(next, data+t[i-1].start, t[i-1].end - t[i-1].start, NULL, 0, DT_BOOL_T);
					} else if (dict_cmp(data, &t[i], "false")) {
						next = slay_wrap(next, data+t[i-1].start, t[i-1].end - t[i-1].start, NULL, 0, DT_BOOL_F);
					} else {
						next = slay_wrap(next, data+t[i-1].start, t[i-1].end - t[i-1].start, data+t[i].start, t[i].end - t[i].start, DT_INT);
					}
				} else if (t[i].type == DICT_STRING) {
					next = slay_wrap(next, data+t[i-1].start, t[i-1].end - t[i-1].start, data+t[i].start, t[i].end - t[i].start, DT_TEXT);
				} else if (t[i].type == DICT_OBJECT) {
					next = slay_wrap(next, data+t[i-1].start, t[i-1].end - t[i-1].start, data+t[i].start, t[i].end - t[i].start, DT_JSON);
					int x, j = 0;
					for (x=0; x<t[i].size; x++) {
						j += dict_levelcount(&t[i+1+j], 0, 0, NULL);
						j += dict_levelcount(&t[i+1+j], 0, 0, NULL);
					}
					i += j;
				} else if (t[i].type == DICT_ARRAY) {
					next = slay_wrap(next, data+t[i-1].start, t[i-1].end - t[i-1].start, data+t[i].start, t[i].end - t[i].start, DT_JSON);
					int x, j = 0;
					for (x=0; x<t[i].size; x++) {
						j += dict_levelcount(&t[i+1+j], 0, 0, NULL);
					}
					i += j;
				}
			}
		}

	}

	return slay;
}

void *slay_parse_quid(char *data, size_t *slay_len) {
	quid_t pu;
	void *slay = create_row(SCHEMA_FIELD, 1, sizeof(quid_t), slay_len);

	strtoquid(data, &pu);
	void *next = movetodata_row(slay);
	slay_wrap(next, NULL, 0, (void *)&pu, sizeof(quid_t), DT_QUID);

	return slay;
}

void *slay_parse_text(char *data, size_t data_len, size_t *slay_len) {
	void *slay = create_row(SCHEMA_FIELD, 1, data_len, slay_len);

	void *next = movetodata_row(slay);
	slay_wrap(next, NULL, 0, data, data_len, DT_TEXT);
	return slay;
}

void *slay_bool(bool boolean, size_t *slay_len) {
	void *slay = create_row(SCHEMA_FIELD, 1, 0, slay_len);

	void *next = movetodata_row(slay);
	slay_wrap(next, NULL, 0, NULL, 0, boolean ? DT_BOOL_T : DT_BOOL_F);
	return slay;
}

void *slay_null(size_t *slay_len) {
	void *slay = create_row(SCHEMA_FIELD, 1, 0, slay_len);

	void *next = movetodata_row(slay);
	slay_wrap(next, NULL, 0, NULL, 0, DT_NULL);
	return slay;
}

void *slay_char(char *data, size_t *slay_len) {
	void *slay = create_row(SCHEMA_FIELD, 1, 1, slay_len);

	((uint8_t *)data)[1] = '\0';
	void *next = movetodata_row(slay);
	slay_wrap(next, NULL, 0, data, 1, DT_CHAR);
	return slay;
}

void *slay_float(char *data, size_t data_len, size_t *slay_len) {
	void *slay = create_row(SCHEMA_FIELD, 1, data_len, slay_len);

	void *next = movetodata_row(slay);
	slay_wrap(next, NULL, 0, data, data_len, DT_FLOAT);
	return slay;
}

void *slay_integer(char *data, size_t data_len, size_t *slay_len) {
	void *slay = create_row(SCHEMA_FIELD, 1, data_len, slay_len);

	void *next = movetodata_row(slay);
	slay_wrap(next, NULL, 0, data, data_len, DT_INT);
	return slay;
}

void *slay_put_data(char *data, size_t data_len, size_t *len, int *items) {
	void *slay = NULL;
	dstype_t adt = autotype(data, data_len);
	switch (adt) {
		case DT_QUID:
			slay = slay_parse_quid((char *)data, len);
			*items = 1;
			break;
		case DT_JSON:
			slay = slay_parse_object((char *)data, data_len, len, items);
			break;
		case DT_NULL:
			slay = slay_null(len);
			break;
		case DT_CHAR:
			slay = slay_char((char *)data, len);
			*items = 1;
			break;
		case DT_BOOL_F:
			slay = slay_bool(FALSE, len);
			break;
		case DT_BOOL_T:
			slay = slay_bool(TRUE, len);
			break;
		case DT_FLOAT:
			slay = slay_float((char *)data, data_len, len);
			*items = 1;
			break;
		case DT_INT:
			slay = slay_integer((char *)data, data_len, len);
			*items = 1;
			break;
		case DT_TEXT:
			slay = slay_parse_text((char *)data, data_len, len);
			*items = 1;
			break;
	}
	return (void *)slay;
}

json_value *resolv_quid(char *buf, size_t buflen, dstype_t dt) {
	switch (dt) {
		case DT_QUID: {
			dstype_t dt;
			char *xbuf = _db_get((quid_t *)buf, &dt);
			size_t xbuflen = strlen(xbuf);
			return resolv_quid(xbuf, xbuflen, dt);
		}
		case DT_JSON: {
			json_settings settings;
			memset(&settings, 0, sizeof(json_settings));
			settings.value_extra = json_builder_extra;

			char error[128];
			return json_parse_ex(&settings, buf, buflen, error);
		}
		case DT_NULL:
			return json_null_new();
		case DT_BOOL_F:
			return json_boolean_new(FALSE);
		case DT_BOOL_T:
			return json_boolean_new(TRUE);
		case DT_FLOAT: {
			double ld = atof(buf);
			return json_double_new(ld);
		}
		case DT_INT: {
			long int li = atol(buf);
			return json_integer_new(li);
		}
		case DT_CHAR:
		case DT_TEXT: {
			char *rbuf = strrmquote(buf);
			return json_string_new(rbuf);
		}
	}
	return json_null_new();
}

void *slay_get_data(void *data, dstype_t *dt) {
	uint64_t elements;
	schema_t schema;
	void *slay = get_row(data, &schema, &elements);
	void *next = (void *)(((uint8_t *)slay)+sizeof(struct row_slay));

	char *buf = NULL;
	switch (schema) {
		case SCHEMA_FIELD: {
			size_t val_len;
			size_t namelen;
			dstype_t val_dt;

			void *val_data = slay_unwrap(next, NULL, &namelen, &val_len, &val_dt);
			next = next_row(next);
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
					buf = zmalloc(strlen(escdata)+3);
					snprintf(buf, strlen(escdata)+3, "\"%s\"", escdata);
					zfree(escdata);
					break;
				}
				case DT_JSON:
					val_data = (void *)realloc(val_data, val_len+1);
					((uint8_t *)val_data)[val_len] = '\0';
					buf = zstrdup(val_data);
					break;
				case DT_QUID: {
					dstype_t dt;
					buf = _db_get((quid_t *)val_data, &dt);
					break;
				}
			}

			*dt = val_dt;
			zfree(val_data);
			break;
		}
		case SCHEMA_ARRAY: {
			size_t val_len;
			dstype_t val_dt;
			unsigned int i;
			json_value *arr = json_array_new(elements);
			for (i=0; i<elements; ++i) {
				size_t namelen;
				void *val_data = slay_unwrap(next, NULL, &namelen, &val_len, &val_dt);
				next = next_row(next);
				switch (val_dt) {
					case DT_NULL:
						json_array_push(arr, json_null_new());
						zfree(val_data);
						break;
					case DT_BOOL_T:
						json_array_push(arr, json_boolean_new(TRUE));
						zfree(val_data);
						break;
					case DT_BOOL_F:
						json_array_push(arr, json_boolean_new(FALSE));
						zfree(val_data);
						break;
					case DT_INT: {
						val_data = (char *)zrealloc(val_data, val_len+1);
						((char *)val_data)[val_len] = '\0';
						long int li = atol(val_data);
						json_array_push(arr, json_integer_new(li));
						zfree(val_data);
						break;
					}
					case DT_FLOAT: {
						val_data = (char *)zrealloc(val_data, val_len+1);
						((char *)val_data)[val_len] = '\0';
						double ld = atof(val_data);
						json_array_push(arr, json_double_new(ld));
						zfree(val_data);
						break;
					}
					case DT_CHAR:
					case DT_TEXT: {
						val_data = (char *)zrealloc(val_data, val_len+1);
						((char *)val_data)[val_len] = '\0';
						json_array_push(arr, json_string_new(val_data));
						zfree(val_data);
						break;
					}
					case DT_JSON:
						val_data = (char *)zrealloc(val_data, val_len+1);
						((char *)val_data)[val_len] = '\0';
						json_settings settings;
						memset(&settings, 0, sizeof(json_settings));
						settings.value_extra = json_builder_extra;

						char error[128];
						json_value *zarr = json_parse_ex(&settings, val_data, val_len, error);
						json_array_push(arr, zarr);

						zfree(val_data);
						break;
					case DT_QUID: {
						dstype_t dt;
						buf = _db_get((quid_t *)val_data, &dt);
						if (!buf)
							json_array_push(arr, json_null_new());
						else {
							size_t buflen = strlen(buf);
							json_array_push(arr, resolv_quid(buf, buflen, dt));
						}

						zfree(buf);
						zfree(val_data);
						break;
					}
				}
			}

			*dt = DT_JSON;
			buf = malloc(json_measure(arr));
			json_serialize(buf, arr);
			json_builder_free(arr);
			break;
		}
		case SCHEMA_ASOCARRAY: {
			size_t val_len;
			dstype_t val_dt;
			unsigned int i;
			json_value *obj = json_object_new(elements);
			for (i=0; i<elements; ++i) {
				void *name = NULL;
				size_t namelen;
				void *val_data = slay_unwrap(next, &name, &namelen, &val_len, &val_dt);
				next = next_row(next);
				name = (char *)zrealloc(name, namelen+1);
				((char *)name)[namelen] = '\0';

				switch (val_dt) {
					case DT_NULL:
						json_object_push(obj, (char *)name, json_null_new());
						zfree(val_data);
						break;
					case DT_BOOL_T:
						json_object_push(obj, (char *)name, json_boolean_new(TRUE));
						zfree(val_data);
						break;
					case DT_BOOL_F:
						json_object_push(obj, (char *)name, json_boolean_new(FALSE));
						zfree(val_data);
						break;
					case DT_INT: {
						val_data = (char *)zrealloc(val_data, val_len+1);
						((char *)val_data)[val_len] = '\0';
						long int li = atol(val_data);
						json_object_push(obj, (char *)name, json_integer_new(li));
						zfree(val_data);
						break;
					}
					case DT_FLOAT: {
						val_data = (char *)zrealloc(val_data, val_len+1);
						((char *)val_data)[val_len] = '\0';
						double ld = atof(val_data);
						json_object_push(obj, (char *)name, json_double_new(ld));
						zfree(val_data);
						break;
					}
					case DT_CHAR:
					case DT_TEXT: {
						val_data = (char *)zrealloc(val_data, val_len+1);
						((char *)val_data)[val_len] = '\0';
						json_object_push(obj, (char *)name, json_string_new(val_data));
						zfree(val_data);
						break;
					}
					case DT_JSON:
						val_data = (char *)zrealloc(val_data, val_len+1);
						((char *)val_data)[val_len] = '\0';
						json_settings settings;
						memset(&settings, 0, sizeof(json_settings));
						settings.value_extra = json_builder_extra;

						char error[128];
						json_value *zarr = json_parse_ex(&settings, val_data, val_len, error);
						json_object_push(obj, (char *)name, zarr);

						zfree(val_data);
						break;
					case DT_QUID: {
						dstype_t dt;
						buf = _db_get((quid_t *)val_data, &dt);
						if (!buf)
							json_object_push(obj, (char *)name, json_null_new());
						else {
							size_t buflen = strlen(buf);
							json_object_push(obj, (char *)name, resolv_quid(buf, buflen, dt));
						}

						zfree(buf);
						zfree(val_data);
						break;
					}
				}
				zfree(name);
			}

			*dt = DT_JSON;
			buf = malloc(json_measure(obj));
			json_serialize(buf, obj);
			json_builder_free(obj);

			break;
		}
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

uint8_t *slay_wrap(void *arrp, void *name, size_t namelen, void *data, size_t len, dstype_t dt) {
	size_t data_size = len;
	if (isdata(dt))
		data_size = 0;

	struct value_slay *slay = (struct value_slay *)arrp;
	slay->val_type = dt;
	slay->size = data_size;
	slay->namesize = namelen;

	uint8_t *dest = ((uint8_t *)slay)+sizeof(struct value_slay);
	if (slay->size) {
		memcpy(dest, data, data_size);
	}

	if (slay->namesize) {
		uint8_t *namedest = dest+slay->size;
		memcpy(namedest, name, namelen);
	}

	return dest+slay->size+namelen;
}

void *slay_unwrap(void *arrp, void **name, size_t *namelen, size_t *len, dstype_t *dt) {
	struct value_slay *slay = (struct value_slay *)arrp;
	void *data = NULL;

	if (slay->size) {
		void *src = ((uint8_t *)arrp)+sizeof(struct value_slay);
		data = zmalloc(slay->size);
		memcpy(data, src, slay->size);
	}

	if (slay->namesize) {
		assert(!*name);
		void *src = ((uint8_t *)arrp)+sizeof(struct value_slay)+slay->size;
		*name = zmalloc(slay->namesize);
		memcpy(*name, src, slay->namesize);
	}

	*dt = slay->val_type;
	*len = slay->size;
	*namelen = slay->namesize;

	return data;
}
