#include <stdlib.h>
#include <string.h>

#include <error.h>
#include "zmalloc.h"
#include "quid.h"
#include "core.h"
#include "engine.h"
#include "btree.h"
#include "index.h"

static marshall_t *get_record(quid_t *key) {
	struct engine *engine = get_current_engine();

	size_t len;
	struct metadata meta;
	uint64_t offset = engine_get(engine, key, &meta);
	if (iserror()) {
		error_clear();
		return NULL;
	}

	void *data = get_data_block(engine, offset, &len);
	if (!data)
		return NULL;

	marshall_t *dataobj = slay_get(data, NULL, TRUE);
	zfree(data);
	return dataobj;
}

/*
 * Create btree index on table structure
 */
int index_btree_create_table(char *squid, const char *element, marshall_t *marshall, index_result_t *result) {
	btree_t index;

	btree_init(&index, squid);
	btree_set_unique(&index, FALSE);

	for (unsigned int i = 0; i < marshall->size; ++i) {
		quid_t key;
		struct metadata meta;
		strtoquid(marshall->child[i]->data, &key);

		marshall_t *rowobj = get_record(&key);
		if (!rowobj)
			continue;

		uint64_t offset = engine_get(get_current_engine(), &key, &meta);
		for (unsigned int j = 0; j < rowobj->size; ++j) {
			if (!rowobj->child[j])
				continue;

			if (!rowobj->child[j]->name_len)
				continue;

			if (!strcmp(rowobj->child[j]->name, element)) {
				size_t value_len;
				char *value = marshall_strdata(rowobj->child[j], &value_len);
				btree_insert(&index, value, value_len, offset);
				result->index_elements++;
			}
		}
		marshall_free(rowobj);
	}

	btree_close(&index);
	return 0;
}

/*
 * Create btree index on set
 */
int index_btree_create_set(char *squid, const char *element, marshall_t *marshall, index_result_t *result) {
	btree_t index;

	if (!strisdigit((char *)element) || element[0] == '-') {
		error_throw("888d28dff048", "Operation expects an positive index given");
		return -1;
	}

	btree_init(&index, squid);
	btree_set_unique(&index, FALSE);

	long int array_index = atol(element);
	for (unsigned int i = 0; i < marshall->size; ++i) {
		quid_t key;
		struct metadata meta;
		strtoquid(marshall->child[i]->data, &key);

		marshall_t *rowobj = get_record(&key);
		if (!rowobj)
			continue;

		uint64_t offset = engine_get(get_current_engine(), &key, &meta);
		if (array_index <= (rowobj->size - 1)) {
			size_t value_len;
			if (!rowobj->child[array_index])
				continue;

			char *value = marshall_strdata(rowobj->child[array_index], &value_len);
			btree_insert(&index, value, value_len, offset);
			result->index_elements++;
		}
		marshall_free(rowobj);
	}

	btree_close(&index);
	return 0;
}


marshall_t *index_btree_all(quid_t *key, bool descent) {
	char squid[QUID_LENGTH + 1];
	btree_t index;

	quidtostr(squid, key);

	btree_init(&index, squid);

	vector_t *rskv = btree_get_all(&index);

	marshall_t *marshall = (marshall_t *)tree_zcalloc(1, sizeof(marshall_t), NULL);
	marshall->child = (marshall_t **)tree_zcalloc(rskv->size, sizeof(marshall_t *), marshall);
	marshall->type = descent ? MTYPE_OBJECT : MTYPE_ARRAY;

	for (unsigned int i = 0; i < rskv->size; ++i) {
		index_keyval_t *kv = (index_keyval_t *)(vector_at(rskv, i));

		size_t len;
		char *data = get_data_block(get_current_engine(), kv->value, &len);
		if (!data) {
			zfree(kv->key);
			zfree(kv);
			continue;
		}

		marshall_t *dataobj = slay_get(data, marshall, TRUE);
		if (!dataobj) {
			zfree(data);
			return NULL;
		}

		if (descent) {
			marshall->child[marshall->size] = dataobj;
			marshall->child[marshall->size]->name = tree_zstrdup(kv->key, marshall);
			marshall->child[marshall->size]->name_len = kv->key_len;
		} else {
			marshall->child[marshall->size] = (marshall_t *)tree_zcalloc(1, sizeof(marshall_t), marshall);
			marshall->child[marshall->size]->data = tree_zstrdup(kv->key, marshall);
			marshall->child[marshall->size]->data_len = kv->key_len;
			marshall->child[marshall->size]->type = MTYPE_INT;
			marshall->child[marshall->size]->size = 1;
		}
		marshall->size++;

		zfree(data);
		zfree(kv->key);
		zfree(kv);
	}

	vector_free(rskv);
	btree_close(&index);

	return marshall;
}
