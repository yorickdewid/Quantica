#include <stdlib.h>
#include <string.h>

#include <config.h>
#include <common.h>
#include <log.h>
#include <error.h>
#include "zmalloc.h"
#include "quid.h"
#include "sha1.h"
#include "md5.h"
#include "sha256.h"
#include "aes.h"
#include "crc32.h"
#include "base64.h"
#include "time.h"
#include "json_encode.h"
#include "slay.h"
#include "engine.h"
#include "bootstrap.h"
#include "core.h"

static struct engine btx;
static uint8_t ready = FALSE;
static qtime_t uptime;
struct error _eglobal;

void start_core() {
	start_log();
	ERRORZEOR();
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
	strtoupper(name);
	strlcpy(btx.ins_name, name, INSTANCE_LENGTH);
	btx.ins_name[INSTANCE_LENGTH-1] = '\0';
	engine_sync(&btx);
}

char *get_instance_name() {
	return btx.ins_name;
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

int crypto_md5(char *s, const char *data) {
	unsigned char digest[16];
	md5_ctx md5;
	md5_init(&md5);
	md5_update(&md5, data, strlen(data));
	md5_final(digest, &md5);
	md5_strsum(s, digest);
	return 0;
}

int crypto_sha256(char *s, const char *data) {
	unsigned char digest[SHA256_BLOCK_SIZE];
	sha256_ctx ctx;
	sha256_init(&ctx);
	sha256_update(&ctx, (const unsigned char *)data, strlen(data));
	sha256_final(&ctx, digest);
	sha256_strsum(s, digest);
	return 0;
}

char *crypto_base64_enc(const char *data) {
	size_t encsz = base64_encode_len(strlen(data));
	char *s = (char *)zmalloc(encsz+1);
	base64_encode(s, data, strlen(data));
	s[encsz] = '\0';
	return s;
}

char *crypto_base64_dec(const char *data) {
	size_t decsz = base64_decode_len(data);
	char *s = (char *)zmalloc(decsz+1);
	base64_decode(s, data);
	s[decsz] = '\0';
	return s;
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

int db_put(char *quid, int *items, const void *data, size_t data_len) {
	if (!ready)
		return -1;
	quid_t key;
	size_t len = 0;
	quid_create(&key);

	void *slay = slay_put_data((char *)data, data_len, &len, items);

	if (engine_insert(&btx, &key, slay, len)<0)
		return -1;
	zfree(slay);

	quidtostr(quid, &key);
	return 0;
}

void *_db_get(char *quid, dstype_t *dt) {
	if (!ready)
		return NULL;
	quid_t key;
	strtoquid(quid, &key);

	size_t len;
	void *data = engine_get(&btx, &key, &len);
	if (!data)
		return NULL;

	char *buf = slay_get_data(data, dt);
	zfree(data);
	return buf;
}

void *db_get(char *quid) {
	if (!ready)
		return NULL;
	quid_t key;
	strtoquid(quid, &key);

	size_t len;
	void *data = engine_get(&btx, &key, &len);
	if (!data)
		return NULL;

	dstype_t dt;
	char *buf = slay_get_data(data, &dt);
	zfree(data);
	return buf;
}

char *db_get_type(char *quid) {
	if (!ready)
		return NULL;
	(void)quid;
	/*size_t len;
	quid_t key;
	dstype_t dt;
	strtoquid(quid, &key);

	void *val_data = engine_get(&btx, &key, &len);
	if (!val_data)
		return NULL;
	void *data = slay_unwrap(val_data, &len, &dt);
	zfree(data);
	return str_type(dt);*/
	return NULL;
}

int db_update(char *quid, const void *data, size_t len) {
	if (!ready)
		return -1;
	(void)quid;
	(void)data;
	(void)len;
/*	quid_t key;
	strtoquid(quid, &key);
	void *slay = create_row(1, &len);
	void *val_data = slay_wrap(slay, (void *)data, len, DT_TEXT);
	if (engine_update(&btx, &key, val_data, len)<0) {
		return -1;
	}
	zfree(slay);
	zfree(val_data);*/
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

int db_purge(char *quid) {
	if (!ready)
		return -1;
	quid_t key;
	strtoquid(quid, &key);
	if (engine_purge(&btx, &key)<0)
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
