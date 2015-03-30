#include <unistd.h>
#include <string.h>

#include "test.h"
#include "../src/quid.h"
#include "../src/engine.h"

static void unlink_backup(const char* fname) {
    char dbname[1024], idxname[1024], walname[1024];
    snprintf(idxname, 1024, "%s.db1", fname);
    snprintf(dbname, 1024, "%s.idx1", fname);
    snprintf(walname, 1024, "%s.log1", fname);
    unlink(idxname);
    unlink(dbname);
    unlink(walname);
}

static void test_engine_create(){
	struct engine e;
	const char fname[] = "test_database1";
    size_t fpath_sz = sizeof(fname)+5;
	char fpath[fpath_sz];

	engine_init(&e, fname);
	ASSERT(e.fd);
	ASSERT(e.db_fd);
	ASSERT(e.alloc);
	ASSERT(e.db_alloc);
	engine_close(&e);
	snprintf(fpath, fpath_sz, "%s.db", fname);
	ASSERT(file_exists(fpath));
	engine_unlink(fname);
	ASSERT(!file_exists(fpath));
}

static void test_engine_crud(){
	struct engine e;
	const char fname[] = "test_database2";
	quid_t quid;
	char data[] = ".....";

	engine_init(&e, fname);
	quid_create(&quid);
	int r = engine_insert(&e, &quid, data, strlen(data));
	ASSERT(!r);
	engine_close(&e);

	engine_init(&e, fname);
	size_t len;
	void *rdata = engine_get(&e, &quid, &len);
	ASSERT(rdata);
	free(rdata);
	engine_close(&e);

	engine_init(&e, fname);
	int r2 = engine_delete(&e, &quid);
	ASSERT(!r2);
	engine_close(&e);

	engine_init(&e, fname);
	size_t len2;
	void *r2data = engine_get(&e, &quid, &len2);
	ASSERT(!r2data);
	engine_close(&e);
	engine_unlink(fname);
}

static void test_engine_clean(){
	struct engine e;
	const char fname[] = "test_database3";
	quid_t quid;
	char data[] = ".....";

	engine_init(&e, fname);
	quid_create(&quid);
	int r = engine_insert(&e, &quid, data, strlen(data));
	ASSERT(!r);

	int r2 = engine_vacuum(&e, fname);
	ASSERT(!r2);
	engine_close(&e);

    size_t fpath_sz = sizeof(fname)+5;
	char fpath[fpath_sz];
	snprintf(fpath, fpath_sz, "%s._db", fname);
	ASSERT(file_exists(fpath));
	snprintf(fpath, fpath_sz, "%s.db1", fname);
	ASSERT(file_exists(fpath));

	engine_init(&e, fname);
	size_t len;
	void *rdata = engine_get(&e, &quid, &len);
	ASSERT(rdata);
	free(rdata);
	engine_close(&e);
	engine_unlink(fname);
	unlink_backup(fname);
}

static void test_engine_meta(){
	struct engine e;
	const char fname[] = "test_database4";
	quid_t quid;
	char data[] = ".....";

	engine_init(&e, fname);
	quid_create(&quid);
	int r = engine_insert(&e, &quid, data, strlen(data));
	ASSERT(!r);

	const struct microdata md = {
		.lifecycle = MD_LIFECYCLE_FINITE,
		.importance = MD_IMPORTANT_LEVEL3,
		.syslock = 0,
		.exec = 1,
		.freeze = 1,
		.error = 0,
		.type = MD_TYPE_BOOL_TRUE,
	};
	int r2 = engine_setmeta(&e, &quid, &md);
	ASSERT(!r2);
	engine_close(&e);

	engine_init(&e, fname);
	struct microdata md2;
	int r3 = engine_getmeta(&e, &quid, &md2);
	ASSERT(!r3);
	ASSERT(!memcmp(&md, &md2, sizeof(struct microdata)));
	engine_close(&e);
	engine_unlink(fname);
}

TEST_IMPL(engine) {
	/* Run testcase */
	test_engine_create();
	test_engine_crud();
	test_engine_clean();
	test_engine_meta();

	RETURN_OK();
}
