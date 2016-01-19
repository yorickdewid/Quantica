#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <log.h>
#include <error.h>
#include "slay_marshall.h"
#include "quid.h"
#include "dict.h"
#include "marshall.h"
#include "engine.h"
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
	struct row_slay *row = (struct row_slay *)zcalloc(1, sizeof(struct row_slay) + (el * sizeof(struct value_slay)) + data_len);
	row->elements = el;
	row->schema = schema;
	*len = sizeof(struct row_slay) + (el * sizeof(struct value_slay)) + data_len;
	return (void *)row;
}

static void *update_row(void *val, schema_t schema, unsigned int el) {
	zassert(el >= 1);
	struct row_slay *row = (struct row_slay *)val;
	row->elements = el;
	row->schema = schema;
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
		data = zcalloc(slay->size, sizeof(char));
		memcpy(data, src, slay->size);
	}

	if (slay->namesize) {
		void *src = ((uint8_t *)arrp) + sizeof(struct value_slay) + slay->size;
		*name = zcalloc(slay->namesize, sizeof(char));
		memcpy(*name, src, slay->namesize);
	}

	*dt = slay->val_type;
	*len = slay->size;
	*namelen = slay->namesize;

	return data;
}

void *slay_put(base_t *base, marshall_t *marshall, size_t *len, slay_result_t *rs) {
	void *slay = NULL;

	if (marshall_type_hasdata(marshall->type)) {
		rs->schema = SCHEMA_FIELD;
		rs->items = 1;

		slay = create_row(rs->schema, 1, marshall->data_len, len);
		slay_wrap(movetodata_row(slay), NULL, 0, marshall->data, marshall->data_len, marshall->type);
		return slay;
	} else if (marshall_type_hasdescent(marshall->type)) {
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
				data_len += marshall->child[i]->data_len;
			if (marshall->child[i]->name)
				data_len += marshall->child[i]->name_len;
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
				name_len = marshall->child[i]->name_len;
			}

			if (marshall_type_hasdata(marshall->child[i]->type)) {
				next = slay_wrap(next, name, name_len, marshall->child[i]->data, marshall->child[i]->data_len, marshall->child[i]->type);
			} else if (marshall_type_hasdescent(marshall->child[i]->type)) {
				size_t _len = 0;
				slay_result_t _rs;
				char squid[QUID_LENGTH + 1];
				void *_slay = slay_put(base, marshall->child[i], &_len, &_rs);

				/* Insert subgroup in database */
				quid_t key;
				quid_create(&key);
				quidtostr(squid, &key);

				if (engine_insert_data(base, &key, _slay, _len) < 0) {
					zfree(_slay);
					continue;
				}
				zfree(_slay);

				next = slay_wrap(next, name, name_len, squid, QUID_LENGTH, MTYPE_QUID);
			} else {
				next = slay_wrap(next, name, name_len, NULL, 0, marshall->child[i]->type);
			}
		}
		return slay;
	} else {
		rs->schema = SCHEMA_FIELD;
		rs->items = 1;

		slay = create_row(rs->schema, 1, 0, len);
		slay_wrap(movetodata_row(slay), NULL, 0, NULL, 0, marshall->type);
		return slay;
	}
	return slay;
}

void slay_update_row(void *data, slay_result_t *rs) {
	update_row(data, rs->schema, rs->items);
}

static marshall_t *get_child_record(base_t *base, char *quid, void *parent) {
	quid_t key;
	strtoquid(quid, &key);

	size_t len;
	struct metadata meta;
	uint64_t offset = engine_get(base, &key, &meta);
	if (!offset)
		return NULL;

	void *data = get_data_block(base, offset, &len);
	if (!data)
		return NULL;

	marshall_t *dataobj = slay_get(base, data, parent, TRUE);
	zfree(data);
	return dataobj;
}

