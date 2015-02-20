#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "quid.h"
#include "engine.h"
#include "bootstrap.h"

#define BS_MAGIC "__zero()__"

struct quid key0;
struct microdata md;

void bootstrap(struct btree *btree) {
	memset(&key0, 0, sizeof(struct quid));
	memset(&md, 0, sizeof(struct microdata));

	size_t len;
	void *data = btree_get(btree, &key0, &len);
	if (data && !strcmp(data, BS_MAGIC))
		goto done;

	const char data0[] = BS_MAGIC;
	if(btree_insert(btree, &key0, data0, strlen(data0))<0)
		fprintf(stderr, "bootstrap: Insert failed\n");

	md.importance = MD_IMPORTANT_CRITICAL;
	md.flag = MD_FLAG_STRICT;
	md.syslock = 1;
	md.exec = 1;
	md.freeze = 0;
	if (btree_meta(btree, &key0, &md)<0)
		fprintf(stderr, "bootstrap: Update meta failed\n");

	const char skey1[] = "{00000000-00c1-a150-ab68-01cafaa7a081}";
	struct quid key1;
	strtoquid(skey1, &key1);
	const char data1[] = "{\"pre\":\"_init\",\"description\":\"bootstrap\"}";
	if(btree_insert(btree, &key1, data1, strlen(data1))<0)
		fprintf(stderr, "bootstrap: Insert failed\n");

	md.importance = MD_IMPORTANT_LEVEL2;
	md.flag = MD_FLAG_STRICT;
	md.syslock = 1;
	md.exec = 0;
	md.freeze = 0;
	if (btree_meta(btree, &key1, &md)<0)
		fprintf(stderr, "bootstrap: Update meta failed\n");

done:
	free(data);
}
