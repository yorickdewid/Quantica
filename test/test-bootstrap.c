#include <string.h>

#include "test.h"
#include "../src/quid.h"
#include "../src/engine.h"
#include "../src/bootstrap.h"

#define BS_MAGIC "__zero()__"

static void bootstrap_zero(){
    struct btree btree;
    const char fname[] = "test_bootstrap";
	quid_t quid;

	const char squid[] = "{00000000-0000-0000-0000-000000000000}";
	strtoquid(squid, &quid);
	btree_init(&btree, fname);
	bootstrap(&btree);

	size_t len;
	void *data = btree_get(&btree, &quid, &len);
	ASSERT(data);
	ASSERT(!strncmp(data, BS_MAGIC, len));

	btree_close(&btree);
	btree_purge(fname);
}

static void bootstrap_init(){
	struct btree btree;
	const char fname[] = "test_bootstrap2";
	quid_t quid;

	const char squid[] = "{00000000-00c1-a150-0000-000000000080}";
	const char data[] = "{\"pre\":\"_init\",\"description\":\"bootstrap\"}";
	strtoquid(squid, &quid);
	btree_init(&btree, fname);
	bootstrap(&btree);

	size_t len;
	void *rdata = btree_get(&btree, &quid, &len);
	ASSERT(rdata);
	ASSERT(!strncmp(rdata, data, len));

	btree_close(&btree);
	btree_purge(fname);
}

TEST_IMPL(bootstrap) {
	/* Run testcase */
	bootstrap_zero();
	bootstrap_init();

	RETURN_OK();
}
