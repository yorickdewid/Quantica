#ifndef CORE_H_INCLUDED
#define CORE_H_INCLUDED

#include "quid.h"
#include "time.h"
#include "sql.h"
#include "marshall.h"
#include "engine.h"

#define STATUS_LIFECYCLE_SIZE 10
#define STATUS_TYPE_SIZE 12
#define STATUS_ALIAS_LENGTH 48

struct record_status {
	char lifecycle[STATUS_LIFECYCLE_SIZE];
	int importance;
	bool syslock;
	bool exec;
	bool freeze;
	bool nodata;
	bool has_alias;
	char type[STATUS_TYPE_SIZE];
	char alias[STATUS_ALIAS_LENGTH];
};

/*
 * Core control
 */
void start_core();
void detach_core();

char *get_zero_key();
bool get_ready_status();
void set_instance_name(char name[]);
char *get_instance_name();
char *get_instance_key();
char *get_session_key();
char *get_pager_alloc_size();
char *get_total_disk_size();
unsigned int get_pager_page_size();
unsigned int get_pager_page_count();
char *get_dataheap_name();
char *get_instance_prefix_key(char *short_quid);
char *get_uptime();
int crypto_sha1(char *s, const char *data);
int crypto_md5(char *s, const char *data);
int crypto_sha256(char *s, const char *data);
int crypto_sha512(char *s, const char *data);
int crypto_hmac_sha256(char *s, const char *key, const char *data);
int crypto_hmac_sha512(char *s, const char *key, const char *data);
sqlresult_t *exec_sqlquery(const char *query, size_t *len);
char *crypto_base64_enc(const char *data);
char *crypto_base64_dec(const char *data);

char *auth_token(char *key);

unsigned long int stat_getkeys();
unsigned long int stat_getfreekeys();
unsigned long int stat_getfreeblocks();
unsigned long int stat_tablesize();
unsigned long int stat_indexsize();
int generate_random_number(int range);
void quid_generate(char *quid);
void quid_generate_short(char *quid);
void filesync();
int zvacuum(int page_size);

/*
 * Database operations
 */
char *key_decode(char *quid);
int db_put(char *quid, int *items, const void *data, size_t len, char *hint, char *hint_option);
void *db_get(char *quid, size_t *len, bool descent, bool force);
char *db_get_type(char *quid);
char *db_get_schema(char *quid);
char *db_get_history(char *quid);
char *db_get_version(char *quid, char *element);
int db_count_group(char *quid);
int db_update(char *quid, int *items, bool descent, const void *data, size_t data_len);
int db_duplicate(char *quid, char *nquid, int *items, bool copy_meta);
int db_delete(char *quid, bool descent);
int db_purge(char *quid, bool descent);
void *db_select(char *quid, const char *select_element, const char *where_element);
int db_item_add(char *quid, int *items, const void *ndata, size_t ndata_len);
int db_item_remove(char *quid, int *items, const void *ndata, size_t ndata_len);

int db_record_get_meta(char *quid, bool force, struct record_status *status);
int db_record_set_meta(char *quid, struct record_status *status);

char *db_alias_get_name(char *quid);
char *db_index_on_group(char *quid);
int db_alias_update(char *quid, const char *name);
char *db_alias_all();
char *db_index_all();
char *db_pager_all();
void *db_alias_get_data(char *name, size_t *len, bool descent);

int db_index_rebuild(char *quid, int *items);
int db_index_create(char *group_quid, char *index_quid, int *items, const char *idxkey);

#endif // CORE_H_INCLUDED
