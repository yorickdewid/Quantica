#include <stdlib.h>
#include <string.h>

#include <config.h>
#include <common.h>
#include <log.h>
#include <error.h>
#include "zmalloc.h"
#include "quid.h"
#include "sha1.h"
#include "sha2.h"
#include "hmac.h"
#include "md5.h"
#include "aes.h"
#include "crc32.h"
#include "base64.h"
#include "time.h"
#include "btree.h"
#include "index.h"
#include "marshall.h"
#include "slay.h"
#include "basecontrol.h"
#include "engine.h"
#include "bootstrap.h"
#include "sql.h"
#include "core.h"

static struct engine btx;
static struct base control;
static uint8_t ready = FALSE;
static long long uptime;
static quid_t sessionid;
struct error _eglobal;

char *get_zero_key() {
	static char buf[QUID_LENGTH + 1];
	quidtostr(buf, &control.zero_key);
	return buf;
}

void start_core() {
	/* Start the logger */
	start_log();
	ERRORZEOR();

	quid_create(&sessionid);
	base_init(&control);

	/* Initialize engine */
	engine_init(&btx, get_zero_key(), control.bindata);

	/* Bootstrap database if not exist */
	bootstrap(&btx);

	/* Server ready */
	uptime = get_timestamp();
	ready = TRUE;
}

void detach_core() {
	if (!ready)
		return;
	/* CLose all databases */
	engine_close(&btx);
	base_close(&control);

	/* Stop the logger */
	stop_log();

	/* Server is inactive */
	ready = FALSE;
}

void set_instance_name(char name[]) {
	strtoupper(name);
	strlcpy(control.instance_name, name, INSTANCE_LENGTH);
	control.instance_name[INSTANCE_LENGTH - 1] = '\0';
	base_sync(&control);
}

char *get_instance_name() {
	return control.instance_name;
}

char *get_instance_key() {
	static char buf[QUID_LENGTH + 1];
	quidtostr(buf, &control.instance_key);
	return buf;
}

char *get_session_key() {
	static char buf[QUID_LENGTH + 1];
	quidtostr(buf, &sessionid);
	return buf;
}

char *get_uptime() {
	static char buf[32];
	long long passed = get_timestamp() - uptime;
	unsigned int days = passed / 86400;
	passed = passed % 86400;
	unsigned int hours = passed / 3600;
	passed = passed % 3600;
	unsigned int mins = passed / 60;
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
	unsigned char digest[MD5_BLOCK_SIZE];
	md5_ctx md5;
	md5_init(&md5);
	md5_update(&md5, data, strlen(data));
	md5_final(digest, &md5);
	md5_strsum(s, digest);
	return 0;
}

int crypto_sha256(char *s, const char *data) {
	unsigned char digest[SHA256_BLOCK_SIZE];
	sha256((const unsigned char *)data, strlen(data), digest);
	sha2_strsum(s, digest, 2 * SHA256_DIGEST_SIZE);
	return 0;
}

int crypto_sha512(char *s, const char *data) {
	unsigned char digest[SHA512_BLOCK_SIZE];
	sha512((const unsigned char *)data, strlen(data), digest);
	sha2_strsum(s, digest, 2 * SHA512_DIGEST_SIZE);
	return 0;
}

int crypto_hmac_sha256(char *s, const char *key, const char *data) {
	unsigned char mac[SHA256_DIGEST_SIZE];

	hmac_sha256((const unsigned char *)key, strlen(key), (unsigned char *)data, strlen(data), mac, SHA256_DIGEST_SIZE);
	sha2_strsum(s, mac, SHA256_BLOCK_SIZE);
	return 0;
}

int crypto_hmac_sha512(char *s, const char *key, const char *data) {
	unsigned char mac[SHA512_DIGEST_SIZE];

	hmac_sha512((const unsigned char *)key, strlen(key), (unsigned char *)data, strlen(data), mac, SHA512_DIGEST_SIZE);
	sha2_strsum(s, mac, SHA512_BLOCK_SIZE);
	return 0;
}

char *crypto_base64_enc(const char *data) {
	size_t encsz = base64_encode_len(strlen(data));
	char *s = (char *)zmalloc(encsz + 1);
	base64_encode(s, data, strlen(data));
	s[encsz] = '\0';
	return s;
}

