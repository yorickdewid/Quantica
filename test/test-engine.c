#include <string.h>

#include "test.h"
#include "quid.h"
#include "engine.h"

static void engine_create(){
	struct btree btree;
	const char fname[] = "test_database";
	btree_init(&btree, fname);
	ASSERT(btree.fd);
	ASSERT(btree.db_fd);
	ASSERT(btree.alloc);
	ASSERT(btree.db_alloc);
	btree_close(&btree);
	btree_purge(fname);
}

TEST_IMPL(engine) {
	/* Run testcase */
	engine_create();

	RETURN_OK();
}
