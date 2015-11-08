#include <stdlib.h>
#include <string.h>

#include "zmalloc.h"
#include "quid.h"
#include "core.h"
#include "slay.h"
#include "engine.h"
#include "btree.h"

int index_create_btree(const char *key, marshall_t *marshall, schema_t schema) {
	char squid[QUID_LENGTH + 1];

	if (schema == SCHEMA_TABLE) {
		quid_t idxkey_name;
		quid_create(&idxkey_name);
		quidtostr(squid, &idxkey_name);

		index_t index;
		btree_init(&index, squid);

		for (unsigned int i = 0; i < marshall->size; ++i) {
			marshall_t *rowobj = raw_db_get(marshall->child[i]->data, NULL);
			uint64_t offset = db_get_offset(marshall->child[i]->data);
			for (unsigned int j = 0; j < rowobj->size; ++j) {
				if (!strcmp(rowobj->child[j]->name, key)) {
					btree_insert(&index, rowobj->child[j]->data, rowobj->child[j]->data_len, offset);
				}
			}
			marshall_free(rowobj);
		}

		btree_print(&index);
		btree_close(&index);
	} else if (schema == SCHEMA_SET) {
		puts("deal with set");
	}

	return 0;
}
