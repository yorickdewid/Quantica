#include <unistd.h>
#include <string.h>

#include <error.h>
#include "test.h"
#include "../src/zmalloc.h"
#include "../src/quid.h"
#include "../src/engine.h"

static void test_engine_create() {
	engine_t engine;
	const char fname[] = "test_database1.idx";
	const char dbname[] = "test_database1.db";

	error_clear();
	engine_init(&engine, fname, dbname);
	ASSERT(engine.fd);
	ASSERT(engine.db_fd);
	ASSERT(engine.alloc);
	ASSERT(engine.db_alloc);
	engine_close(&engine);
	ASSERT(file_exists(fname));
	ASSERT(file_exists(dbname));
	unlink(fname);
	unlink(dbname);
	ASSERT(!file_exists(fname));
	ASSERT(!file_exists(dbname));
	error_clear();
}

static void test_engine_crud() {
	engine_t engine;
	const char fname[] = "test_database2.idx";
	const char dbname[] = "test_database2.db";
	quid_t quid;
	struct metadata meta;
	char data[] = ".....";

	error_clear();
	engine_init(&engine, fname, dbname);
	quid_create(&quid);
	int r = engine_insert_data(&engine, &quid, data, strlen(data));
	ASSERT(!r);
	engine_close(&engine);

	engine_init(&engine, fname, dbname);
	size_t len;
	uint64_t offset = engine_get(&engine, &quid, &meta);
	void *rdata = get_data_block(&engine, offset, &len);
	ASSERT(rdata);
	zfree(rdata);
	engine_close(&engine);

	engine_init(&engine, fname, dbname);
	int r2 = engine_delete(&engine, &quid);
	ASSERT(!r2);
	engine_close(&engine);

	engine_init(&engine, fname, dbname);
	size_t len2;
	uint64_t offset2 = engine_get(&engine, &quid, &meta);
	void *r2data = get_data_block(&engine, offset2, &len2);
	ASSERT(!r2data);
	engine_close(&engine);
	unlink(fname);
	unlink(dbname);
	error_clear();
}

static void test_engine_meta() {
	engine_t engine;
	const char fname[] = "test_database4.idx";
	const char dbname[] = "test_database4.db";
	quid_t quid;
	char data[] = ".....";

	error_clear();
	engine_init(&engine, fname, dbname);
	quid_create(&quid);
	int r = engine_insert_data(&engine, &quid, data, strlen(data));
	ASSERT(!r);

	const struct metadata md = {
		.lifecycle = MD_LIFECYCLE_FINITE,
		.importance = MD_IMPORTANT_LEVEL3,
		.syslock = 0,
		.exec = 1,
		.freeze = 1,
		.nodata = 0,
		.type = MD_TYPE_RAW,
	};
	int r2 = engine_setmeta(&engine, &quid, &md);
	ASSERT(!r2);
	engine_close(&engine);

	engine_init(&engine, fname, dbname);
	struct metadata md2;
	engine_get(&engine, &quid, &md2);
	ASSERT(!memcmp(&md, &md2, sizeof(struct metadata)));
	engine_close(&engine);
	unlink(fname);
	unlink(dbname);
	error_clear();
}

TEST_IMPL(engine) {

	TESTCASE("engine");

	/* Run testcase */
	test_engine_create();
	test_engine_crud();
	test_engine_meta();

	RETURN_OK();
}
