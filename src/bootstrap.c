#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "quid.h"
#include "engine.h"
#include "bootstrap.h"

static const char skey0[] = "{00000000-0000-0000-0000-000000000000}";

void bootstrap(struct btree *btree) {

	struct quid key0;
	strtoquid(skey0, &key0);
	size_t len;
	void *data = btree_get(btree, &key0, &len);
	if (data && !strcmp(data, BS_MAGIC))
		goto done;

	const char data0[] = BS_MAGIC;
	if(btree_insert(btree, &key0, data0, strlen(data0))<0)
		fprintf(stderr, "bootstrap: Insert failed\n");

	const char skey1[] = "{00000000-00c1-a150-ab68-01cafaa7a081}";
	struct quid key1;
	strtoquid(skey1, &key1);
	const char data1[] = "{\"pre\":\"_init\",\"description\":\"bootstrap\"}";
	if(btree_insert(btree, &key1, data1, strlen(data1))<0)
		fprintf(stderr, "bootstrap: Insert failed\n");

done:
	free(data);
}