marshall_t *slay_get(base_t *base, void *data, void *parent, bool descent) {
	marshall_t *marshall = NULL;
	uint64_t elements;
	schema_t schema;
	void *name = NULL;
	size_t namelen, val_len;
	marshall_type_t val_dt;

	void *slay = get_row(data, &schema, &elements);
	void *next = movetodata_row(slay);

	switch (schema) {
		case SCHEMA_FIELD: {
			void *val_data = slay_unwrap(next, NULL, &namelen, &val_len, &val_dt);

			//TODO ugly
			if (val_data) {
				val_data = (void *)zrealloc(val_data, val_len + 1);
				((char *)val_data)[val_len] = '\0';
			}

			if (val_dt == MTYPE_QUID && descent) {
				marshall = get_child_record(base, val_data, NULL);
				if (!marshall) {
					marshall = (marshall_t *)tree_zcalloc(1, sizeof(marshall_t), parent);
					marshall->type = MTYPE_NULL;
					marshall->size = 1;
				}
				zfree(val_data);
				error_clear();
				return marshall;
			}

			marshall = (marshall_t *)tree_zcalloc(1, sizeof(marshall_t), parent);
			marshall->type = val_dt;
			marshall->data = tree_zstrndup(val_data, val_len, marshall);
			marshall->data_len = val_len;
			marshall->size = 1;
			if (val_data)
				zfree(val_data);
			break;
		}
		case SCHEMA_ARRAY: {
			marshall = (marshall_t *)tree_zcalloc(1, sizeof(marshall_t), parent);
			marshall->child = (marshall_t **)tree_zcalloc(elements, sizeof(marshall_t *), marshall);
			marshall->type = MTYPE_ARRAY;

			for (unsigned int i = 0; i < elements; ++i) {
				void *val_data = slay_unwrap(next, NULL, &namelen, &val_len, &val_dt);
				next = next_row(next);

				//TODO ugly
				if (val_data) {
					val_data = (void *)zrealloc(val_data, val_len + 1);
					((char *)val_data)[val_len] = '\0';
				}

				if (val_dt == MTYPE_QUID && descent) {
					marshall->child[marshall->size] = get_child_record(base, val_data, marshall);
					if (!marshall->child[marshall->size]) {
						marshall->child[marshall->size] = (marshall_t *)tree_zcalloc(1, sizeof(marshall_t), marshall);
						marshall->child[marshall->size]->type = MTYPE_NULL;
						marshall->child[marshall->size]->size = 1;
					}
					marshall->size++;
					zfree(val_data);
					error_clear();
					continue;
				}

				marshall->child[marshall->size] = (marshall_t *)tree_zcalloc(1, sizeof(marshall_t), marshall);
				marshall->child[marshall->size]->type = val_dt;
				marshall->child[marshall->size]->data = tree_zstrndup(val_data, val_len, marshall);
				marshall->child[marshall->size]->data_len = val_len;
				marshall->child[marshall->size]->size = 1;
				marshall->size++;
				if (val_data)
					zfree(val_data);
			}
			break;
		}
		case SCHEMA_OBJECT: {
			marshall = (marshall_t *)tree_zcalloc(1, sizeof(marshall_t), parent);
			marshall->child = (marshall_t **)tree_zcalloc(elements, sizeof(marshall_t *), marshall);
			marshall->type = MTYPE_OBJECT;

			for (unsigned int i = 0; i < elements; ++i) {
				void *val_data = slay_unwrap(next, &name, &namelen, &val_len, &val_dt);
				next = next_row(next);

				//TODO ugly
				if (val_data) {
					val_data = (void *)zrealloc(val_data, val_len + 1);
					((char *)val_data)[val_len] = '\0';
				}

				if (val_dt == MTYPE_QUID && descent) {
					marshall->child[marshall->size] = get_child_record(base, val_data, marshall);
					if (!marshall->child[marshall->size]) {
						marshall->child[marshall->size] = (marshall_t *)tree_zcalloc(1, sizeof(marshall_t), marshall);
						marshall->child[marshall->size]->type = MTYPE_NULL;
					}
					marshall->child[marshall->size]->name = tree_zstrndup(name, namelen, marshall);
					marshall->child[marshall->size]->name_len = namelen;
					marshall->size++;
					zfree(name);
					zfree(val_data);
					error_clear();
					continue;
				}

				marshall->child[marshall->size] = (marshall_t *)tree_zcalloc(1, sizeof(marshall_t), marshall);
				marshall->child[marshall->size]->name = tree_zstrndup(name, namelen, marshall);
				marshall->child[marshall->size]->name_len = namelen;
				marshall->child[marshall->size]->type = val_dt;
				marshall->child[marshall->size]->data = tree_zstrndup(val_data, val_len, marshall);
				marshall->child[marshall->size]->data_len = val_len;
				marshall->child[marshall->size]->size = 1;
				marshall->size++;
				if (val_data)
					zfree(val_data);
				zfree(name);
			}
			break;
		}
		case SCHEMA_TABLE: {
			marshall = (marshall_t *)tree_zcalloc(1, sizeof(marshall_t), parent);
			marshall->child = (marshall_t **)tree_zcalloc(elements, sizeof(marshall_t *), marshall);
			marshall->type = MTYPE_ARRAY;

			for (unsigned int i = 0; i < elements; ++i) {
				void *val_data = slay_unwrap(next, NULL, &namelen, &val_len, &val_dt);
				next = next_row(next);

				marshall->child[marshall->size] = (marshall_t *)tree_zcalloc(1, sizeof(marshall_t), marshall);

				//TODO ugly
				val_data = (void *)zrealloc(val_data, val_len + 1);
				((char *)val_data)[val_len] = '\0';

				if (descent) {
					marshall->child[marshall->size] = get_child_record(base, val_data, marshall);
				} else {
					marshall->child[marshall->size] = (marshall_t *)tree_zcalloc(1, sizeof(marshall_t), marshall);
					marshall->child[marshall->size]->type = val_dt;
					marshall->child[marshall->size]->data = tree_zstrndup(val_data, val_len, marshall);
					marshall->child[marshall->size]->data_len = val_len;
					marshall->child[marshall->size]->size = 1;
				}

				marshall->size++;
				zfree(val_data);
				error_clear();
			}
			break;
		}
		case SCHEMA_SET: {
			marshall = (marshall_t *)tree_zcalloc(1, sizeof(marshall_t), parent);
			marshall->child = (marshall_t **)tree_zcalloc(elements, sizeof(marshall_t *), marshall);
			marshall->type = MTYPE_OBJECT;

			for (unsigned int i = 0; i < elements; ++i) {
				void *val_data = slay_unwrap(next, &name, &namelen, &val_len, &val_dt);
				next = next_row(next);

				marshall->child[marshall->size] = (marshall_t *)tree_zcalloc(1, sizeof(marshall_t), marshall);

				//TODO ugly
				val_data = (void *)zrealloc(val_data, val_len + 1);
				((char *)val_data)[val_len] = '\0';

				if (descent) {
					marshall->child[marshall->size] = get_child_record(base, val_data, marshall);
				} else {
					marshall->child[marshall->size] = (marshall_t *)tree_zcalloc(1, sizeof(marshall_t), marshall);
					marshall->child[marshall->size]->type = val_dt;
					marshall->child[marshall->size]->data = tree_zstrndup(val_data, val_len, marshall);
					marshall->child[marshall->size]->data_len = val_len;
					marshall->child[marshall->size]->size = 1;
				}

				if (name) {
					marshall->child[marshall->size]->name = tree_zstrndup(name, namelen, marshall);
					marshall->child[marshall->size]->name_len = namelen;
					marshall->size++;

					zfree(name);
					zfree(val_data);
				}
				error_clear();
			}
			break;
		}
		default:
			error_throw("dcb796d620d1", "Unknown datastructure");
			break;
	}

	return marshall;
}

marshall_type_t slay_get_type(void *data) {
	uint64_t elements;
	schema_t schema;
	void *name = NULL;
	size_t namelen, val_len;
	marshall_type_t val_dt;

	void *slay = get_row(data, &schema, &elements);

	switch (schema) {
		/* Only for single item can the type be determined */
		case SCHEMA_FIELD: {
			void *val_data = slay_unwrap(movetodata_row(slay), &name, &namelen, &val_len, &val_dt);
			if (val_data)
				zfree(val_data);
			if (name)
				zfree(name);
			break;
		}
		case SCHEMA_OBJECT:
		case SCHEMA_SET:
			val_dt = MTYPE_OBJECT;
			break;
		case SCHEMA_ARRAY:
		case SCHEMA_TABLE:
			val_dt = MTYPE_ARRAY;
			break;
		default:
			val_dt = MTYPE_NULL;
	}

	return val_dt;
}

schema_t slay_get_schema(void *data) {
	uint64_t elements;
	schema_t schema;

	get_row(data, &schema, &elements);
	return schema;
}

char *slay_get_strschema(void *data) {
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
}
