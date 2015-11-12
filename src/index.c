#include <stdlib.h>
#include <string.h>

#include <error.h>
#include "zmalloc.h"
#include "quid.h"
#include "core.h"
#include "slay.h"
#include "engine.h"
#include "btree.h"
#include "index.h"

/*
 * Create btree index
 */
int index_btree_create(const char *element, marshall_t *marshall, schema_t schema, index_result_t *result) {
	char squid[QUID_LENGTH + 1];
	btree_t index;

	quid_create(&result->index);
	quidtostr(squid, &result->index);

	if (schema == SCHEMA_TABLE) {

		btree_init(&index, squid);
		btree_set_unique(&index, FALSE);

		for (unsigned int i = 0; i < marshall->size; ++i) {
			marshall_t *rowobj = raw_db_get(marshall->child[i]->data, NULL);
			uint64_t offset = db_get_offset(marshall->child[i]->data);
			for (unsigned int j = 0; j < rowobj->size; ++j) {
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
	} else if (schema == SCHEMA_SET) {
		if (!strisdigit((char *)element)) {
			error_throw("888d28dff048", "Operation expexts an index given");
			return -1;
		}

		btree_init(&index, squid);
		btree_set_unique(&index, FALSE);

		long int array_index = atol(element);

		for (unsigned int i = 0; i < marshall->size; ++i) {
			marshall_t *rowobj = raw_db_get(marshall->child[i]->data, NULL);
			uint64_t offset = db_get_offset(marshall->child[i]->data);

			if (array_index <= (rowobj->size - 1)) {
				size_t value_len;
				char *value = marshall_strdata(rowobj->child[array_index], &value_len);
				btree_insert(&index, value, value_len, offset);
				result->index_elements++;
			}
			marshall_free(rowobj);
		}

		btree_close(&index);
	} else {
		error_throw("ece28bc980db", "Invalid schema");
		return -1;
	}

	return 0;
}

marshall_t *index_btree_all(quid_t *key) {
	char squid[QUID_LENGTH + 1];
	btree_t index;

	quidtostr(squid, key);

	btree_init(&index, squid);

	vector_t *rskv = btree_get_all(&index);

	marshall_t *marshall = (marshall_t *)tree_zcalloc(1, sizeof(marshall_t), NULL);
	marshall->child = (marshall_t **)tree_zcalloc(rskv->size, sizeof(marshall_t *), marshall);
	marshall->type = MTYPE_OBJECT;

	for (unsigned int i = 0; i < rskv->size; ++i) {
		index_keyval_t *kv = (index_keyval_t *)(vector_at(rskv, i));

		size_t len;
		char *data = db_get_data(kv->value, &len);
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

		marshall->child[marshall->size] = dataobj;
		marshall->child[marshall->size]->name = tree_zstrdup(kv->key, marshall);
		marshall->child[marshall->size]->name_len = kv->key_len;
		marshall->size++;

		zfree(data);
		zfree(kv->key);
		zfree(kv);
	}

	vector_free(rskv);
	btree_close(&index);

	return marshall;
}