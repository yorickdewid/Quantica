#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <log.h>
#include <error.h>
#include "slay.h"
#include "quid.h"
#include "dict.h"
#include "marshall.h"
#include "json_encode.h"
#include "core.h"
#include "zmalloc.h"

#define VECTOR_SIZE	1024

#define movetodata_row(row) (void *)(((uint8_t *)row)+sizeof(struct row_slay))
#define next_row(next) (void *)(((uint8_t *)next)+sizeof(struct value_slay)+val_len+namelen)

int object_descent_count(marshall_t *obj) {
	int cnt = 0;
	for (unsigned int i = 0; i < obj->size; ++i) {
		if (obj->child[i]->type == MTYPE_OBJECT || obj->child[i]->type == MTYPE_ARRAY)
			cnt++;
	}
	return cnt;
}

static void *create_row(schema_t schema, uint64_t el, size_t data_len, size_t *len) {
	zassert(el >= 1);
	struct row_slay *row = zcalloc(1, sizeof(struct row_slay) + (el * sizeof(struct value_slay)) + data_len);
	row->elements = el;
	row->schema = schema;
	*len = sizeof(struct row_slay) + (el * sizeof(struct value_slay)) + data_len;
	return (void *)row;
}

static void *get_row(void *val, schema_t *schema, uint64_t *el) {
	struct row_slay *row = (struct row_slay *)val;
	*el = row->elements;
	*schema = row->schema;
	return (void *)row;
}

static uint8_t *slay_wrap(void *arrp, void *name, size_t namelen, void *data, size_t len, marshall_type_t dt) {
	size_t data_size = len;
	struct value_slay *slay = (struct value_slay *)arrp;
	slay->val_type = dt;
	slay->size = data_size;
	slay->namesize = namelen;

	uint8_t *dest = ((uint8_t *)slay) + sizeof(struct value_slay);
	if (slay->size) {
		memcpy(dest, data, data_size);
	}

	if (slay->namesize) {
		uint8_t *namedest = dest + slay->size;
		memcpy(namedest, name, namelen);
	}

	return (uint8_t *)dest + slay->size + namelen;
}

static void *slay_unwrap(void *arrp, void **name, size_t *namelen, size_t *len, marshall_type_t *dt) {
	struct value_slay *slay = (struct value_slay *)arrp;
	void *data = NULL;

	if (slay->size) {
		void *src = ((uint8_t *)arrp) + sizeof(struct value_slay);
		data = zmalloc(slay->size);
		memcpy(data, src, slay->size);
	}

	if (slay->namesize) {
		void *src = ((uint8_t *)arrp) + sizeof(struct value_slay) + slay->size;
		*name = zmalloc(slay->namesize);
		memcpy(*name, src, slay->namesize);
	}

	*dt = slay->val_type;
	*len = slay->size;
	*namelen = slay->namesize;

	return data;
}

//TODO rewrite switches
void *slay_put(marshall_t *marshall, size_t *len, slay_result_t *rs) {
	void *slay = NULL;

	switch (marshall->type) {
		case MTYPE_NULL:
		case MTYPE_TRUE:
		case MTYPE_FALSE: {
			rs->schema = SCHEMA_FIELD;
			rs->items = 1;

			slay = create_row(rs->schema, 1, 0, len);
			slay_wrap(movetodata_row(slay), NULL, 0, NULL, 0, marshall->type);
			return slay;
		}
		case MTYPE_INT:
		case MTYPE_FLOAT:
		case MTYPE_STRING:
		case MTYPE_QUID: {
			rs->schema = SCHEMA_FIELD;
			rs->items = 1;

			slay = create_row(rs->schema, 1, strlen(marshall->data), len);
			slay_wrap(movetodata_row(slay), NULL, 0, marshall->data, strlen(marshall->data), marshall->type);
			return slay;
		}
		case MTYPE_ARRAY:
		case MTYPE_OBJECT: {
			if (marshall->type == MTYPE_ARRAY) {
				rs->schema = SCHEMA_ARRAY;
			} else if (marshall->type == MTYPE_OBJECT)
				rs->schema = SCHEMA_OBJECT;

			size_t desccnt = object_descent_count(marshall);
			rs->items = marshall_get_count(marshall, 1, 0) - 1;

			/* Estimate the total bytes for the object */
			size_t data_len = desccnt * QUID_LENGTH;
			for (unsigned int i = 0; i < marshall->size; ++i) {
				if (marshall->child[i]->data)
					data_len += strlen(marshall->child[i]->data);
				if (marshall->child[i]->name)
					data_len += strlen(marshall->child[i]->name);
			}

			/* Does the structure qualify for table/set */
			if (desccnt == rs->items && rs->items > 1) {
				if (rs->schema == SCHEMA_ARRAY)
					rs->schema = SCHEMA_TABLE;
				else
					rs->schema = SCHEMA_SET;
			}

			slay = create_row(rs->schema, rs->items, data_len, len);
			void *next = movetodata_row(slay);
			for (unsigned int i = 0; i < marshall->size; ++i) {
				char *name = NULL;
				size_t name_len = 0;
				if (marshall->child[i]->name) {
					name = marshall->child[i]->name;
					name_len = strlen(marshall->child[i]->name);
				}

				switch (marshall->child[i]->type) {
					case MTYPE_NULL:
					case MTYPE_TRUE:
					case MTYPE_FALSE: {
						next = slay_wrap(next, name, name_len, NULL, 0, marshall->child[i]->type);
						break;
					}
					case MTYPE_INT:
					case MTYPE_FLOAT:
					case MTYPE_STRING:
					case MTYPE_QUID: {
						next = slay_wrap(next, name, name_len, marshall->child[i]->data, strlen(marshall->child[i]->data), marshall->child[i]->type);
						break;
					}
					case MTYPE_ARRAY:
					case MTYPE_OBJECT: {
						size_t _len = 0;
						slay_result_t _rs;
						char squid[QUID_LENGTH + 1];
						void *_slay = slay_put(marshall->child[i], &_len, &_rs);
						raw_db_put(squid, _slay, _len);
						next = slay_wrap(next, name, name_len, squid, QUID_LENGTH, MTYPE_QUID);
						break;
					}
					default:
						break;
				}
			}

			return slay;
		}
		default:
			//TODO: throw error
			break;
	}

	return slay;
}