char *crypto_base64_dec(const char *data) {
	size_t decsz = base64_decode_len(data);
	char *s = (char *)zmalloc(decsz + 1);
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

unsigned long int stat_tablesize() {
	return btx.stats.list_size;
}

sqlresult_t *exec_sqlquery(const char *query, size_t *len) {
	return sql_exec(query, len);
}

void quid_generate(char *quid) {
	quid_t key;
	quid_create(&key);
	quidtostr(quid, &key);
}

void filesync() {
	engine_sync(&btx);
	base_sync(&control);
}

int raw_db_put(char *quid, void *dataslay, size_t len) {
	if (!ready)
		return -1;
	quid_t key;
	quid_create(&key);

	if (engine_insert(&btx, &key, dataslay, len) < 0) {
		zfree(dataslay);
		return -1;
	}
	zfree(dataslay);

	quidtostr(quid, &key);
	return 0;
}

int db_put(char *quid, int *items, const void *data, size_t data_len) {
	if (!ready)
		return -1;
	quid_t key;
	size_t len = 0;
	quid_create(&key);
	slay_result_t nrs;

	marshall_t *dataobj = marshall_convert((char *)data, data_len);
	if (!dataobj) {
		return -1;
	}

	void *dataslay = slay_put(dataobj, &len, &nrs);
	*items = nrs.items;
	if (engine_insert(&btx, &key, dataslay, len) < 0) {
		zfree(dataslay);
		marshall_free(dataobj);
		return -1;
	}
	zfree(dataslay);
	marshall_free(dataobj);

	quidtostr(quid, &key);
	if (nrs.schema == SCHEMA_TABLE || nrs.schema == SCHEMA_SET) {
		engine_list_insert(&btx, &key, quid, QUID_LENGTH);

		struct metadata meta;
		if (engine_getmeta(&btx, &key, &meta) < 0)
			return -1;
		meta.type = MD_TYPE_GROUP;
		if (engine_setmeta(&btx, &key, &meta) < 0)
			return -1;
	}
	return 0;
}

marshall_t *raw_db_get(char *quid, void *parent) {
	if (!ready)
		return NULL;

	quid_t key;
	strtoquid(quid, &key);

	size_t len;
	void *data = engine_get(&btx, &key, &len);
	if (!data)
		return NULL;

	marshall_t *dataobj = slay_get(data, parent, TRUE);
	zfree(data);
	return dataobj;
}

void *db_get(char *quid, size_t *len, bool descent) {
	if (!ready)
		return NULL;

	quid_t key;
	strtoquid(quid, &key);
	size_t _len;

	void *data = engine_get(&btx, &key, &_len);
	if (!data)
		return NULL;

	marshall_t *dataobj = slay_get(data, NULL, descent);
	if (!dataobj)
		return NULL;

	char *buf = marshall_serialize(dataobj);
	*len = strlen(buf);
	zfree(data);
	marshall_free(dataobj);

	return buf;
}

char *db_get_type(char *quid) {
	if (!ready)
		return NULL;
	quid_t key;
	strtoquid(quid, &key);

	size_t len;
	void *data = engine_get(&btx, &key, &len);
	if (!data)
		return NULL;

	marshall_type_t type = slay_get_type(data);
	zfree(data);
	return marshall_get_strtype(type);
}

char *db_get_schema(char *quid) {
	if (!ready)
		return NULL;
	quid_t key;
	strtoquid(quid, &key);

	size_t len;
	void *data = engine_get(&btx, &key, &len);
	if (!data)
		return NULL;

	char *buf = slay_get_strschema(data);
	zfree(data);
	return buf;
}

uint64_t db_get_offset(char *quid) {
	if (!ready)
		return 0;

	quid_t key;
	strtoquid(quid, &key);

	return engine_get_offset(&btx, &key);
}

int raw_db_update(char *quid, void *slay, size_t len) {
	if (!ready)
		return -1;
	quid_t key;
	strtoquid(quid, &key);

	if (engine_update(&btx, &key, slay, len) < 0) {
		zfree(slay);
		return -1;
	}
	zfree(slay);
	return 0;
}

int db_update(char *quid, int *items, const void *data, size_t data_len) {
	if (!ready)
		return -1;
	quid_t key;
	size_t len = 0;
	slay_result_t nrs;
	strtoquid(quid, &key);

	marshall_t *dataobj = marshall_convert((char *)data, data_len);
	if (!dataobj) {
		return -1;
	}

	void *dataslay = slay_put(dataobj, &len, &nrs);
	*items = nrs.items;
	if (engine_update(&btx, &key, dataslay, len) < 0) {
		zfree(dataslay);
		marshall_free(dataobj);
		return -1;
	}
	zfree(dataslay);
	marshall_free(dataobj);
	return 0;
}

int db_delete(char *quid) {
	if (!ready)
		return -1;
	quid_t key;
	struct metadata meta;
	strtoquid(quid, &key);
	if (engine_getmeta(&btx, &key, &meta) < 0)
		return -1;
	if (meta.type == MD_TYPE_GROUP)
		engine_list_delete(&btx, &key);
	if (engine_delete(&btx, &key) < 0)
		return -1;
	return 0;
}

int db_purge(char *quid) {
	if (!ready)
		return -1;
	quid_t key;
	struct metadata meta;
	strtoquid(quid, &key);
	if (engine_getmeta(&btx, &key, &meta) < 0)
		return -1;
	if (meta.type == MD_TYPE_GROUP)
		engine_list_delete(&btx, &key);
	if (engine_purge(&btx, &key) < 0)
		return -1;
	return 0;
}

int db_vacuum() {
	if (!ready)
		return -1;
	char tmp_key[QUID_LENGTH + 1];
	quid_t key;
	quid_create(&key);
	quidtostr(tmp_key, &key);
	char *bindata = generate_bindata_name(&control);

	if (engine_vacuum(&btx, tmp_key, bindata) < 0)
		return -1;
	memcpy(&control.zero_key, &key, sizeof(quid_t));
	strcpy(control.bindata, bindata);
	return 0;
}

int db_record_get_meta(char *quid, struct record_status *status) {
	if (!ready)
		return -1;
	quid_t key;
	struct metadata meta;
	strtoquid(quid, &key);
	if (engine_getmeta(&btx, &key, &meta) < 0)
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

	memset(&meta, 0, sizeof(struct metadata));
	meta.syslock = status->syslock;
	meta.exec = status->exec;
	meta.freeze = status->freeze;
	meta.error = status->error;
	meta.importance = status->importance;
	meta.lifecycle = get_meta_lifecycle(status->lifecycle);
	meta.type = get_meta_type(status->type);
	if (engine_setmeta(&btx, &key, &meta) < 0)
		return -1;
	return 0;
}

char *db_list_get(char *quid) {
	if (!ready)
		return NULL;
	quid_t key;
	strtoquid(quid, &key);
	return engine_list_get_val(&btx, &key);
}

int db_list_update(char *quid, const char *name) {
	if (!ready)
		return -1;
	quid_t key;
	strtoquid(quid, &key);
	return engine_list_update(&btx, &key, name, strlen(name));;
}

char *db_list_all() {
	if (!ready)
		return NULL;

	marshall_t *dataobj = engine_list_all(&btx);
	if (!dataobj) {
		return NULL;
	}

	char *buf = marshall_serialize(dataobj);
	marshall_free(dataobj);
	return buf;
}

void *db_table_get(char *name, size_t *len, bool descent) {
	if (!ready)
		return NULL;

	quid_t key;
	if (engine_list_get_key(&btx, &key, name, strlen(name)) < 0)
		return NULL;

	size_t _len;
	void *data = engine_get(&btx, &key, &_len);
	if (!data)
		return NULL;

	marshall_t *dataobj = slay_get(data, NULL, descent);
	if (!dataobj)
		return NULL;

	char *buf = marshall_serialize(dataobj);
	*len = strlen(buf);
	zfree(data);
	marshall_free(dataobj);
	return buf;
}

int db_create_index(char *group_quid, char *index_quid, int *items, const char *idxkey) {
	if (!ready)
		return -1;

	quid_t key;
	index_result_t nrs;
	strtoquid(group_quid, &key);
	size_t _len;
	memset(&nrs, 0, sizeof(index_result_t));

	void *data = engine_get(&btx, &key, &_len);
	if (!data)
		return -1;

	marshall_t *dataobj = slay_get(data, NULL, FALSE);
	if (!dataobj)
		return -1;

	schema_t group = slay_get_schema(data);

	index_create_btree(idxkey, dataobj, group, &nrs);

	marshall_free(dataobj);
	zfree(data);

	quidtostr(index_quid, &nrs.index);
	*items = nrs.index_elements;

	// TODO insert in db
	// Set metadata to index
	engine_list_insert(&btx, &nrs.index, index_quid, QUID_LENGTH);

	return 0;
}
