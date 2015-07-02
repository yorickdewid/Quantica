#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <config.h>
#include <common.h>
#include <log.h>
#include "quid.h"
#include "engine.h"
#include "diagnose.h"

bool diag_exerr(struct base *base) {
	char buf[QUID_LENGTH+1];
	char tmp_key[QUID_LENGTH+1];
	quid_t key;
	struct engine engine;
	quidtostr(buf, &base->zero_key);

	lprintf("[info] Failure on exit, run diagnostics\n");
	engine_init(&engine, buf, base->bindata);
	quid_create(&key);
	quidtostr(tmp_key, &key);
	char *bindata = generate_bindata_name(base);

	if (engine_vacuum(&engine, tmp_key, bindata)<0) {
		lprintf("[erro] Failed to vacuum\n");
		return FALSE;
	}
	memcpy(&base->zero_key, &key, sizeof(quid_t));
	strcpy(base->bindata, bindata);

	engine_close(&engine);
	return TRUE;
}
