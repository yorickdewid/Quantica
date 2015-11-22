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

char *get_zero_key() {
	static char buf[QUID_LENGTH + 1];
	quidtostr(buf, &control.zero_key);
	return buf;
}

void start_core() {
	/* Start the logger */
	start_log();
	error_clear();

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

bool get_ready_status() {
	return ready ? TRUE : FALSE;
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

char *get_dataheap_name() {
	return control.bindata;
}

struct engine *get_current_engine() {
	return &btx;
}

/*
 * Create instance key QUID from short QUID
 */
char *get_instance_prefix_key(char *short_quid) {
	static char buf[QUID_LENGTH + 1];
	quidtostr(buf, &control.instance_key);
	memcpy(buf + 25, short_quid, SHORT_QUID_LENGTH - 2);
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

void quid_generate_short(char *quid) {
	quid_t key;
	quid_create(&key);
	quidtoshortstr(quid, &key);
}

void filesync() {
	engine_sync(&btx);
	base_sync(&control);
}

int db_put(char *quid, int *items, const void *data, size_t data_len) {
	quid_t key;
	size_t len = 0;
	quid_create(&key);
	slay_result_t nrs;

	if (!ready)
		return -1;

	marshall_t *dataobj = marshall_convert((char *)data, data_len);
	if (!dataobj) {
		return -1;
	}

	void *dataslay = slay_put(dataobj, &len, &nrs);
	*items = nrs.items;
	if (engine_insert_data(&btx, &key, dataslay, len) < 0) {
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
		engine_get(&btx, &key, &meta);
		if (meta.type != MD_TYPE_RECORD) {
			error_throw("2f05699f70fa", "Key does not contain data");
			return -1;
		}

		meta.type = MD_TYPE_GROUP;
		if (engine_setmeta(&btx, &key, &meta) < 0)
			return -1;
	}

	return 0;
}

void *db_get(char *quid, size_t *len, bool descent) {
	quid_t key;
	size_t _len;
	struct metadata meta;
	strtoquid(quid, &key);
	void *data = NULL;
	marshall_t *dataobj = NULL;

	if (!ready)
		return NULL;

	uint64_t offset = engine_get(&btx, &key, &meta);
	switch (meta.type) {
		case MD_TYPE_RECORD:
		case MD_TYPE_GROUP: {
			data = get_data_block(&btx, offset, &_len);
			if (!data)
				return NULL;

			dataobj = slay_get(data, NULL, descent);
			if (!dataobj) {
				zfree(data);
				return NULL;
			}
			break;
		}
		case MD_TYPE_INDEX: {
			dataobj = index_btree_all(&key);
			break;
		}
		default:
			/* Key contains data we cannot (yet) return */
			error_throw("2f05699f70fa", "Key does not contain data");
			return NULL;
	}

	char *buf = marshall_serialize(dataobj);
	*len = strlen(buf);
	if (data)
		zfree(data);
	marshall_free(dataobj);

	return buf;
}

char *db_get_type(char *quid) {
	quid_t key;
	struct metadata meta;
	strtoquid(quid, &key);

	if (!ready)
		return NULL;

	size_t len;
	uint64_t offset = engine_get(&btx, &key, &meta);
	if (!engine_keytype_hasdata(meta.type)) {
		error_throw("2f05699f70fa", "Key does not contain data");
		return NULL;
	}

	void *data = get_data_block(&btx, offset, &len);
	if (!data)
		return NULL;

	marshall_type_t type = slay_get_type(data);
	zfree(data);
	return marshall_get_strtype(type);
}

char *db_get_schema(char *quid) {
	quid_t key;
	struct metadata meta;
	strtoquid(quid, &key);

	if (!ready)
		return NULL;

	size_t len;
	uint64_t offset = engine_get(&btx, &key, &meta);
	if (!engine_keytype_hasdata(meta.type)) {
		error_throw("2f05699f70fa", "Key does not contain data");
		return NULL;
	}

	void *data = get_data_block(&btx, offset, &len);
	if (!data)
		return NULL;

	char *buf = slay_get_strschema(data);
	zfree(data);
	return buf;
}

int db_update(char *quid, int *items, const void *data, size_t data_len) {
	quid_t key;
	size_t len = 0;
	slay_result_t nrs;
	strtoquid(quid, &key);

	if (!ready)
		return -1;

	marshall_t *dataobj = marshall_convert((char *)data, data_len);
	if (!dataobj) {
		return -1;
	}

	void *dataslay = slay_put(dataobj, &len, &nrs);
	*items = nrs.items;
	if (engine_update_data(&btx, &key, dataslay, len) < 0) {
		zfree(dataslay);
		marshall_free(dataobj);
		return -1;
	}

	zfree(dataslay);
	marshall_free(dataobj);
	return 0;
}

int db_delete(char *quid, bool descent) {
	quid_t key;
	size_t _len;
	struct metadata meta;
	strtoquid(quid, &key);

	if (!ready)
		return -1;

	uint64_t offset = engine_get(&btx, &key, &meta);
	switch (meta.type) {
		case MD_TYPE_GROUP: {
			if (descent) {
				void *data = get_data_block(&btx, offset, &_len);
				if (!data)
					break;

				marshall_t *dataobj = slay_get(data, NULL, FALSE);
				if (!dataobj) {
					zfree(data);
					break;
				}

				for (unsigned int i = 0; i < dataobj->size; ++i) {
					quid_t _key;
					strtoquid(dataobj->child[i]->data, &_key);
					engine_delete(&btx, &_key);
					error_clear();
				}
				marshall_free(dataobj);
				zfree(data);
			}

			engine_list_delete(&btx, &key);
			break;
		}
		case MD_TYPE_INDEX:
		case MD_TYPE_RECORD:
		default:
			break;
	}

	if (engine_delete(&btx, &key) < 0)
		return -1;

	return 0;
}

int db_purge(char *quid, bool descent) {
	quid_t key;
	size_t _len;
	struct metadata meta;
	strtoquid(quid, &key);

	if (!ready)
		return -1;

	uint64_t offset = engine_get(&btx, &key, &meta);
	switch (meta.type) {
		case MD_TYPE_GROUP: {
			if (descent) {
				void *data = get_data_block(&btx, offset, &_len);
				if (!data)
					break;

				marshall_t *dataobj = slay_get(data, NULL, FALSE);
				if (!dataobj) {
					zfree(data);
					break;
				}

				for (unsigned int i = 0; i < dataobj->size; ++i) {
					quid_t _key;
					strtoquid(dataobj->child[i]->data, &_key);
					engine_purge(&btx, &_key);
					error_clear();
				}
				marshall_free(dataobj);
				zfree(data);
			}

			engine_list_delete(&btx, &key);
			break;
		}
		case MD_TYPE_INDEX:
		case MD_TYPE_RECORD:
		default:
			break;
	}

	if (engine_purge(&btx, &key) < 0)
		return -1;

	return 0;
}

int db_vacuum() {
	char tmp_key[QUID_LENGTH + 1];
	quid_t key;
	quid_create(&key);
	quidtostr(tmp_key, &key);

	if (!ready)
		return -1;

	char *bindata = generate_bindata_name(&control);
	if (engine_vacuum(&btx, tmp_key, bindata) < 0)
		return -1;

	memcpy(&control.zero_key, &key, sizeof(quid_t));
	strcpy(control.bindata, bindata);
	return 0;
}

int db_record_get_meta(char *quid, struct record_status *status) {
	quid_t key;
	struct metadata meta;
	strtoquid(quid, &key);

	if (!ready)
		return -1;

	engine_get(&btx, &key, &meta);
	status->syslock = meta.syslock;
	status->exec = meta.exec;
	status->freeze = meta.freeze;
	status->nodata = meta.nodata;
	status->importance = meta.importance;
	strlcpy(status->lifecycle, get_str_lifecycle(meta.lifecycle), STATUS_LIFECYCLE_SIZE);
	strlcpy(status->type, get_str_type(meta.type), STATUS_TYPE_SIZE);
	return 0;
}

int db_record_set_meta(char *quid, struct record_status *status) {
	quid_t key;
	struct metadata meta;
	strtoquid(quid, &key);

	if (!ready)
		return -1;

	memset(&meta, 0, sizeof(struct metadata));
	meta.syslock = status->syslock;
	meta.exec = status->exec;
	meta.freeze = status->freeze;
	meta.importance = status->importance;
	meta.lifecycle = get_meta_lifecycle(status->lifecycle);
	if (engine_setmeta(&btx, &key, &meta) < 0)
		return -1;

	return 0;
}

char *db_alias_get_name(char *quid) {
	quid_t key;
	strtoquid(quid, &key);

	if (!ready)
		return NULL;

	return engine_list_get_val(&btx, &key);
}

int db_alias_update(char *quid, const char *name) {
	quid_t key;
	strtoquid(quid, &key);

	if (!ready)
		return -1;

	return engine_list_update(&btx, &key, name, strlen(name));;
}

char *db_alias_all() {
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

void *db_alias_get_data(char *name, size_t *len, bool descent) {
	quid_t key;
	size_t _len;
	struct metadata meta;
	void *data = NULL;
	marshall_t *dataobj = NULL;

	if (!ready)
		return NULL;

	if (engine_list_get_key(&btx, &key, name, strlen(name)) < 0)
		return NULL;

	uint64_t offset = engine_get(&btx, &key, &meta);
	switch (meta.type) {
		case MD_TYPE_RECORD:
		case MD_TYPE_GROUP: {
			data = get_data_block(&btx, offset, &_len);
			if (!data)
				return NULL;

			dataobj = slay_get(data, NULL, descent);
			if (!dataobj) {
				zfree(data);
				return NULL;
			}
			break;
		}
		case MD_TYPE_INDEX: {
			dataobj = index_btree_all(&key);
			break;
		}
		default:
			/* Key contains data we cannot (yet) return */
			error_throw("2f05699f70fa", "Key does not contain data");
			return NULL;
	}

	char *buf = marshall_serialize(dataobj);
	*len = strlen(buf);
	if (data)
		zfree(data);
	marshall_free(dataobj);

	return buf;
}

/*
 * Set index on group element
 */
int db_index_create(char *group_quid, char *index_quid, int *items, const char *idxkey) {
	quid_t key;
	size_t _len;
	struct metadata meta;
	index_result_t nrs;
	strtoquid(group_quid, &key);
	memset(&nrs, 0, sizeof(index_result_t));

	if (!ready)
		return -1;

	quid_create(&nrs.index);
	quidtostr(index_quid, &nrs.index);

	uint64_t offset = engine_get(&btx, &key, &meta);
	if (meta.type != MD_TYPE_GROUP) {
		error_throw("2f05699f70fa", "Key does not contain data");
		return -1;
	}

	void *data = get_data_block(&btx, offset, &_len);
	if (!data)
		return -1;

	marshall_t *dataobj = slay_get(data, NULL, FALSE);
	if (!dataobj)
		return -1;

	/* Determine index based on dataschema */
	schema_t group = slay_get_schema(data);
	if (group == SCHEMA_TABLE)
		index_btree_create_table(index_quid, idxkey, dataobj, &nrs);
	else if (group == SCHEMA_SET)
		index_btree_create_set(index_quid, idxkey, dataobj, &nrs);

	marshall_free(dataobj);
	zfree(data);

	quidtostr(index_quid, &nrs.index);
	*items = nrs.index_elements;

	/* Add index to database */
	memset(&meta, 0, sizeof(struct metadata));
	meta.nodata = 1;
	meta.type = MD_TYPE_INDEX;
	engine_insert_meta(&btx, &nrs.index, &meta);

	/* Add index to alias list */
	engine_list_insert(&btx, &nrs.index, index_quid, QUID_LENGTH);

	return 0;
}
