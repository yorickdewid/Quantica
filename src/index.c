#include <stdlib.h>
#include <string.h>

#include <error.h>
#include "zmalloc.h"
#include "quid.h"
#include "core.h"
#include "engine.h"
#include "btree.h"
#include "index.h"

static marshall_t *get_record(base_t *base, quid_t *key) {
	size_t len;
	struct metadata meta;
	uint64_t offset = engine_get(base, key, &meta);
	if (iserror()) {
		error_clear();
		return NULL;
	}

	void *data = get_data_block(base, offset, &len);
	if (!data)
		return NULL;

	marshall_t *dataobj = slay_get(base, data, NULL, TRUE);
	zfree(data);
	return dataobj;
}

/*
 * Create btree index on table structure
 */
int index_btree_create_table(base_t *base, const char *element, marshall_t *marshall, index_result_t *result) {
	btree_t index;

	result->offset = btree_create(base, &index);
	btree_set_unique(&index, FALSE);

	for (unsigned int i = 0; i < marshall->size; ++i) {
		quid_t key;
		struct metadata meta;
		strtoquid(marshall->child[i]->data, &key);

		marshall_t *rowobj = get_record(base, &key);
		if (!rowobj)
			continue;

		unsigned long long offset = engine_get(base, &key, &meta);
		for (unsigned int j = 0; j < rowobj->size; ++j) {
			if (!rowobj->child[j])
				continue;

			if (!rowobj->child[j]->name_len)
				continue;

			if (!strcmp(rowobj->child[j]->name, element)) {
				size_t value_len;
				char *value = marshall_strdata(rowobj->child[j], &value_len);
				btree_insert(base, &index, value, value_len, offset);
				result->index_elements++;
				result->element = j;
			}
		}
		marshall_free(rowobj);
	}

	btree_close(base, &index);
	return 0;
}

/*
 * Create btree index on set
 */
int index_btree_create_set(base_t *base, const char *element, marshall_t *marshall, index_result_t *result) {
	btree_t index;

	if (!strisdigit((char *)element) || element[0] == '-') {
		error_throw("888d28dff048", "Operation expects an positive index given");
		return -1;
	}

	result->offset = btree_create(base, &index);
	btree_set_unique(&index, FALSE);

	long int array_index = atol(element);
	for (unsigned int i = 0; i < marshall->size; ++i) {
		quid_t key;
		struct metadata meta;
		strtoquid(marshall->child[i]->data, &key);

		marshall_t *rowobj = get_record(base, &key);
		if (!rowobj)
			continue;

		unsigned long long offset = engine_get(base, &key, &meta);
		if (array_index <= (rowobj->size - 1)) {
			size_t value_len;
			if (!rowobj->child[array_index])
				continue;

			char *value = marshall_strdata(rowobj->child[array_index], &value_len);
			btree_insert(base, &index, value, value_len, offset);
			result->index_elements++;
			result->element = array_index;
		}
		marshall_free(rowobj);
	}

	btree_close(base, &index);
	return 0;
}

marshall_t *index_get(base_t *base, unsigned long long offset, char *key) {
	btree_t index;
	vector_t *result = alloc_vector(DEFAULT_RESULT_SIZE * 10);

	btree_open(base, &index, offset);

	if (btree_get(base, &index, key, &result) != SUCCESS) {
		return NULL;
	}

	marshall_t *marshall = (marshall_t *)tree_zcalloc(1, sizeof(marshall_t), NULL);
	marshall->child = (marshall_t **)tree_zcalloc(result->size, sizeof(marshall_t *), marshall);
	marshall->type = MTYPE_ARRAY;

	for (unsigned int i = 0; i < result->size; ++i) {
		unsigned long long *data_offset = (unsigned long long *)(vector_at(result, i));

		size_t len;
		char *data = get_data_block(base, *data_offset, &len);
		if (!data) {
			zfree(data_offset);
			continue;
		}

		marshall_t *dataobj = slay_get(base, data, marshall, TRUE);
		if (!dataobj) {
			zfree(data);
			zfree(data_offset);
			continue;
		}

		marshall->child[marshall->size] = dataobj;
		marshall->size++;

		zfree(data);
		zfree(data_offset);
	}

	vector_free(result);
	btree_close(base, &index);

	return marshall;
}

int index_add(base_t *base, unsigned long long offset, char *key, unsigned long long valset) {
	btree_t index;

	btree_open(base, &index, offset);
	btree_insert(base, &index, key, strlen(key), valset);
	btree_close(base, &index);

	return 0;
}

int index_delete(base_t *base, unsigned long long offset, char *key) {
	btree_t index;

	btree_open(base, &index, offset);
	btree_delete(base, &index, key);
	btree_close(base, &index);

	return 0;
}

size_t index_btree_count(base_t *base, unsigned long long offset) {
	btree_t index;
	btree_open(base, &index, offset);

	vector_t *rskv = btree_get_all(base, &index);
	size_t count = rskv->size;
	vector_free(rskv);

	return count;
}

marshall_t *index_btree_all(base_t *base, unsigned long long offset, bool descent) {
	btree_t index;

	btree_open(base, &index, offset);

	vector_t *rskv = btree_get_all(base, &index);

	marshall_t *marshall = (marshall_t *)tree_zcalloc(1, sizeof(marshall_t), NULL);
	marshall->child = (marshall_t **)tree_zcalloc(rskv->size, sizeof(marshall_t *), marshall);
	marshall->type = descent ? MTYPE_OBJECT : MTYPE_ARRAY;

	for (unsigned int i = 0; i < rskv->size; ++i) {
		index_keyval_t *kv = (index_keyval_t *)(vector_at(rskv, i));

		size_t len;
		char *data = get_data_block(base, kv->value, &len);
		if (!data) {
			zfree(kv->key);
			zfree(kv);
			continue;
		}

		if (descent) {
			marshall_t *dataobj = slay_get(base, data, marshall, TRUE);
			if (!dataobj) {
				zfree(kv->key);
				zfree(kv);
				zfree(data);
				continue;
			}

			marshall->child[marshall->size] = dataobj;
			marshall->child[marshall->size]->name = tree_zstrdup(kv->key, marshall);
			marshall->child[marshall->size]->name_len = kv->key_len;
		} else {
			marshall->child[marshall->size] = (marshall_t *)tree_zcalloc(1, sizeof(marshall_t), marshall);
			marshall->child[marshall->size]->data = tree_zstrdup(kv->key, marshall);
			marshall->child[marshall->size]->data_len = kv->key_len;
			marshall->child[marshall->size]->type = MTYPE_STRING;
			marshall->child[marshall->size]->size = 1;
		}
		marshall->size++;

		zfree(data);
		zfree(kv->key);
		zfree(kv);
	}

	vector_free(rskv);
	btree_close(base, &index);

	return marshall;
}
