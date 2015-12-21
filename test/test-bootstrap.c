#include <string.h>

#include "test.h"
#include "../src/quid.h"
#include "../src/engine.h"
#include "../src/bootstrap.h"

#define BS_MAGIC "__zero()__"
#define DEFAULT_PREFIX "00000000-0000-0000-0000"

static void bootstrap_zero() {
	engine_t engine;
	const char fname[] = "test_bootstrap.idx";
	const char dbname[] = "test_bootstrap.db";
	quid_t quid;
	struct metadata meta;

	const char squid[] = "{" DEFAULT_PREFIX "-000000000000}";
	strtoquid(squid, &quid);
	engine_init(&engine, fname, dbname);
	bootstrap(&engine);

	size_t len;
	uint64_t offset = engine_get(&engine, &quid, &meta);
	void *data = get_data_block(&engine, offset, &len);
	ASSERT(data);
	ASSERT(!strncmp(data, BS_MAGIC, len));

	engine_close(&engine);
	unlink(fname);
	unlink(dbname);
}

static void bootstrap_init() {
	engine_t engine;
	const char fname[] = "test_bootstrap2.idx";
	const char dbname[] = "test_bootstrap2.db";
	quid_t quid;
	struct metadata meta;

	const char squid[] = "{" DEFAULT_PREFIX "-000000000080}";
	const char data[] = "{\"pre\":\"_init\",\"description\":\"bootstrap\"}";
	strtoquid(squid, &quid);
	engine_init(&engine, fname, dbname);
	bootstrap(&engine);

	size_t len;
	uint64_t offset = engine_get(&engine, &quid, &meta);
	void *rdata = get_data_block(&engine, offset, &len);
	ASSERT(rdata);
	ASSERT(!strncmp(rdata, data, len));

	engine_close(&engine);
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
