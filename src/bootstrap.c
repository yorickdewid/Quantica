#include <stdlib.h>
#include <string.h>

#include <config.h>
#include <common.h>
#include "quid.h"
#include "engine.h"
#include "bootstrap.h"

#define BS_MAGIC "__zero()__"

quid_t key;
struct microdata md;

void bootstrap(struct btree *btree) {
	memset(&key, 0, sizeof(quid_t));
	memset(&md, 0, sizeof(struct microdata));

	/* Verify bootstrap signature */
	size_t len;
	void *rdata = btree_get(btree, &key, &len);
	if (rdata && !strcmp(rdata, BS_MAGIC))
		goto done;

	/* Add bootstrap signature to empty database */
	const char data0[] = BS_MAGIC;
	if(btree_insert(btree, &key, data0, strlen(data0))<0)
		fprintf(stderr, "bootstrap: Insert failed\n");

	md.importance = MD_IMPORTANT_CRITICAL;
	md.syslock = LOCK;
	md.exec = TRUE;
	if (btree_meta(btree, &key, &md)<0)
		fprintf(stderr, "bootstrap: Update meta failed\n");

	/* Add database initial routine */
	const char skey1[] = "{00000000-00c1-a150-0000-000000000080}";
	strtoquid(skey1, &key);
	const char data1[] = "{\"pre\":\"_init\",\"description\":\"bootstrap\"}";
	if(btree_insert(btree, &key, data1, strlen(data1))<0)
		fprintf(stderr, "bootstrap: Insert failed\n");

	md.importance = MD_IMPORTANT_LEVEL2;
	md.syslock = LOCK;
	if (btree_meta(btree, &key, &md)<0)
		fprintf(stderr, "bootstrap: Update meta failed\n");

	/* Add database bootlog routine */
	const char skey2[] = "{00000000-00c1-a150-0000-00000000008a}";
	strtoquid(skey2, &key);
	const char data2[] = "_bootlog()";
	if(btree_insert(btree, &key, data2, strlen(data2))<0)
		fprintf(stderr, "bootstrap: Insert failed\n");

	md.importance = MD_IMPORTANT_LEVEL3;
	md.syslock = LOCK;
	md.exec = TRUE;
	if (btree_meta(btree, &key, &md)<0)
		fprintf(stderr, "bootstrap: Update meta failed\n");

	/* Set backend intercommunication */
	const char skey3[] = "{00000000-00c1-a150-0000-00000000008b}";
	strtoquid(skey3, &key);
	const char data3[] = "1";
	if(btree_insert(btree, &key, data3, strlen(data3))<0)
		fprintf(stderr, "bootstrap: Insert failed\n");

	md.importance = MD_IMPORTANT_LEVEL1;
	md.syslock = LOCK;
	md.type = MD_TYPE_BOOL_TRUE;
	if (btree_meta(btree, &key, &md)<0)
		fprintf(stderr, "bootstrap: Update meta failed\n");

done:
	free(rdata);
}
