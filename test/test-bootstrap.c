#include <string.h>

#include "test.h"
#include "../src/quid.h"
#include "../src/engine.h"
#include "../src/bootstrap.h"

#define BS_MAGIC "__zero()__"

static void bootstrap_zero(){
    struct engine e;
    const char fname[] = "test_bootstrap.idx";
    const char dbname[] = "test_bootstrap.db";
	quid_t quid;

	const char squid[] = "{00000000-0000-0000-0000-000000000000}";
	strtoquid(squid, &quid);
	engine_init(&e, fname, dbname);
	bootstrap(&e);

	size_t len;
	void *data = engine_get(&e, &quid, &len);
	ASSERT(data);
	ASSERT(!strncmp(data, BS_MAGIC, len));

	engine_close(&e);
	unlink(fname);
	unlink(dbname);
}

static void bootstrap_init(){
	struct engine e;
	const char fname[] = "test_bootstrap2.idx";
	const char dbname[] = "test_bootstrap2.db";
	quid_t quid;

	const char squid[] = "{00000000-00c1-a150-0000-000000000080}";
	const char data[] = "{\"pre\":\"_init\",\"description\":\"bootstrap\"}";
	strtoquid(squid, &quid);
	engine_init(&e, fname, dbname);
	bootstrap(&e);

	size_t len;
	void *rdata = engine_get(&e, &quid, &len);
	ASSERT(rdata);
	ASSERT(!strncmp(rdata, data, len));

	engine_close(&e);
	unlink(fname);
	unlink(dbname);
}

TEST_IMPL(bootstrap) {

	TESTCASE("bootstrap");

	/* Run testcase */
	bootstrap_zero();
	bootstrap_init();

	RETURN_OK();
}
