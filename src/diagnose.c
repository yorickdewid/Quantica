#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <config.h>
#include <common.h>
#include <log.h>
#include "core.h"
#include "quid.h"
#include "engine.h"
#include "diagnose.h"

bool diag_exerr(struct base *base) {
	char tmp_key[QUID_LENGTH+1];
	quid_t key;
	struct engine engine;

	lprint("[info] Failure on exit, run diagnostics\n");
	engine_init(&engine, get_zero_key(), base->bindata);
	quid_create(&key);
	quidtostr(tmp_key, &key);
	char *bindata = generate_bindata_name(base);

	engine_recover_storage(&engine);
	if (engine_vacuum(&engine, tmp_key, bindata)<0) {
		lprint("[erro] Failed to vacuum\n");
		return FALSE;
	}
	memcpy(&base->zero_key, &key, sizeof(quid_t));
	strcpy(base->bindata, bindata);

	engine_close(&engine);
	return TRUE;
}
