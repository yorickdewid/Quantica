#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "dstype.h"
#include "slay.h"
#include "quid.h"
#include "json_parse.h"
#include "json_encode.h"
#include "core.h"
#include "zmalloc.h"

#define movetodata_row(row) (void *)(((uint8_t *)row)+sizeof(struct row_slay))


#define FATAL(msg)                                        \
  do {                                                    \
    fprintf(stderr,                                       \
            "Fatal error in %s on line %d: %s\n",         \
            __FILE__,                                     \
            __LINE__,                                     \
            msg);                                         \
    fflush(stderr);                                       \
    abort();                                              \
  } while (0)

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

void *slay_parse_object(char *data, size_t data_len, size_t *slay_len, int *items) {
	void *slay = NULL;

	json_value *json = json_parse(data, data_len);
	if (json->type == json_array) {
		slay = create_row(SCHEMA_ARRAY, json->u.array.length, data_len, slay_len);
		void *next = movetodata_row(slay);
		*items = json->u.array.length;

		unsigned int i;
		for(i=0; i<json->u.array.length; ++i) {
			switch (json->u.array.values[i]->type) {
				case json_none:
					next = slay_wrap(next, NULL, 0, NULL, 0, DT_NULL);
					break;
				case json_object: {
					json_value *obj = json_object_new(json->u.array.values[i]->u.object.length);
					unsigned int j = 0;

					for (; j<json->u.array.values[i]->u.object.length; ++j) {
						json_value *val = parse_json(json->u.array.values[i]->u.object.values[j].value);
						json_object_push(obj, json->u.array.values[i]->u.object.values[j].name, val);
					}
					size_t objsz = json_measure(obj);
					char *buf = zmalloc(objsz);
					json_serialize(buf, obj);
					json_builder_free(obj);
					next = slay_wrap(next, NULL, 0, buf, objsz, DT_JSON);
					zfree(buf);
					break;
				}
				case json_array: {
					json_value *arr = json_array_new(json->u.array.values[i]->u.array.length);
					unsigned int j = 0;
					for (; j<json->u.array.values[i]->u.array.length; ++j) {
						json_value *val = parse_json(json->u.array.values[i]->u.array.values[j]);
						json_array_push(arr, val);
					}
					size_t arrsz = json_measure(arr);
					char *buf = zmalloc(arrsz);
					json_serialize(buf, arr);
					json_builder_free(arr);
					next = slay_wrap(next, NULL, 0, buf, arrsz, DT_JSON);
					zfree(buf);
					break;
				}
				case json_integer: {
					char *istr = itoa(json->u.array.values[i]->u.integer);
					next = slay_wrap(next, NULL, 0, istr, strlen(istr), DT_INT);
					break;
				}
				case json_double: {
					char lstr[32];
					sprintf(lstr, "%f", json->u.array.values[i]->u.dbl);
					next = slay_wrap(next, NULL, 0, lstr, strlen(lstr), DT_FLOAT);
					break;
				}
				case json_string:
					if (strquid_format(json->u.array.values[i]->u.string.ptr)>0) {
						quid_t pu;
						strtoquid(json->u.array.values[i]->u.string.ptr, &pu);

						next = slay_wrap(next, NULL, 0, (void *)&pu, sizeof(quid_t), DT_QUID);
						break;
					}
					next = slay_wrap(next, NULL, 0, json->u.array.values[i]->u.string.ptr, json->u.array.values[i]->u.string.length, DT_TEXT);
					break;
				case json_boolean:
					next = slay_wrap(next, NULL, 0, NULL, 0, json->u.array.values[i]->u.boolean ? DT_BOOL_T : DT_BOOL_F);
					break;
				case json_null:
					next = slay_wrap(next, NULL, 0, NULL, 0, DT_NULL);
					break;
			}
		}
	} else {
		slay = create_row(SCHEMA_ASOCARRAY, json->u.object.length, data_len, slay_len);
		void *next = movetodata_row(slay);
		*items = json->u.object.length;

		unsigned int i;
		for(i=0; i<json->u.object.length; ++i) {
			switch (json->u.object.values[i].value->type) {
				case json_none:
					next = slay_wrap(next, json->u.object.values[i].name, json->u.object.values[i].name_length, NULL, 0, DT_NULL);
					break;
				case json_object: {
					json_value *obj = json_object_new(json->u.object.values[i].value->u.object.length);
					unsigned int j = 0;

					for (; j<json->u.object.values[i].value->u.object.length; ++j) {
						json_value *val = parse_json(json->u.object.values[i].value->u.object.values[j].value);
						json_object_push(obj, json->u.object.values[i].value->u.object.values[j].name, val);
					}
					size_t objsz = json_measure(obj);
					char *buf = zmalloc(objsz);
					json_serialize(buf, obj);
					json_builder_free(obj);
					next = slay_wrap(next, json->u.object.values[i].name, json->u.object.values[i].name_length, buf, objsz, DT_JSON);
					zfree(buf);
					break;
				}
				case json_array: {
					json_value *arr = json_array_new(json->u.object.values[i].value->u.array.length);
					unsigned int j = 0;
					for (; j<json->u.object.values[i].value->u.array.length; ++j) {
						json_value *val = parse_json(json->u.object.values[i].value->u.array.values[j]);
						json_array_push(arr, val);
					}
					size_t arrsz = json_measure(arr);
					char *buf = zmalloc(arrsz);
					json_serialize(buf, arr);
					json_builder_free(arr);
					next = slay_wrap(next, json->u.object.values[i].name, json->u.object.values[i].name_length, buf, arrsz, DT_JSON);
					zfree(buf);
					break;
				}
				case json_integer: {
					char *istr = itoa(json->u.object.values[i].value->u.integer);
					next = slay_wrap(next, json->u.object.values[i].name, json->u.object.values[i].name_length, istr, strlen(istr), DT_INT);
					break;
				}
				case json_double: {
					char lstr[32];
					sprintf(lstr, "%f", json->u.object.values[i].value->u.dbl);
					next = slay_wrap(next, json->u.object.values[i].name, json->u.object.values[i].name_length, lstr, strlen(lstr), DT_FLOAT);
					break;
				}
				case json_string:
					if (strquid_format(json->u.object.values[i].value->u.string.ptr)>0) {
						quid_t pu;
						strtoquid(json->u.object.values[i].value->u.string.ptr, &pu);

						next = slay_wrap(next, json->u.object.values[i].name, json->u.object.values[i].name_length, (void *)&pu, sizeof(quid_t), DT_QUID);
						break;
					}
					next = slay_wrap(next, json->u.object.values[i].name, json->u.object.values[i].name_length, json->u.object.values[i].value->u.string.ptr, json->u.object.values[i].value->u.string.length, DT_TEXT);
					break;
				case json_boolean:
					next = slay_wrap(next, json->u.object.values[i].name, json->u.object.values[i].name_length, NULL, 0, json->u.object.values[i].value->u.boolean ? DT_BOOL_T : DT_BOOL_F);
					break;
				case json_null:
					next = slay_wrap(next, json->u.object.values[i].name, json->u.object.values[i].name_length, NULL, 0, DT_NULL);
					break;
			}
		}
	}

	json_value_free(json);
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

void *slay_get_data(void *data) {
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
			next = (void *)(((uint8_t *)next)+sizeof(struct value_slay)+val_len+namelen);
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
			json_value *arr = json_array_new(elements);
			for (i=0; i<elements; ++i) {
				size_t namelen;
				void *val_data = slay_unwrap(next, NULL, &namelen, &val_len, &val_dt);
				next = (void *)(((uint8_t *)next)+sizeof(struct value_slay)+val_len+namelen);
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
						buf = _db_get((quid_t *)val_data);
						json_array_push(arr, json_string_new(buf)); //TODO must be native type
						zfree(buf);
						zfree(val_data);
						break;
					}
				}
			}

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
				next = (void *)(((uint8_t *)next)+sizeof(struct value_slay)+val_len+namelen);
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
						buf = _db_get((quid_t *)val_data);
						json_object_push(obj, (char *)name, json_string_new(buf)); //TODO must be native type
						zfree(buf);
						zfree(val_data);
						break;
					}
				}
				zfree(name);
			}

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
