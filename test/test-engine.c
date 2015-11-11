#include <unistd.h>
#include <string.h>

#include "test.h"
#include "../src/zmalloc.h"
#include "../src/quid.h"
#include "../src/engine.h"

static void test_engine_create(){
	struct engine e;
	const char fname[] = "test_database1.idx";
	const char dbname[] = "test_database1.db";

	engine_init(&e, fname, dbname);
	ASSERT(e.fd);
	ASSERT(e.db_fd);
	ASSERT(e.alloc);
	ASSERT(e.db_alloc);
	engine_close(&e);
	ASSERT(file_exists(fname));
	ASSERT(file_exists(dbname));
	unlink(fname);
	unlink(dbname);
	ASSERT(!file_exists(fname));
	ASSERT(!file_exists(dbname));
}

static void test_engine_crud(){
	struct engine e;
	const char fname[] = "test_database2.idx";
	const char dbname[] = "test_database2.db";
	quid_t quid;
	char data[] = ".....";

	engine_init(&e, fname, dbname);
	quid_create(&quid);
	int r = engine_insert_data(&e, &quid, data, strlen(data));
	ASSERT(!r);
	engine_close(&e);

	engine_init(&e, fname, dbname);
	size_t len;
	uint64_t offset = engine_get(&e, &quid);
	void *rdata = get_data(&e, offset, &len);
	ASSERT(rdata);
	zfree(rdata);
	engine_close(&e);

	engine_init(&e, fname, dbname);
	int r2 = engine_delete(&e, &quid);
	ASSERT(!r2);
	engine_close(&e);

	engine_init(&e, fname, dbname);
	size_t len2;
	uint64_t offset2 = engine_get(&e, &quid);
	void *r2data = get_data(&e, offset2, &len2);
	ASSERT(!r2data);
	engine_close(&e);
	unlink(fname);
	unlink(dbname);
}

static void test_engine_meta(){
	struct engine e;
	const char fname[] = "test_database4.idx";
	const char dbname[] = "test_database4.db";
	quid_t quid;
	char data[] = ".....";

	engine_init(&e, fname, dbname);
	quid_create(&quid);
	int r = engine_insert_data(&e, &quid, data, strlen(data));
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
	int r2 = engine_setmeta(&e, &quid, &md);
	ASSERT(!r2);
	engine_close(&e);

	engine_init(&e, fname, dbname);
	struct metadata md2;
	int r3 = engine_getmeta(&e, &quid, &md2);
	ASSERT(!r3);
	ASSERT(!memcmp(&md, &md2, sizeof(struct metadata)));
	engine_close(&e);
	unlink(fname);
	unlink(dbname);
}

TEST_IMPL(engine) {

	TESTCASE("engine");

	/* Run testcase */
	test_engine_create();
	test_engine_crud();
	test_engine_meta();

	RETURN_OK();
}
