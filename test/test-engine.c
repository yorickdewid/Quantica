#include <unistd.h>
#include <string.h>

#include "test.h"
#include "quid.h"
#include "engine.h"

static int file_exists(const char *path)
{
    int fd = open(path, O_RDWR);
    if(fd>-1) {
        close(fd);
        return 1;
    }
    return 0;
}

static void engine_create(){
	struct btree btree;
	const char fname[] = "test_database1";
	char fpath[sizeof(fname)+5];
	btree_init(&btree, fname);
	ASSERT(btree.fd);
	ASSERT(btree.db_fd);
	ASSERT(btree.alloc);
	ASSERT(btree.db_alloc);
	btree_close(&btree);
	sprintf(fpath, "%s.db", fname);
	ASSERT(file_exists(fpath));
	btree_purge(fname);
	ASSERT(!file_exists(fpath));
}

static void engine_crud(){
	struct btree btree;
	const char fname[] = "test_database2";
	struct quid quid;
	char data[] = ".....";

	btree_init(&btree, fname);
	quid_create(&quid);
	int r = btree_insert(&btree, &quid, data, strlen(data));
	ASSERT(!r);
	btree_close(&btree);

	btree_init(&btree, fname);
	size_t len;
	void *rdata = btree_get(&btree, &quid, &len);
	ASSERT(rdata);
	free(rdata);
	btree_close(&btree);

	btree_init(&btree, fname);
	int r2 = btree_delete(&btree, &quid);
	ASSERT(!r2);
	btree_close(&btree);

	btree_init(&btree, fname);
	size_t len2;
	void *r2data = btree_get(&btree, &quid, &len2);
	ASSERT(!r2data);
	btree_close(&btree);

	btree_purge(fname);
}

TEST_IMPL(engine) {
	/* Run testcase */
	engine_create();
	engine_crud();

	RETURN_OK();
}