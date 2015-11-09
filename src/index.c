#include <stdlib.h>
#include <string.h>

#include "zmalloc.h"
#include "quid.h"
#include "core.h"
#include "slay.h"
#include "engine.h"
#include "btree.h"
#include "index.h"

int index_create_btree(const char *element, marshall_t *marshall, schema_t schema, index_result_t *result) {
	char squid[QUID_LENGTH + 1];
	btree_t index;

	quid_create(&result->index);
	quidtostr(squid, &result->index);

	if (schema == SCHEMA_TABLE) {

		btree_init(&index, squid);

		for (unsigned int i = 0; i < marshall->size; ++i) {
			marshall_t *rowobj = raw_db_get(marshall->child[i]->data, NULL);
			uint64_t offset = db_get_offset(marshall->child[i]->data);
			for (unsigned int j = 0; j < rowobj->size; ++j) {
				if (!strcmp(rowobj->child[j]->name, element)) {
					size_t value_len;
					char *value = marshall_strdata(rowobj->child[j]->data, &value_len);
					btree_insert(&index, value, value_len, offset);
					result->index_elements++;
				}
			}
			marshall_free(rowobj);
		}

		btree_print(&index);
		btree_close(&index);
	} else if (schema == SCHEMA_SET) {
		if (!strisdigit((char *)element)) {
			//TODO throw err
			return -1;
		}

		btree_init(&index, squid);

		long int array_index = atol(element);

		for (unsigned int i = 0; i < marshall->size; ++i) {
			marshall_t *rowobj = raw_db_get(marshall->child[i]->data, NULL);
			uint64_t offset = db_get_offset(marshall->child[i]->data);

			if (array_index > (rowobj->size - 1))
				//TODO note skipped keys
				puts("Warn: element out of array bounds");
			else {
				size_t value_len;
				char *value = marshall_strdata(rowobj->child[array_index], &value_len);
				btree_insert(&index, value, value_len, offset);
				result->index_elements++;
			}
			marshall_free(rowobj);
		}

		btree_print(&index);
		btree_close(&index);
	} else {
		//TODO throw err
		return -1;
	}

	return 0;
}
