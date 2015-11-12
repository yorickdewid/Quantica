#include <stdlib.h>
#include <string.h>

#include <config.h>
#include <common.h>
#include <log.h>
#include "zmalloc.h"
#include "quid.h"
#include "slay.h"
#include "engine.h"
#include "bootstrap.h"

#define BS_MAGIC "__zero()__"

static int register_error(struct engine *e, char *error_code, char *error_message) {
	quid_t key;
	size_t len;
	slay_result_t nrs;

	char skey[QUID_LENGTH + 1];
	sprintf(skey, "{" DEFAULT_PREFIX "-%s}", error_code);
	strtoquid(skey, &key);

	marshall_t *dataobj = marshall_convert(error_message, strlen(error_message));
	if (!dataobj)
		lprint("[erro] bootstrap: Conversion failed\n");

	void *dataslay = slay_put(dataobj, &len, &nrs);

	struct metadata meta;
	memset(&meta, 0, sizeof(struct metadata));
	meta.importance = MD_IMPORTANT_LEVEL1;
	meta.syslock = LOCK;
	meta.type = MD_TYPE_RAW;
	if (engine_insert_meta_data(e, &key, &meta, dataslay, len) < 0) {
		zfree(dataslay);
		marshall_free(dataobj);
		return -1;
	}
	zfree(dataslay);
	marshall_free(dataobj);

	return 0;
}

void bootstrap(struct engine *e) {
	quid_t key;
	struct metadata meta;
	memset(&meta, 0, sizeof(struct metadata));

	const char skey[] = "{" DEFAULT_PREFIX "-000000000000}";
	strtoquid(skey, &key);

	/* Verify bootstrap signature */
	size_t len;
	uint64_t offset = engine_get(e, &key);
	if (offset) {
		void *rdata = get_data(e, offset, &len);
		if (rdata && !memcmp(rdata, BS_MAGIC, strlen(BS_MAGIC))) {
			zfree(rdata);
			return;
		}
	}

	/* Add bootstrap signature to empty database */
	const char data0[] = BS_MAGIC;
	meta.importance = MD_IMPORTANT_CRITICAL;
	meta.syslock = LOCK;
	meta.exec = TRUE;
	meta.type = MD_TYPE_RAW;
	if (engine_insert_meta_data(e, &key, &meta, data0, strlen(data0)) < 0)
		lprint("[erro] bootstrap: Insert failed\n");

	/* Add database initial routine */
	const char skey1[] = "{" DEFAULT_PREFIX "-000000000080}";
	strtoquid(skey1, &key);
	const char data1[] = "{\"pre\":\"_init\",\"description\":\"bootstrap\"}";
	meta.importance = MD_IMPORTANT_LEVEL2;
	meta.syslock = LOCK;
	meta.type = MD_TYPE_RAW;
	if (engine_insert_meta_data(e, &key, &meta, data1, strlen(data1)) < 0)
		lprint("[erro] bootstrap: Insert failed\n");

	/* Add database bootlog routine */
	const char skey2[] = "{" DEFAULT_PREFIX "-00000000008a}";
	strtoquid(skey2, &key);
	const char data2[] = "_bootlog()";
	meta.importance = MD_IMPORTANT_LEVEL3;
	meta.syslock = LOCK;
	meta.exec = TRUE;
	meta.type = MD_TYPE_RAW;
	if (engine_insert_meta_data(e, &key, &meta, data2, strlen(data2)) < 0)
		lprint("[erro] bootstrap: Insert failed\n");

	/* Set backend inter process communication */
	const char skey3[] = "{" DEFAULT_PREFIX "-00000000008b}";
	strtoquid(skey3, &key);
	const char data3[] = "SELECT IPC();SELECT SPNEGO()";
	meta.importance = MD_IMPORTANT_LEVEL1;
	meta.syslock = LOCK;
	meta.type = MD_TYPE_RAW;
	if (engine_insert_meta_data(e, &key, &meta, data3, strlen(data3)) < 0)
		lprint("[erro] bootstrap: Insert failed\n");

	/*
	 * Register error messages
	 */
	if (register_error(e, "de65321630e4", "Server is not ready") < 0)
		lprint("[erro] bootstrap: Insert error failed\n");

	if (register_error(e, "caa73770706e", "Error reading from disk") < 0)
		lprint("[erro] bootstrap: Insert error failed\n");

	if (register_error(e, "777007517053", "Error writing to disk") < 0)
		lprint("[erro] bootstrap: Insert error failed\n");

	if (register_error(e, "6fc7a048300a", "No record found") < 0)
		lprint("[erro] bootstrap: Insert error failed\n");
}
