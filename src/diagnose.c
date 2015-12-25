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

bool diag_exerr(base_t *base) {
	char tmp_key[QUID_LENGTH + 1];
	quid_t key;
	engine_t engine;

	lprint("[info] Failure on exit, run diagnostics\n");
	engine_init(base, &engine);
	quid_create(&key);
	quidtostr(tmp_key, &key);

	engine_recover_storage(base, &engine);
	if (engine_vacuum(base, &engine) < 0) {
		lprint("[erro] Failed to vacuum\n");
		return FALSE;
	}
	engine_close(base, &engine);
	return TRUE;
}
