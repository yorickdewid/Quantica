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

	const char squid[] = "{" DEFAULT_PREFIX "-000000000000}";
	strtoquid(squid, &quid);
	engine_init(&e, fname, dbname);
	bootstrap(&e);

	size_t len;
	uint64_t offset = engine_get(&e, &quid);
	void *data = get_data(&e, offset, &len);
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

	const char squid[] = "{" DEFAULT_PREFIX "-000000000080}";
	const char data[] = "{\"pre\":\"_init\",\"description\":\"bootstrap\"}";
	strtoquid(squid, &quid);
	engine_init(&e, fname, dbname);
	bootstrap(&e);

	size_t len;
	uint64_t offset = engine_get(&e, &quid);
	void *rdata = get_data(&e, offset, &len);
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
