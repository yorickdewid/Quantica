#include <stdlib.h>
#include <string.h>

#include <config.h>
#include <common.h>
#include <log.h>
#include "zmalloc.h"
#include "error.h"
#include "quid.h"
#include "slay.h"
#include "core.h"
#include "engine.h"
#include "bootstrap.h"

#define BS_MAGIC "__zero()__"

#define E_FATAL	0x1
#define E_WARN	0x2

static int register_error(struct engine *e, int level, char *error_code, char *error_message) {
	quid_t key;
	size_t len;
	slay_result_t nrs;

	char *skey = get_instance_prefix_key(error_code);
	strtoquid(skey, &key);

	char errobj[256];
	memset(&errobj, 0, sizeof(errobj));

	if (level == E_FATAL) {
		snprintf(errobj, 256, "{\"level\":\"Fatal\", \"description\":\"%s\"}", error_message);
	} else if (level == E_WARN) {
		snprintf(errobj, 256, "{\"level\":\"Warning\", \"description\":\"%s\"}", error_message);
	} else {
		snprintf(errobj, 256, "{\"level\":\"Unknown\", \"description\":\"%s\"}", error_message);
	}

	marshall_t *dataobj = marshall_convert(errobj, strlen(errobj));
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

	const char *skey = get_instance_prefix_key("000000000000");
	strtoquid(skey, &key);

	/* Verify bootstrap signature */
	size_t len;
	uint64_t offset = engine_get(e, &key, &meta);
	if (offset && !iserror()) {
		void *rdata = get_data_block(e, offset, &len);
		if (rdata && !memcmp(rdata, BS_MAGIC, strlen(BS_MAGIC))) {
			zfree(rdata);
			return;
		}
	}

	/* No errors from this point on */
	error_clear();

	/* Add bootstrap signature to empty database */
	memset(&meta, 0, sizeof(struct metadata));
	const char data0[] = BS_MAGIC;
	meta.importance = MD_IMPORTANT_CRITICAL;
	meta.syslock = LOCK;
	meta.exec = TRUE;
	meta.type = MD_TYPE_RAW;
	if (engine_insert_meta_data(e, &key, &meta, data0, strlen(data0)) < 0)
		lprint("[erro] bootstrap: Insert failed\n");

	/* Add database initial routine */
	const char *skey1 = get_instance_prefix_key("000000000080");
	strtoquid(skey1, &key);
	const char data1[] = "{\"pre\":\"_init\",\"description\":\"bootstrap\"}";
	meta.importance = MD_IMPORTANT_LEVEL2;
	meta.syslock = LOCK;
	meta.type = MD_TYPE_RAW;
	if (engine_insert_meta_data(e, &key, &meta, data1, strlen(data1)) < 0)
		lprint("[erro] bootstrap: Insert failed\n");

	/* Add database bootlog routine */
	const char *skey2 = get_instance_prefix_key("00000000008a");
	strtoquid(skey2, &key);
	const char data2[] = "_bootlog()";
	meta.importance = MD_IMPORTANT_LEVEL3;
	meta.syslock = LOCK;
	meta.exec = TRUE;
	meta.type = MD_TYPE_RAW;
	if (engine_insert_meta_data(e, &key, &meta, data2, strlen(data2)) < 0)
		lprint("[erro] bootstrap: Insert failed\n");

	/* Set backend inter process communication */
	const char *skey3 = get_instance_prefix_key("00000000008b");
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
	if (register_error(e, E_FATAL, "7b8a6ac440e2", "Failed to request memory") < 0)
		lprint("[erro] bootstrap: Insert error failed\n");

	if (register_error(e, E_FATAL, "a7df40ba3075", "Failed to read disk") < 0)
		lprint("[erro] bootstrap: Insert error failed\n");

	if (register_error(e, E_FATAL, "1fd531fa70c1", "Failed to write disk") < 0)
		lprint("[erro] bootstrap: Insert error failed\n");

	if (register_error(e, E_WARN, "de65321630e4", "Server is not ready") < 0)
		lprint("[erro] bootstrap: Insert error failed\n");

	if (register_error(e, E_WARN, "e8880046e019", "No data provided") < 0)
		lprint("[erro] bootstrap: Insert error failed\n");

	if (register_error(e, E_WARN, "a475446c70e8", "Key exists") < 0)
		lprint("[erro] bootstrap: Insert error failed\n");

	if (register_error(e, E_WARN, "986154f80058", "Database locked") < 0)
		lprint("[erro] bootstrap: Insert error failed\n");

	if (register_error(e, E_WARN, "4987a3310049", "Record locked") < 0)
		lprint("[erro] bootstrap: Insert error failed\n");

	if (register_error(e, E_WARN, "6ef42da7901f", "Record not found") < 0)
		lprint("[erro] bootstrap: Insert error failed\n");

	if (register_error(e, E_WARN, "2836444cd009", "Alias not found") < 0)
		lprint("[erro] bootstrap: Insert error failed\n");

	if (register_error(e, E_WARN, "dcb796d620d1", "Unknown datastructure") < 0)
		lprint("[erro] bootstrap: Insert error failed\n");

	if (register_error(e, E_WARN, "888d28dff048", "Operation expexts an index given") < 0)
		lprint("[erro] bootstrap: Insert error failed\n");

	if (register_error(e, E_WARN, "ece28bc980db", "Invalid schema") < 0)
		lprint("[erro] bootstrap: Insert error failed\n");

	if (register_error(e, E_WARN, "2f05699f70fa", "Key does not contain data") < 0)
		lprint("[erro] bootstrap: Insert error failed\n");

	/* Clear any failed operations */
	error_clear();
}
