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
#include "base.h"
#include "pager.h"
#include "btree.h"
#include "index.h"
#include "marshall.h"
#include "dict_marshall.h"
#include "slay_marshall.h"
#include "arc4random.h"
#include "engine.h"
#include "alias.h"
#include "history.h"
#include "index_list.h"
#include "bootstrap.h"
#include "jwt.h"
#include "sql.h"
#include "core.h"

static engine_t zero;
static base_t control;
static bool ready = FALSE;
static long long uptime;
static quid_t sessionid;

void start_core() {
	/* Start the logger */
	start_log();
	error_clear();

	/* Daemon Session */
	quid_create(&sessionid);

	base_init(&control, &zero);
	pager_init(&control);
	engine_init(&control);

	/* Bootstrap database if not exist */
	bootstrap(&control);

	/* Server ready */
	uptime = get_timestamp();
	ready = TRUE;
}

void detach_core() {
	if (!ready)
		return;

	/* Close all databases */
	engine_close(&control);
	pager_close(&control);
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

char *get_pager_alloc_size() {
	static char buf[10];
	unsigned long long total_size = (BASE_PAGE_SIZE << control.pager.size) * control.core->count;
	return unit_bytes(total_size, buf);
}

char *get_total_disk_size() {
	static char buf[10];
	size_t total_size = pager_total_disk_size(&control);
	total_size += file_size(control.fd);
	return unit_bytes(total_size, buf);
}

unsigned int get_pager_page_size() {
	return control.pager.size;
}

unsigned int get_pager_page_count() {
	return control.core->count;
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

char *auth_token(char *key) {
	// Test payload {
	marshall_t *marshall = (marshall_t *)tree_zcalloc(1, sizeof(marshall_t), NULL);
	marshall->child = (marshall_t **)tree_zcalloc(1, sizeof(marshall_t *), marshall);
	marshall->type = MTYPE_OBJECT;

	marshall->child[marshall->size] = tree_zcalloc(1, sizeof(marshall_t), marshall);
	marshall->child[marshall->size]->type = MTYPE_QUID;
	marshall->child[marshall->size]->name = tree_zstrdup("name", marshall);
	marshall->child[marshall->size]->name_len = 4;
	marshall->child[marshall->size]->data = tree_zstrdup(get_instance_key(), marshall);
	marshall->child[marshall->size]->data_len = QUID_LENGTH;
	marshall->child[marshall->size]->size = 1;
	marshall->size++;
	// }

	return jwt_encode(marshall, (const unsigned char *)key);
}

unsigned long int stat_getkeys() {
	return control.stats.zero_size;
}

unsigned long int stat_getfreekeys() {
	return control.stats.zero_free_size;
}

unsigned long int stat_getfreeblocks() {
	return control.stats.heap_free_size;
}

unsigned long int stat_tablesize() {
	return control.stats.alias_size;
}

unsigned long int stat_indexsize() {
	return control.stats.index_list_size;
}

sqlresult_t *exec_sqlquery(const char *query, size_t *len) {
	return sql_exec(query, len);
}

int generate_random_number(int range) {
	return (!range) ? (int)arc4random() : (int)arc4random_uniform(range);
}

void quid_generate(char *quid) {
	quid_t key;
	quid_create(&key);
	quidtostr(quid, &key);
}

void quid_generate_short(char *quid) {
	quid_short_t key;
	quid_short_create(&key);
	quid_shorttostr(quid, &key);
}

void filesync() {
	engine_sync(&control);
	pager_sync(&control);
	base_sync(&control);
}

int zvacuum(int page_size) {
	base_t new_control;
	engine_t new_zero;

	if (!ready)
		return -1;

	if (!page_size)
		page_size = control.pager.size;

	/* Copy current database */
	base_lock(&control);
	base_copy(&control, &new_control, &new_zero, page_size);
	pager_init(&new_control);

	/* Rebuild structures into order */
	engine_rebuild(&control, &new_control);
	alias_rebuild(&control, &new_control);
	index_list_rebuild(&control, &new_control);

	engine_close(&control);
	pager_unlink_all(&control);
	pager_close(&control);
	base_close(&control);

	base_swap();
	memcpy(&zero, &new_zero, sizeof(engine_t));
	memcpy(&control, &new_control, sizeof(base_t));
	control.engine = &zero;

	return 0;
}

char *key_decode(char *quid) {
	quid_t key;
	if (!strquid_format(quid)) {
		error_throw("f0b867c41006", "Key malformed or invalid");
		return NULL;
	}

	strtoquid(quid, &key);
	marshall_t *dataobj = quid_decode(&key);

	char *buf = marshall_serialize(dataobj);
	marshall_free(dataobj);
	return buf;
}

int db_put(char *quid, int *items, const void *data, size_t data_len, char *hint, char *hint_option) {
	quid_t key;
	size_t len = 0;
	quid_create(&key);
	slay_result_t nrs;
	marshall_t *dataobj = NULL;

	if (!ready)
		return -1;

	if (hint) {

		marshall_t *option = NULL;
		if (hint_option) {
			option = marshall_convert(hint_option, strlen(hint_option));
		}

		dataobj = marshall_convert_suggest((char *)data, hint, option);
		marshall_free(option);
		if (dataobj)
			goto db_store;
	}

	dataobj = marshall_convert((char *)data, data_len);
	if (!dataobj) {
		return -1;
	}

db_store:;
	void *dataslay = slay_put(&control, dataobj, &len, &nrs);
	*items = nrs.items;
	if (engine_insert_data(&control, &key, dataslay, len) < 0) {
		zfree(dataslay);
		marshall_free(dataobj);
		return -1;
	}

	zfree(dataslay);
	marshall_free(dataobj);

	quidtostr(quid, &key);
	if (nrs.schema == SCHEMA_TABLE || nrs.schema == SCHEMA_SET) {
		alias_add(&control, &key, quid, QUID_LENGTH);

		struct metadata meta;
		engine_get(&control, &key, &meta);
		if (meta.type != MD_TYPE_RECORD) {
			error_throw("1e933eea602c", "Invalid record type");
			return -1;
		}

		meta.type = MD_TYPE_GROUP;
		meta.alias = 1;
		if (engine_setmeta(&control, &key, &meta) < 0)
			return -1;
	}

	return 0;
}

void *db_get(char *quid, size_t *len, bool descent, bool force) {
	quid_t key;
	size_t _len;
	struct metadata meta;
	strtoquid(quid, &key);
	void *data = NULL;
	marshall_t *dataobj = NULL;

	if (!ready)
		return NULL;

	uint64_t offset = force ? engine_get_force(&control, &key, &meta) : engine_get(&control, &key, &meta);
	switch (meta.type) {
		case MD_TYPE_RECORD:
		case MD_TYPE_GROUP: {
			data = get_data_block(&control, offset, &_len);
			if (!data)
				return NULL;

			dataobj = slay_get(&control, data, NULL, descent);
			if (!dataobj) {
				zfree(data);
				return NULL;
			}
			break;
		}
		case MD_TYPE_INDEX: {
			uint64_t index_offset = index_list_get_index_offset(&control, &key);
			dataobj = index_btree_all(&control, index_offset, descent);
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
	uint64_t offset = engine_get(&control, &key, &meta);
	if (!engine_keytype_hasdata(meta.type)) {
		error_throw("2f05699f70fa", "Key does not contain data");
		return NULL;
	}

	void *data = get_data_block(&control, offset, &len);
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
	uint64_t offset = engine_get(&control, &key, &meta);
	if (!engine_keytype_hasdata(meta.type)) {
		error_throw("2f05699f70fa", "Key does not contain data");
		return NULL;
	}

	void *data = get_data_block(&control, offset, &len);
	if (!data)
		return NULL;

	char *buf = slay_get_strschema(data);
	zfree(data);
	return buf;
}

char *db_get_history(char *quid) {
	quid_t key;
	strtoquid(quid, &key);

	if (!ready)
		return NULL;

	marshall_t *dataobj = history_all(&control, &key);
	char *buf = marshall_serialize(dataobj);
	marshall_free(dataobj);
	return buf;
}

char *db_get_version(char *quid, char *element) {
	quid_t key;
	size_t len;
	strtoquid(quid, &key);

	if (!ready)
		return NULL;

	if (!strisdigit(element) || element[0] == '-') {
		error_throw("888d28dff048", "Operation expects an positive index given");
		return NULL;
	}

	uint64_t offset = history_get_version_offset(&control, &key, atoi(element)) ;
	if (iserror()) {
		return NULL;
	}

	void *data = get_data_block(&control, offset, &len);
	if (!data)
		return NULL;

	marshall_t *dataobj = slay_get(&control, data, NULL, TRUE);

	char *buf = marshall_serialize(dataobj);
	if (data)
		zfree(data);
	marshall_free(dataobj);
	return buf;
}

int db_update(char *quid, int *items, bool descent, const void *data, size_t data_len) {
	quid_t key;
	size_t len = 0;
	size_t _len;
	slay_result_t nrs;
	struct metadata meta;
	strtoquid(quid, &key);

	if (!ready)
		return -1;

	marshall_t *dataobj = marshall_convert((char *)data, data_len);
	if (!dataobj) {
		return -1;
	}

	uint64_t offset = engine_get(&control, &key, &meta);
	switch (meta.type) {
		case MD_TYPE_GROUP: {
			if (descent) {
				void *descentdata = get_data_block(&control, offset, &_len);
				if (!descentdata)
					break;

				marshall_t *descentobj = slay_get(&control, descentdata, NULL, FALSE);
				if (!descentobj) {
					zfree(descentdata);
					break;
				}

				for (unsigned int i = 0; i < descentobj->size; ++i) {
					quid_t _key;
					struct metadata _meta;
					strtoquid(descentobj->child[i]->data, &_key);

					engine_get(&control, &_key, &_meta);

					if (_meta.alias)
						alias_delete(&control, &_key);

					engine_delete(&control, &_key);
					error_clear();
				}
				marshall_free(descentobj);
				zfree(descentdata);
			}
		}
		case MD_TYPE_RECORD:
			break;
		case MD_TYPE_INDEX:
		default:
			error_throw("0fb1dd21b0fd", "Internal records cannot be altered");
			marshall_free(dataobj);
			return -1;
			break;
	}

	/* Return if record was not found */
	if (iserror()) {
		marshall_free(dataobj);
		return -1;
	}

	/* Save old version as record history */
	history_add(&control, &key, offset);

	void *dataslay = slay_put(&control, dataobj, &len, &nrs);
	*items = nrs.items;
	if (engine_update_data(&control, &key, dataslay, len) < 0) {
		zfree(dataslay);
		marshall_free(dataobj);
		return -1;
	}

	if (nrs.schema == SCHEMA_TABLE || nrs.schema == SCHEMA_SET) {
		/* New data became a group */
		if (meta.type != MD_TYPE_GROUP) {
			char _quid[QUID_LENGTH + 1];
			quidtostr(_quid, &key);

			alias_add(&control, &key, _quid, QUID_LENGTH);

			meta.type = MD_TYPE_GROUP;
			meta.alias = 1;
			if (engine_setmeta(&control, &key, &meta) < 0)
				return -1;
		}
	} else {
		/* New data is no longer a group */
		if (meta.type == MD_TYPE_GROUP) {
			alias_delete(&control, &key);
		}
	}

	zfree(dataslay);
	marshall_free(dataobj);
	return 0;
}

int db_duplicate(char *quid, char *nquid, int *items, bool copy_meta) {
	quid_t key;
	quid_t nkey;
	size_t len = 0;
	size_t _len;
	slay_result_t nrs;
	struct metadata meta;
	strtoquid(quid, &key);

	if (!ready)
		return -1;

	uint64_t offset = engine_get(&control, &key, &meta);
	switch (meta.type) {
		case MD_TYPE_RECORD:
		case MD_TYPE_GROUP:
			break;
		default:
			error_throw("0fb1dd21b0fd", "Internal records cannot be altered");
			return -1;
			break;
	}

	void *descentdata = get_data_block(&control, offset, &_len);
	if (!descentdata)
		return 0;

	marshall_t *descentobj = slay_get(&control, descentdata, NULL, TRUE);
	if (!descentobj) {
		zfree(descentdata);
		return 0;
	}

	quid_create(&nkey);

	void *dataslay = slay_put(&control, descentobj, &len, &nrs);
	*items = nrs.items;
	if (engine_insert_data(&control, &nkey, dataslay, len) < 0) {
		zfree(descentdata);
		zfree(dataslay);
		marshall_free(descentobj);
		return -1;
	}

	if (!copy_meta) {
		nullify(&meta, sizeof(struct metadata));
		meta.importance = MD_IMPORTANT_NORMAL;
	}

	quidtostr(nquid, &nkey);
	if (nrs.schema == SCHEMA_TABLE || nrs.schema == SCHEMA_SET) {
		if (copy_meta) {
			marshall_t *index_element = index_list_get_element(&control, &key); /* TODO make kv list */
			if (index_element) {

				/* For every existing index create a new one */
				for (unsigned int i = 0; i < index_element->size; ++i) {
					index_result_t inrs;
					struct metadata _meta;
					nullify(&inrs, sizeof(index_result_t));
					char index_quid[QUID_LENGTH + 1];

					/* Create index key */
					quid_create(&inrs.index);
					quidtostr(index_quid, &inrs.index);

					marshall_t *_descentobj = slay_get(&control, descentdata, NULL, FALSE);
					if (!_descentobj) {
						continue;
					}

					/* Determine index based on dataschema */
					switch (nrs.schema) {
						case SCHEMA_TABLE:
							index_btree_create_table(&control, index_element->child[i]->data, _descentobj, &inrs);
							break;
						case SCHEMA_SET:
							index_btree_create_set(&control, index_element->child[i]->data, _descentobj, &inrs);
							break;
						default:
							error_throw("ece28bc980db", "Invalid schema");
					}

					/* Add index to database */
					nullify(&_meta, sizeof(struct metadata));
					_meta.nodata = 1;
					_meta.type = MD_TYPE_INDEX;
					_meta.alias = 1;
					engine_insert_meta(&control, &inrs.index, &_meta);

					/* Add index to alias list */
					alias_add(&control, &inrs.index, index_quid, QUID_LENGTH);

					/* TODO get index type */
					/* Add index to index list */
					index_list_add(&control, &inrs.index, &nkey, index_element->child[i]->data, INDEX_BTREE, inrs.offset);

					marshall_free(_descentobj);
				}
				error_clear();
				marshall_free(index_element);
			}
		}

		alias_add(&control, &nkey, nquid, QUID_LENGTH);
		meta.type = MD_TYPE_GROUP;
		meta.alias = 1;
	}

	if (engine_setmeta(&control, &nkey, &meta) < 0) {
		zfree(descentdata);
		return -1;
	}

	zfree(descentdata);
	zfree(dataslay);
	marshall_free(descentobj);
	return 0;
}

int db_count_group(char *quid) {
	quid_t key;
	size_t _len;
	marshall_t *dataobj = NULL;
	struct metadata meta;
	strtoquid(quid, &key);
	int cnt = 0;

	if (!ready)
		return -1;

	uint64_t offset = engine_get(&control, &key, &meta);
	switch (meta.type) {
		case MD_TYPE_GROUP: {
			void *data = get_data_block(&control, offset, &_len);
			if (!data)
				return -1;

			dataobj = slay_get(&control, data, NULL, FALSE);
			if (!dataobj) {
				zfree(data);
				return -1;
			}
			zfree(data);
			break;
		}
		case MD_TYPE_INDEX: {
			uint64_t index_offset = index_list_get_index_offset(&control, &key);
			dataobj = index_btree_all(&control, index_offset, FALSE);
			break;
		}
		default:
			/* Key contains data we cannot (yet) return */
			error_throw("2f05699f70fa", "Key does not contain data");
			return -1;
	}
	cnt = marshall_count(dataobj);

	marshall_free(dataobj);
	return cnt;
}

int db_delete(char *quid, bool descent) {
	quid_t key;
	size_t _len;
	struct metadata meta;
	strtoquid(quid, &key);

	if (!ready)
		return -1;

	uint64_t offset = engine_get(&control, &key, &meta);
	switch (meta.type) {
		case MD_TYPE_GROUP: {
			if (descent) {
				void *data = get_data_block(&control, offset, &_len);
				if (!data)
					break;

				marshall_t *dataobj = slay_get(&control, data, NULL, FALSE);
				if (!dataobj) {
					zfree(data);
					break;
				}

				for (unsigned int i = 0; i < dataobj->size; ++i) {
					quid_t _key;
					strtoquid(dataobj->child[i]->data, &_key);
					engine_delete(&control, &_key);
					error_clear();
				}
				marshall_free(dataobj);
				zfree(data);
			}

			/* Loop over the indexes on this group */
			quid_t *index = index_list_get_index(&control, &key);
			while (index) {
				alias_delete(&control, index);
				index_list_delete(&control, index);
				zfree(index);
				index = index_list_get_index(&control, &key);
			}
			error_clear();
			break;
		}
		case MD_TYPE_INDEX:
			index_list_delete(&control, &key);
			break;
		case MD_TYPE_RECORD:
		default:
			break;
	}

	if (meta.alias)
		alias_delete(&control, &key);

	if (engine_delete(&control, &key) < 0)
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

	uint64_t offset = engine_get_force(&control, &key, &meta);
	switch (meta.type) {
		case MD_TYPE_GROUP: {
			if (descent) {
				void *data = get_data_block(&control, offset, &_len);
				if (!data)
					break;

				marshall_t *dataobj = slay_get(&control, data, NULL, FALSE);
				if (!dataobj) {
					zfree(data);
					break;
				}

				for (unsigned int i = 0; i < dataobj->size; ++i) {
					quid_t _key;
					strtoquid(dataobj->child[i]->data, &_key);
					engine_purge(&control, &_key);
					error_clear();
				}
				marshall_free(dataobj);
				zfree(data);
			}

			quid_t *index = index_list_get_index(&control, &key);
			while (index) {
				alias_delete(&control, index);
				index_list_delete(&control, index);
				zfree(index);
				index = index_list_get_index(&control, &key);
			}
			error_clear();
			break;
		}
		case MD_TYPE_INDEX:

			index_list_delete(&control, &key);
			break;
		case MD_TYPE_RECORD:
		default:
			break;
	}

	if (meta.alias)
		alias_delete(&control, &key);

	if (engine_purge(&control, &key) < 0)
		return -1;

	return 0;
}

void *db_select(char *quid, const char *select_element, const char *where_element) {
	quid_t key;
	size_t _len;
	struct metadata meta;
	strtoquid(quid, &key);
	void *data = NULL;
	marshall_t *dataobj = NULL;
	marshall_t *whereobj = NULL;
	marshall_t *selectobj = NULL;

	if (!ready)
		return NULL;

	uint64_t offset = engine_get(&control, &key, &meta);
	switch (meta.type) {
		case MD_TYPE_RECORD:
		case MD_TYPE_GROUP: {
			data = get_data_block(&control, offset, &_len);
			if (!data)
				return NULL;

			dataobj = slay_get(&control, data, NULL, TRUE);
			if (!dataobj) {
				zfree(data);
				return NULL;
			}
			break;
		}
		case MD_TYPE_INDEX: {
			uint64_t index_offset = index_list_get_index_offset(&control, &key);
			dataobj = index_btree_all(&control, index_offset, TRUE);
			break;
		}
		default:
			/* Key contains data we cannot (yet) select */
			error_throw("2f05699f70fa", "Key does not contain data");
			return NULL;
	}

	/* Where */
	if (where_element) {
		marshall_t *where_elementobj = marshall_convert((char *)where_element, strlen(where_element));
		if (!where_elementobj) {
			if (data)
				zfree(data);
			marshall_free(dataobj);
			return NULL;
		}

		/* Can indexes be used */
		marshall_t *indexes = index_list_on_group(&control, &key);
		if (indexes) {
			quid_t index_key;

			if (where_elementobj->type == MTYPE_OBJECT) {

				/* Does the indexed element match the condition */
				if (!strcmp(where_elementobj->child[0]->name, indexes->child[0]->child[1]->data)) {
					strtoquid(indexes->child[0]->child[0]->data, &index_key);

					uint64_t index_offset = index_list_get_index_offset(&control, &index_key);
					whereobj = index_get(&control, index_offset, where_elementobj->child[0]->data);

					/* We're done */
					marshall_free(where_elementobj);
					marshall_free(indexes);
					goto where_done;
				}
			} else if (where_elementobj->type == MTYPE_ARRAY) {
				unsigned int index_found = 0;

				for (unsigned int j = 0; j < where_elementobj->size; ++j) {
					if (!strcmp(where_elementobj->child[j]->child[0]->name, indexes->child[0]->child[1]->data))
						index_found++;
				}

				/* Do the found indexes satisfy all conditions */
				if (where_elementobj->size == index_found) {
					unsigned int item_count = 0;
					marshall_t *rs[where_elementobj->size];

					/* Gather results */
					for (unsigned int j = 0; j < where_elementobj->size; ++j) {
						strtoquid(indexes->child[0]->child[0]->data, &index_key);

						uint64_t index_offset = index_list_get_index_offset(&control, &index_key);
						rs[j] = index_get(&control, index_offset, where_elementobj->child[j]->child[0]->data);
					}

					/* Count total items */
					for (unsigned int j = 0; j < where_elementobj->size; ++j) {
						if (rs[j])
							item_count += rs[j]->size;
					}

					marshall_t *x_whereobj = (marshall_t *)tree_zcalloc(1, sizeof(marshall_t), NULL);
					x_whereobj->child = (marshall_t **)tree_zcalloc(item_count, sizeof(marshall_t *), x_whereobj);
					x_whereobj->type = MTYPE_ARRAY;

					/* incremental append */
					for (unsigned int j = 0; j < where_elementobj->size; ++j) {
						if (rs[j])
							x_whereobj = marshall_merge(rs[j], x_whereobj);
					}

					whereobj = marshall_copy(x_whereobj, NULL);
					for (unsigned int j = 0; j < where_elementobj->size; ++j) {
						if (rs[j])
							tree_zfree(rs[j]);
					}

					marshall_free(x_whereobj);
					marshall_free(where_elementobj);
					marshall_free(indexes);
					goto where_done;
				}
			}

			marshall_free(indexes);
		}

		whereobj = marshall_condition(where_elementobj, dataobj);
		marshall_free(where_elementobj);
	}

where_done:

	/* Selector */
	if (select_element) {
		marshall_t *select_elementobj = marshall_convert((char *)select_element, strlen(select_element));
		if (!select_elementobj) {
			if (data)
				zfree(data);
			marshall_free(dataobj);
			return NULL;
		}

		/* Selector type */
		if (select_elementobj->type != MTYPE_ARRAY && select_elementobj->type != MTYPE_STRING) {
			error_throw("14d882da30d9", "Operation expects an string or array given");
			if (data)
				zfree(data);
			marshall_free(select_elementobj);
			marshall_free(dataobj);
			return NULL;
		}

		selectobj = marshall_filter(select_elementobj, whereobj ? whereobj : dataobj, NULL);
		marshall_free(select_elementobj);
	}

	char *buf = marshall_serialize(selectobj ? selectobj : whereobj);
	if (data)
		zfree(data);

	if (whereobj)
		marshall_free(whereobj);
	marshall_free(selectobj);
	marshall_free(dataobj);
	return buf;
}

int db_item_add(char *quid, int *items, const void *ndata, size_t ndata_len) {
	quid_t key;
	size_t _len;
	size_t len = 0;
	struct metadata meta;
	marshall_t *newobject = NULL;
	slay_result_t nrs;
	schema_t schema;
	strtoquid(quid, &key);

	if (!ready)
		return -1;

	marshall_t *mergeobj = marshall_convert((char *)ndata, ndata_len);
	if (!mergeobj) {
		return -1;
	}

	uint64_t offset = engine_get(&control, &key, &meta);
	switch (meta.type) {
		case MD_TYPE_RECORD: {
			void *data = get_data_block(&control, offset, &_len);
			if (!data) {
				marshall_free(mergeobj);
				return -1;
			}

			marshall_t *dataobj = slay_get(&control, data, NULL, TRUE);
			if (!dataobj) {
				zfree(data);
				marshall_free(mergeobj);
				return -1;
			}

			newobject = marshall_merge(mergeobj, dataobj);
			zfree(data);
			break;
		}
		case MD_TYPE_GROUP: {
			void *data = get_data_block(&control, offset, &_len);
			if (!data) {
				return -1;
			}

			schema = slay_get_schema(data);
			marshall_t *dataobj = slay_get(&control, data, NULL, FALSE);
			if (!dataobj) {
				zfree(data);
				return -1;
			}

			quid_t newkey;
			quid_create(&newkey);
			char squid[QUID_LENGTH + 1];
			quidtostr(squid, &newkey);
			if (schema == SCHEMA_TABLE) {

				/* Insert new item as separate record */
				void *newslay = slay_put(&control, mergeobj, &len, &nrs);
				if (engine_insert_data(&control, &newkey, newslay, len) < 0) {
					zfree(newslay);
					marshall_free(mergeobj);
					return -1;
				}
				zfree(newslay);

				if (mergeobj->type == MTYPE_OBJECT) {

					/* Add item to indexes on this group */
					marshall_t *indexes = index_list_on_group(&control, &key);
					if (indexes) {
						quid_t index_key;

						for (unsigned int i = 0; i < mergeobj->size; ++i) {

							/* Does the indexed element match the condition */
							if (!strcmp(mergeobj->child[i]->name, indexes->child[0]->child[1]->data)) {
								struct metadata newkey_meta;

								strtoquid(indexes->child[0]->child[0]->data, &index_key);
								uint64_t newkey_offset = engine_get(&control, &newkey, &newkey_meta);

								uint64_t index_offset = index_list_get_index_offset(&control, &index_key);
								index_add(&control, index_offset, mergeobj->child[i]->data, newkey_offset);
							}
						}

						marshall_free(indexes);
					}

				}
				marshall_free(mergeobj);

				/* Append key to group */
				mergeobj = marshall_convert(squid, QUID_LENGTH);
				if (!mergeobj) {
					zfree(data);
					marshall_free(dataobj);
					return -1;
				}

				newobject = marshall_merge(mergeobj, dataobj);
			} else {

				/* Add record as set item */
				if (marshall_type_hasdescent(mergeobj->type)) {
					if (mergeobj->child[0]) {
						if (mergeobj->child[0]->name_len) {

							/* Insert new item as separate record */
							void *newslay = slay_put(&control, mergeobj->child[0], &len, &nrs);
							if (engine_insert_data(&control, &newkey, newslay, len) < 0) {
								zfree(newslay);
								marshall_free(mergeobj);
								return -1;
							}
							zfree(newslay);

							marshall_t *old_mergeobj = mergeobj;

							mergeobj = (marshall_t *)tree_zcalloc(1, sizeof(marshall_t), NULL);
							mergeobj->child = (marshall_t **)tree_zcalloc(1, sizeof(marshall_t *), mergeobj);
							mergeobj->type = MTYPE_OBJECT;
							mergeobj->size = 1;

							mergeobj->child[0] = (marshall_t *)tree_zcalloc(1, sizeof(marshall_t), mergeobj);
							mergeobj->child[0]->type = MTYPE_QUID;
							mergeobj->child[0]->name = tree_zstrndup(old_mergeobj->child[0]->name, old_mergeobj->child[0]->name_len, mergeobj);
							mergeobj->child[0]->name_len = old_mergeobj->child[0]->name_len;
							mergeobj->child[0]->data = tree_zstrndup(squid, QUID_LENGTH, mergeobj);
							mergeobj->child[0]->data_len = QUID_LENGTH;
							marshall_free(old_mergeobj);

							newobject = marshall_merge(mergeobj, dataobj);
						}
					}
				}

				/* No set, insert as is and take key as name */
				if (!newobject) {

					/* Insert new item as separate record */
					void *newslay = slay_put(&control, mergeobj, &len, &nrs);
					if (engine_insert_data(&control, &newkey, newslay, len) < 0) {
						zfree(newslay);
						marshall_free(mergeobj);
						return -1;
					}
					zfree(newslay);
					marshall_free(mergeobj);

					mergeobj = (marshall_t *)tree_zcalloc(1, sizeof(marshall_t), NULL);
					mergeobj->child = (marshall_t **)tree_zcalloc(1, sizeof(marshall_t *), mergeobj);
					mergeobj->type = MTYPE_OBJECT;
					mergeobj->size = 1;

					mergeobj->child[0] = (marshall_t *)tree_zcalloc(1, sizeof(marshall_t), mergeobj);
					mergeobj->child[0]->type = MTYPE_QUID;
					mergeobj->child[0]->name = tree_zstrndup(squid, QUID_LENGTH, mergeobj);
					mergeobj->child[0]->name_len = QUID_LENGTH;
					mergeobj->child[0]->data = tree_zstrndup(squid, QUID_LENGTH, mergeobj);
					mergeobj->child[0]->data_len = QUID_LENGTH;
					newobject = marshall_merge(mergeobj, dataobj);
				}
			}
			zfree(data);
			break;
		}
		case MD_TYPE_INDEX:
		default:
			error_throw("0fb1dd21b0fd", "Internal records cannot be altered");
			return -1;
	}

	if (iserror()) {
		marshall_free(mergeobj);
		marshall_free(newobject);
		return -1;
	}

	void *dataslay = slay_put(&control, newobject, &len, &nrs);
	*items = nrs.items;
	nrs.schema = schema;
	slay_update_row(dataslay, &nrs);
	if (engine_update_data(&control, &key, dataslay, len) < 0) {
		zfree(dataslay);
		marshall_free(mergeobj);
		marshall_free(newobject);
		return -1;
	}

	zfree(dataslay);
	marshall_free(mergeobj);
	marshall_free(newobject);

	return 0;
}

int db_item_remove(char *quid, int *items, const void *ndata, size_t ndata_len) {
	quid_t key;
	size_t _len;
	size_t len = 0;
	struct metadata meta;
	slay_result_t nrs;
	strtoquid(quid, &key);

	if (!ready)
		return -1;

	*items = 0;
	marshall_t *mergeobj = marshall_convert((char *)ndata, ndata_len);
	if (!mergeobj) {
		return -1;
	}

	uint64_t offset = engine_get(&control, &key, &meta);
	switch (meta.type) {
		case MD_TYPE_RECORD: {
			void *data = get_data_block(&control, offset, &_len);
			if (!data) {
				marshall_free(mergeobj);
				return -1;
			}

			marshall_t *dataobj = slay_get(&control, data, NULL, TRUE);
			if (!dataobj) {
				zfree(data);
				marshall_free(mergeobj);
				return -1;
			}
			zfree(data);

			bool alteration = FALSE;
			marshall_t *filterobject = marshall_separate(mergeobj, dataobj, &alteration);

			if (!alteration) {
				error_throw("6b4f4d9c00fc", "Cannot separate structures");
				marshall_free(mergeobj);
				marshall_free(filterobject);
				return -1;
			}

			void *dataslay = slay_put(&control, filterobject, &len, &nrs);
			*items = nrs.items;
			if (engine_update_data(&control, &key, dataslay, len) < 0) {
				zfree(dataslay);
				marshall_free(mergeobj);
				marshall_free(filterobject);
				return -1;
			}

			marshall_free(filterobject);
			marshall_free(mergeobj);
			zfree(dataslay);
			return 0;
		}
		case MD_TYPE_GROUP: {
			void *data = get_data_block(&control, offset, &_len);
			if (!data) {
				marshall_free(mergeobj);
				return -1;
			}

			schema_t schema = slay_get_schema(data);
			marshall_t *dataobj = slay_get(&control, data, NULL, FALSE);
			if (!dataobj) {
				zfree(data);
				marshall_free(mergeobj);
				return -1;
			}
			zfree(data);

			for (unsigned int i = 0; i < dataobj->size; ++i) {
				quid_t row_key;
				size_t row_len;
				struct metadata row_meta;
				strtoquid(dataobj->child[i]->data, &row_key);

				uint64_t row_offset = engine_get(&control, &row_key, &row_meta);
				void *row_data = get_data_block(&control, row_offset, &row_len);
				if (!row_data) {
					continue;
				}

				marshall_t *row_dataobj = slay_get(&control, row_data, NULL, TRUE);
				if (!row_dataobj) {
					zfree(row_data);
					continue;
				}

				if (schema == SCHEMA_TABLE) {
					if (marshall_equal(mergeobj, row_dataobj)) {
						marshall_t *rmobj = (marshall_t *)tree_zcalloc(1, sizeof(marshall_t), NULL);
						rmobj->child = (marshall_t **)tree_zcalloc(1, sizeof(marshall_t *), rmobj);
						rmobj->type = MTYPE_QUID;
						rmobj->data = dataobj->child[i]->data;
						rmobj->data_len = QUID_LENGTH;
						rmobj->size = 1;

						if (mergeobj->type == MTYPE_OBJECT) {

							/* Add item to indexes on this group */
							marshall_t *indexes = index_list_on_group(&control, &key);
							if (indexes) {
								quid_t index_key;

								for (unsigned int j = 0; j < mergeobj->size; ++j) {

									/* Does the indexed element match the condition */
									if (!strcmp(mergeobj->child[j]->name, indexes->child[0]->child[1]->data)) {
										strtoquid(indexes->child[0]->child[0]->data, &index_key);

										uint64_t index_offset = index_list_get_index_offset(&control, &index_key);
										index_delete(&control, index_offset, mergeobj->child[j]->data);
									}
								}

								marshall_free(indexes);
							}

						}

						engine_delete(&control, &row_key);

						if (row_meta.alias)
							alias_delete(&control, &row_key);

						bool alteration = FALSE;
						marshall_t *filterobject = marshall_separate(rmobj, dataobj, &alteration);

						/* If anything changed write back the new list */
						if (alteration) {
							void *dataslay = slay_put(&control, filterobject, &len, &nrs);
							*items = nrs.items;
							nrs.schema = schema;
							slay_update_row(dataslay, &nrs);
							if (engine_update_data(&control, &key, dataslay, len) < 0) {
								marshall_free(rmobj);
								marshall_free(row_dataobj);
								zfree(row_data);
								continue;
							}
							zfree(dataslay);
						}
						marshall_free(rmobj);
						marshall_free(row_dataobj);
						zfree(row_data);
						break;
					}
				} else {
					/* Remove record as set item */
					if (marshall_type_hasdescent(mergeobj->type)) {
						if (mergeobj->child[0]) {
							if (mergeobj->child[0]->name_len) {
								if (!strcmp(mergeobj->child[0]->name, dataobj->child[i]->name)) {

									/* Remove name to match */
									mergeobj->child[0]->name = NULL;
									mergeobj->child[0]->name_len = 0;
									if (marshall_equal(mergeobj->child[0], row_dataobj)) {
										engine_delete(&control, &row_key);

										if (row_meta.alias)
											alias_delete(&control, &row_key);

										marshall_t *rmobj = (marshall_t *)tree_zcalloc(1, sizeof(marshall_t), NULL);
										rmobj->child = (marshall_t **)tree_zcalloc(1, sizeof(marshall_t *), rmobj);
										rmobj->type = MTYPE_QUID;
										rmobj->name = dataobj->child[i]->name;
										rmobj->name_len = dataobj->child[i]->name_len;
										rmobj->data = dataobj->child[i]->data;
										rmobj->data_len = QUID_LENGTH;
										rmobj->size = 1;

										bool alteration = FALSE;
										marshall_t *filterobject = marshall_separate(rmobj, dataobj, &alteration);

										/* If anything changed write back the new list */
										if (alteration) {
											if (!filterobject->size) {
												filterobject->type = MTYPE_NULL;
												filterobject->size = 1;
											}

											void *dataslay = slay_put(&control, filterobject, &len, &nrs);
											*items = nrs.items;
											nrs.schema = schema;
											slay_update_row(dataslay, &nrs);
											if (engine_update_data(&control, &key, dataslay, len) < 0) {
												marshall_free(rmobj);
												marshall_free(row_dataobj);
												zfree(row_data);
												continue;
											}
											zfree(dataslay);
										}
										marshall_free(rmobj);
										marshall_free(row_dataobj);
										zfree(row_data);
										break;
									}
								}
							}
						}
					}
				}

				marshall_free(row_dataobj);
				zfree(row_data);
			}

			marshall_free(dataobj);
			marshall_free(mergeobj);
			error_clear();
			return 0;
		}
		case MD_TYPE_INDEX:
		default:
			error_throw("0fb1dd21b0fd", "Internal records cannot be altered");
			return -1;
	}

	error_throw("6b4f4d9c00fc", "Cannot separate structures");
	return -1;
}

int db_record_get_meta(char *quid, bool force, struct record_status * status) {
	quid_t key;
	struct metadata meta;
	strtoquid(quid, &key);

	if (!ready)
		return -1;

	if (force) {
		engine_get_force(&control, &key, &meta);
	} else {
		engine_get(&control, &key, &meta);
	}

	status->syslock = meta.syslock;
	status->exec = meta.exec;
	status->freeze = meta.freeze;
	status->nodata = meta.nodata;
	status->importance = meta.importance;
	status->has_alias = 0;

	if (meta.alias) {
		status->has_alias = 1;
		char *name = alias_get_val(&control, &key);
		strlcpy(status->alias, name, STATUS_ALIAS_LENGTH);
		zfree(name);
	}
	strlcpy(status->lifecycle, get_str_lifecycle(meta.lifecycle), STATUS_LIFECYCLE_SIZE);
	strlcpy(status->type, get_str_type(meta.type), STATUS_TYPE_SIZE);
	return 0;
}

int db_record_set_meta(char *quid, struct record_status * status) {
	quid_t key;
	struct metadata meta;
	strtoquid(quid, &key);

	if (!ready)
		return -1;

	nullify(&meta, sizeof(struct metadata));
	meta.syslock = status->syslock;
	meta.exec = status->exec;
	meta.freeze = status->freeze;
	meta.importance = status->importance;
	meta.lifecycle = get_meta_lifecycle(status->lifecycle);
	if (engine_setmeta(&control, &key, &meta) < 0)
		return -1;

	return 0;
}

char *db_index_on_group(char *quid) {
	quid_t key;
	strtoquid(quid, &key);

	if (!ready)
		return NULL;

	marshall_t *dataobj = index_list_on_group(&control, &key);
	char *buf = marshall_serialize(dataobj);
	marshall_free(dataobj);
	return buf;
}

char *db_alias_get_name(char *quid) {
	quid_t key;
	strtoquid(quid, &key);

	if (!ready)
		return NULL;

	return alias_get_val(&control, &key);
}

int db_alias_update(char *quid, const char *name) {
	quid_t key, _key;
	struct metadata meta;
	strtoquid(quid, &key);

	if (!ready)
		return -1;

	/* Does name already exist */
	if (!alias_get_key(&control, &_key, name, strlen(name))) {
		error_throw("a09c8843b09d", "Alias exists");
		return -1;
	}
	error_clear();

	/* Check if key has alias */
	engine_get(&control, &key, &meta);
	if (!meta.alias) {
		alias_add(&control, &key, name, strlen(name));

		meta.alias = 1;
		if (engine_setmeta(&control, &key, &meta) < 0)
			return -1;
		return 0;
	}

	return alias_update(&control, &key, name, strlen(name));;
}

char *db_alias_all() {
	if (!ready)
		return NULL;

	marshall_t *dataobj = alias_all(&control);
	if (!dataobj) {
		return NULL;
	}

	char *buf = marshall_serialize(dataobj);
	marshall_free(dataobj);
	return buf;
}

char *db_index_all() {
	if (!ready)
		return NULL;

	marshall_t *dataobj = index_list_all(&control);
	if (!dataobj) {
		return NULL;
	}

	char *buf = marshall_serialize(dataobj);
	marshall_free(dataobj);
	return buf;
}

char *db_pager_all() {
	if (!ready)
		return NULL;

	marshall_t *dataobj = pager_all(&control);
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

	if (alias_get_key(&control, &key, name, strlen(name)) < 0)
		return NULL;

	uint64_t offset = engine_get(&control, &key, &meta);
	switch (meta.type) {
		case MD_TYPE_RECORD:
		case MD_TYPE_GROUP: {
			data = get_data_block(&control, offset, &_len);
			if (!data)
				return NULL;

			dataobj = slay_get(&control, data, NULL, descent);
			if (!dataobj) {
				zfree(data);
				return NULL;
			}
			break;
		}
		case MD_TYPE_INDEX: {
			uint64_t index_offset = index_list_get_index_offset(&control, &key);
			dataobj = index_btree_all(&control, index_offset, descent);
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

int db_index_rebuild(char *quid, int *items) {
	quid_t key;
	index_result_t inrs;
	struct metadata meta, group_meta;
	strtoquid(quid, &key);
	nullify(&inrs, sizeof(index_result_t));

	engine_get(&control, &key, &meta);
	if (meta.type != MD_TYPE_INDEX) {
		error_throw("1e933eea602c", "Invalid record type");
		return -1;
	}

	/* Index properties */
	char *element = index_list_get_index_element(&control, &key);
	quid_t *group = index_list_get_index_group(&control, &key);

	size_t len;
	uint64_t offset = engine_get(&control, group, &group_meta);
	void *data = get_data_block(&control, offset, &len);
	if (!data) {
		return -1;
	}
	zfree(group);

	schema_t schema = slay_get_schema(data);
	marshall_t *dataobj = slay_get(&control, data, NULL, FALSE);
	if (!dataobj) {
		zfree(data);
		return -1;
	}
	zfree(data);

	/* Determine index based on dataschema */
	switch (schema) {
		case SCHEMA_TABLE:
			index_btree_create_table(&control, element, dataobj, &inrs);
			break;
		case SCHEMA_SET:
			index_btree_create_set(&control, element, dataobj, &inrs);
			break;
		default:
			error_throw("ece28bc980db", "Invalid schema");
	}

	*items = inrs.index_elements;
	if (*items < 2) {
		error_throw("3d2a88a4502b", "Too few items for index");
		zfree(element);
		marshall_free(dataobj);
		return 0;
	}

	/* Update index item with new offset */
	index_list_update_offset(&control, &key, inrs.offset);

	zfree(element);
	marshall_free(dataobj);
	return 0;
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
	nullify(&nrs, sizeof(index_result_t));

	if (!ready)
		return -1;

	quid_create(&nrs.index);
	quidtostr(index_quid, &nrs.index);

	uint64_t offset = engine_get(&control, &key, &meta);
	if (meta.type != MD_TYPE_GROUP) {
		error_throw("1e933eea602c", "Invalid record type");
		return -1;
	}

	void *data = get_data_block(&control, offset, &_len);
	if (!data)
		return -1;

	marshall_t *dataobj = slay_get(&control, data, NULL, FALSE);
	if (!dataobj) {
		zfree(data);
		return -1;
	}

	if (marshall_count(dataobj) < 2) {
		error_throw("3d2a88a4502b", "Too few items for index");
		return 0;
	}

	/* Determine index based on dataschema */
	schema_t group = slay_get_schema(data);
	switch (group) {
		case SCHEMA_TABLE:
			index_btree_create_table(&control, idxkey, dataobj, &nrs);
			break;
		case SCHEMA_SET:
			index_btree_create_set(&control, idxkey, dataobj, &nrs);
			break;
		default:
			error_throw("ece28bc980db", "Invalid schema");
	}

	marshall_free(dataobj);
	zfree(data);

	*items = nrs.index_elements;
	if (*items < 2) {
		error_throw("3d2a88a4502b", "Too few items for index");
		return 0;
	}

	/* Add index to database */
	nullify(&meta, sizeof(struct metadata));
	meta.nodata = 1;
	meta.type = MD_TYPE_INDEX;
	meta.alias = 1;
	engine_insert_meta(&control, &nrs.index, &meta);

	/* Add index to alias list */
	alias_add(&control, &nrs.index, index_quid, QUID_LENGTH);

	/* Add index to index list */
	index_list_add(&control, &nrs.index, &key, (char *)idxkey, INDEX_BTREE, nrs.offset);
	return 0;
}