marshall_t *slay_get(void *data, void *parent) {
	marshall_t *marshall = NULL;
	uint64_t elements;
	schema_t schema;
	void *name = NULL;
	size_t namelen;
	size_t val_len;
	marshall_type_t val_dt;

	void *slay = get_row(data, &schema, &elements);
	void *next = movetodata_row(slay);

	switch (schema) {
		case SCHEMA_FIELD: {
			void *val_data = slay_unwrap(next, NULL, &namelen, &val_len, &val_dt);
			if (val_data) {
				val_data = (void *)zrealloc(val_data, val_len + 1);
				((uint8_t *)val_data)[val_len] = '\0';
			}

			if (val_dt == MTYPE_QUID) {
				marshall = raw_db_get(val_data, NULL);
				if (!marshall) {
					marshall = (marshall_t *)tree_zmalloc(sizeof(marshall_t), parent);
					memset(marshall, 0, sizeof(marshall_t));
					marshall->type = MTYPE_NULL;
					marshall->size = 1;
				}
				zfree(val_data);
				return marshall;
			}

			marshall = (marshall_t *)tree_zmalloc(sizeof(marshall_t), parent);
			memset(marshall, 0, sizeof(marshall_t));

			marshall->type = val_dt;
			if (val_dt == MTYPE_STRING) {
				char *_tmp = stresc(val_data);
				zfree(val_data);
				val_data = _tmp;
			}

			marshall->data = tree_zstrndup(val_data, val_len, marshall);
			marshall->size = 1;
			if (val_data)
				zfree(val_data);
			break;
		}
		case SCHEMA_ARRAY: {
			marshall = (marshall_t *)tree_zmalloc(sizeof(marshall_t), parent);
			memset(marshall, 0, sizeof(marshall_t));
			marshall->child = (marshall_t **)tree_zmalloc(elements * sizeof(marshall_t *), marshall);
			memset(marshall->child, 0, elements * sizeof(marshall_t *));
			marshall->type = MTYPE_ARRAY;

			for (unsigned int i = 0; i < elements; ++i) {
				void *val_data = slay_unwrap(next, NULL, &namelen, &val_len, &val_dt);
				next = next_row(next);

				if (val_data) {
					val_data = (void *)zrealloc(val_data, val_len + 1);
					((uint8_t *)val_data)[val_len] = '\0';
				}

				if (val_dt == MTYPE_QUID) {
					marshall->child[marshall->size] = raw_db_get(val_data, marshall);
					if (!marshall->child[marshall->size]) {
						marshall->child[marshall->size] = tree_zmalloc(sizeof(marshall_t), marshall);
						memset(marshall->child[marshall->size], 0, sizeof(marshall_t));
						marshall->child[marshall->size]->type = MTYPE_NULL;
						marshall->child[marshall->size]->size = 1;
					}
					marshall->size++;
					zfree(val_data);
					continue;
				}

				marshall->child[marshall->size] = tree_zmalloc(sizeof(marshall_t), marshall);
				memset(marshall->child[marshall->size], 0, sizeof(marshall_t));

				marshall->child[marshall->size]->type = val_dt;
				if (val_dt == MTYPE_STRING) {
					char *_tmp = stresc(val_data);
					zfree(val_data);
					val_data = _tmp;
				}

				marshall->child[marshall->size]->data = tree_zstrndup(val_data, val_len, marshall);
				marshall->size++;
				if (val_data)
					zfree(val_data);
			}
			break;
		}
		case SCHEMA_OBJECT: {
			marshall = (marshall_t *)tree_zmalloc(sizeof(marshall_t), parent);
			memset(marshall, 0, sizeof(marshall_t));
			marshall->child = (marshall_t **)tree_zmalloc(elements * sizeof(marshall_t *), marshall);
			memset(marshall->child, 0, elements * sizeof(marshall_t *));
			marshall->type = MTYPE_OBJECT;

			for (unsigned int i = 0; i < elements; ++i) {
				void *val_data = slay_unwrap(next, &name, &namelen, &val_len, &val_dt);
				next = next_row(next);

				name = (char *)zrealloc(name, namelen + 1);
				((char *)name)[namelen] = '\0';

				if (val_data) {
					val_data = (void *)zrealloc(val_data, val_len + 1);
					((uint8_t *)val_data)[val_len] = '\0';
				}

				if (val_dt == MTYPE_QUID) {
					marshall->child[marshall->size] = raw_db_get(val_data, marshall);
					if (!marshall->child[marshall->size]) {
						marshall->child[marshall->size] = tree_zmalloc(sizeof(marshall_t), marshall);
						memset(marshall->child[marshall->size], 0, sizeof(marshall_t));
						marshall->child[marshall->size]->type = MTYPE_NULL;
						marshall->child[marshall->size]->size = 1;
					}
					marshall->child[marshall->size]->name = tree_zstrdup(name, marshall);
					marshall->size++;
					zfree(name);
					zfree(val_data);
					continue;
				}

				marshall->child[marshall->size] = tree_zmalloc(sizeof(marshall_t), marshall);
				memset(marshall->child[marshall->size], 0, sizeof(marshall_t));
				marshall->child[marshall->size]->name = tree_zstrdup(name, marshall);
				zfree(name);

				marshall->child[marshall->size]->type = val_dt;
				if (val_dt == MTYPE_STRING) {
					char *_tmp = stresc(val_data);
					zfree(val_data);
					val_data = _tmp;
				}

				marshall->child[marshall->size]->data = tree_zstrndup(val_data, val_len, marshall);
				marshall->size++;
				if (val_data)
					zfree(val_data);
			}
			break;
		}
		case SCHEMA_TABLE: {
			marshall = (marshall_t *)tree_zmalloc(sizeof(marshall_t), parent);
			memset(marshall, 0, sizeof(marshall_t));
			marshall->child = (marshall_t **)tree_zmalloc(elements * sizeof(marshall_t *), marshall);
			memset(marshall->child, 0, elements * sizeof(marshall_t *));
			marshall->type = MTYPE_ARRAY;

			for (unsigned int i = 0; i < elements; ++i) {
				void *val_data = slay_unwrap(next, NULL, &namelen, &val_len, &val_dt);
				next = next_row(next);

				marshall->child[marshall->size] = tree_zmalloc(sizeof(marshall_t), marshall);
				memset(marshall->child[marshall->size], 0, sizeof(marshall_t));

				val_data = (void *)zrealloc(val_data, val_len + 1);
				((uint8_t *)val_data)[val_len] = '\0';

				marshall->child[marshall->size] = raw_db_get(val_data, marshall);
				marshall->size++;
				zfree(val_data);
			}
			break;
		}
		case SCHEMA_SET: {
			marshall = (marshall_t *)tree_zmalloc(sizeof(marshall_t), parent);
			memset(marshall, 0, sizeof(marshall_t));
			marshall->child = (marshall_t **)tree_zmalloc(elements * sizeof(marshall_t *), marshall);
			memset(marshall->child, 0, elements * sizeof(marshall_t *));
			marshall->type = MTYPE_OBJECT;

			for (unsigned int i = 0; i < elements; ++i) {
				void *val_data = slay_unwrap(next, &name, &namelen, &val_len, &val_dt);
				next = next_row(next);

				marshall->child[marshall->size] = tree_zmalloc(sizeof(marshall_t), marshall);
				memset(marshall->child[marshall->size], 0, sizeof(marshall_t));

				val_data = (void *)zrealloc(val_data, val_len + 1);
				((uint8_t *)val_data)[val_len] = '\0';

				marshall->child[marshall->size] = raw_db_get(val_data, marshall);
				marshall->child[marshall->size]->name = tree_zstrndup(name, namelen, marshall);
				marshall->size++;
				zfree(name);
				zfree(val_data);
			}
			break;
		}
		default:
			break;
	}

	return marshall;
}

char *slay_get_schema(void *data) {
	uint64_t elements;
	schema_t schema;

	get_row(data, &schema, &elements);
	switch (schema) {
		case SCHEMA_FIELD:
			return "FIELD";
		case SCHEMA_ARRAY:
			return "ARRAY";
		case SCHEMA_OBJECT:
			return "OBJECT";
		case SCHEMA_TABLE:
			return "TABLE";
		case SCHEMA_SET:
			return "SET";
		default:
			return "NULL";
	}
	return "NULL";
}
