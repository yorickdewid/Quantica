#include <stdlib.h>
#include <string.h>

#include <config.h>
#include <common.h>
#include <log.h>
#include <error.h>
#include "quid.h"
#include "sha1.h"
#include "aes.h"
#include "crc32.h"
#include "base64.h"
#include "time.h"
#include "engine.h"
#include "bootstrap.h"
#include "core.h"

#define INSTANCE_LENGTH 32

static struct engine btx;
static uint8_t ready = FALSE;
char ins_name[INSTANCE_LENGTH];
static qtime_t uptime;
struct error _eglobal;

void start_core() {
	start_log();
	ERRORZEOR();
	set_instance_name(INSTANCE);
	engine_init(&btx, INITDB);
	bootstrap(&btx);
	uptime = get_timestamp();
	ready = TRUE;
}

void detach_core() {
	if (!ready)
		return;
	engine_close(&btx);
	stop_log();
	ready = FALSE;
}

void set_instance_name(char name[]) {
	if (strlen(name) > INSTANCE_LENGTH)
		return;

	strlcpy(ins_name, name, INSTANCE_LENGTH);
	ins_name[INSTANCE_LENGTH-1] = '\0';
}

char *get_instance_name() {
	return ins_name;
}

char *get_uptime() {
	static char buf[32];
	qtime_t passed = get_timestamp()-uptime;
	unsigned int days = passed/86400;
	passed = passed % 86400;
	unsigned int hours = passed/3600;
	passed = passed % 3600;
	unsigned int mins = passed/60;
	passed = passed % 60;
	unsigned int secs = passed;
	snprintf(buf, 32, "%u days, %.2u:%.2u:%.2u", days, hours, mins, secs);
	return buf;
}

int crypto_sha1(char *s, const char *data) {
	struct sha sha;
	sha1_reset(&sha);
	sha1_input(&sha, (const unsigned char *)data, strlen(data));

	if (!sha1_result(&sha)) {
		return -1;
	}
	sha1_strsum(s, &sha);
	return 0;
}

unsigned long int stat_getkeys() {
	return btx.stats.keys;
}

unsigned long int stat_getfreekeys() {
	return btx.stats.free_tables;
}

void quid_generate(char *quid) {
	quid_t key;
	quid_create(&key);
	quidtostr(quid, &key);
}

int db_put(char *quid, const void *data, size_t len) {
	if (!ready)
		return -1;
	quid_t key;
	quid_create(&key);
	if (engine_insert(&btx, &key, data, len)<0)
		return -1;
	quidtostr(quid, &key);
	return 0;
}

void *db_get(char *quid, size_t *len) {
	if (!ready)
		return NULL;
	quid_t key;
	strtoquid(quid, &key);
	void *data = engine_get(&btx, &key, len);
	return data;
}

int db_update(char *quid, const void *data, size_t len) {
	if (!ready)
		return -1;
	quid_t key;
	strtoquid(quid, &key);
	if (engine_update(&btx, &key, data, len)<0) {
		return -1;
	}
	return 0;
}

int db_delete(char *quid) {
	if (!ready)
		return -1;
	quid_t key;
	strtoquid(quid, &key);
	if (engine_delete(&btx, &key)<0)
		return -1;
	return 0;
}

int db_vacuum() {
	if (!ready)
		return -1;
	return engine_vacuum(&btx, INITDB);
}

int db_record_get_meta(char *quid, struct record_status *status) {
	if (!ready)
		return -1;
	quid_t key;
	struct metadata meta;
	strtoquid(quid, &key);
	if (engine_getmeta(&btx, &key, &meta)<0)
		return -1;
	status->syslock = meta.syslock;
	status->exec = meta.exec;
	status->freeze = meta.freeze;
	status->error = meta.error;
	status->importance = meta.importance;
	strlcpy(status->lifecycle, get_str_lifecycle(meta.lifecycle), STATUS_LIFECYCLE_SIZE);
	strlcpy(status->type, get_str_type(meta.type), STATUS_TYPE_SIZE);
	return 0;
}

int db_record_set_meta(char *quid, struct record_status *status) {
	if (!ready)
		return -1;
	quid_t key;
	struct metadata meta;
	strtoquid(quid, &key);

	meta.syslock = status->syslock;
	meta.exec = status->exec;
	meta.freeze = status->freeze;
	meta.error = status->error;
	meta.importance = status->importance;
	meta.lifecycle = get_meta_lifecycle(status->lifecycle);
	meta.type = get_meta_type(status->type);
	if (engine_setmeta(&btx, &key, &meta)<0)
		return -1;
	return 0;
}
