#include <stdlib.h>
#include <string.h>

#include <config.h>
#include <common.h>
#include <log.h>
#include "zmalloc.h"
#include "quid.h"
#include "engine.h"
#include "bootstrap.h"

#define BS_MAGIC "__zero()__"

quid_t key;
struct metadata md;

//TODO insert key with metadata

void bootstrap(struct engine *e) {
	memset(&key, 0, sizeof(quid_t));
	memset(&md, 0, sizeof(struct metadata));

	/* Verify bootstrap signature */
	size_t len;
	void *rdata = engine_get(e, &key, &len);
	if (rdata && !memcmp(rdata, BS_MAGIC, strlen(BS_MAGIC))) {
		zfree(rdata);
		return;
	}

	/* Add bootstrap signature to empty database */
	const char data0[] = BS_MAGIC;
	if (engine_insert_data(e, &key, data0, strlen(data0)) < 0)
		lprint("[erro] bootstrap: Insert failed\n");

	md.importance = MD_IMPORTANT_CRITICAL;
	md.syslock = LOCK;
	md.exec = TRUE;
	md.type = MD_TYPE_RAW;
	if (engine_setmeta(e, &key, &md) < 0)
		lprint("[erro] bootstrap: Update meta failed\n");

	/* Add database initial routine */
	const char skey1[] = "{00000000-00c1-a150-0000-000000000080}";
	strtoquid(skey1, &key);
	const char data1[] = "{\"pre\":\"_init\",\"description\":\"bootstrap\"}";
	if (engine_insert_data(e, &key, data1, strlen(data1)) < 0)
		lprint("[erro] bootstrap: Insert failed\n");

	md.importance = MD_IMPORTANT_LEVEL2;
	md.syslock = LOCK;
	md.type = MD_TYPE_RAW;
	if (engine_setmeta(e, &key, &md) < 0)
		lprint("[erro] bootstrap: Update meta failed\n");

	/* Add database bootlog routine */
	const char skey2[] = "{00000000-00c1-a150-0000-00000000008a}";
	strtoquid(skey2, &key);
	const char data2[] = "_bootlog()";
	if (engine_insert_data(e, &key, data2, strlen(data2)) < 0)
		lprint("[erro] bootstrap: Insert failed\n");

	md.importance = MD_IMPORTANT_LEVEL3;
	md.syslock = LOCK;
	md.exec = TRUE;
	md.type = MD_TYPE_RAW;
	if (engine_setmeta(e, &key, &md) < 0)
		lprint("[erro] bootstrap: Update meta failed\n");

	/* Set backend inter process communication */
	const char skey3[] = "{00000000-00c1-a150-0000-00000000008b}";
	strtoquid(skey3, &key);
	const char data3[] = "SELECT IPC();SELECT SPNEGO()";
	if (engine_insert_data(e, &key, data3, strlen(data3)) < 0)
		lprint("[erro] bootstrap: Insert failed\n");

	md.importance = MD_IMPORTANT_LEVEL1;
	md.syslock = LOCK;
	md.type = MD_TYPE_RAW;
	if (engine_setmeta(e, &key, &md) < 0)
		lprint("[erro] bootstrap: Update meta failed\n");
}
