#ifndef ENGINE_H_INCLUDED
#define ENGINE_H_INCLUDED

#include <stdio.h>
#include <fcntl.h>
#include <stdint.h>

#include <config.h>
#include <common.h>
#include "quid.h"
#include "marshall.h"

#define TABLE_SIZE	((4096 - 1) / sizeof(struct engine_item))
#define LIST_SIZE	((8192 - 1) / sizeof(struct engine_tablelist_item))

#define DBNAME_SIZE	64
#define INSTANCE_LENGTH 32
#define LIST_NAME_LENGTH 48

enum key_lifecycle {
	MD_LIFECYCLE_FINITE = 0,
	MD_LIFECYCLE_INVALID,
	MD_LIFECYCLE_CORRUPT,
	MD_LIFECYCLE_RECYCLE,
	MD_LIFECYCLE_INACTIVE,
	MD_LIFECYCLE_UNKNOWN = 31
};

enum key_importance {
	MD_IMPORTANT_CRITICAL,
	MD_IMPORTANT_LEVEL1,
	MD_IMPORTANT_LEVEL2,
	MD_IMPORTANT_LEVEL3,
	MD_IMPORTANT_LEVEL4,
	MD_IMPORTANT_NORMAL,
	MD_IMPORTANT_LEVEL6,
	MD_IMPORTANT_LEVEL7,
	MD_IMPORTANT_LEVEL8,
	MD_IMPORTANT_LEVEL9,
	MD_IMPORTANT_IRRELEVANT
};

enum key_type {
	MD_TYPE_RECORD = 0,		/* Key maps to record */
	MD_TYPE_GROUP,			/* Key represents a table or set */
	MD_TYPE_INDEX,			/* Key points to index */
	MD_TYPE_RAW				/* Key for internal strucuture */
};

struct metadata {
	unsigned int lifecycle	: 5;	/* Record lifecycle */
	unsigned int importance	: 4;	/* Relative importance */
	unsigned int syslock	: 1;	/* System lock */
	unsigned int exec		: 1;	/* Is executable UNUSED */
	unsigned int freeze		: 1;	/* Management lock */
	unsigned int nodata		: 1;	/* Indicates if the key contains data */
	unsigned int type		: 3;	/* Additional flags */
	unsigned int _res		: 16;	/* Reserved */
};

struct engine_item {
	quid_t quid;
	struct metadata meta;
	__be64 offset;
	__be64 child;
} __attribute__((packed));

struct engine_table {
	struct engine_item items[TABLE_SIZE];
	uint16_t size;
} __attribute__((packed));

struct engine_cache {
	uint64_t offset;
	struct engine_table *table;
};

struct engine_dbcache {
	__be32 len;
	uint64_t offset;
};

struct engine_tablelist_item {
	quid_t quid;
	__be32 len;
	__be32 hash;
	char name[LIST_NAME_LENGTH];
} __attribute__((packed));

struct engine_tablelist {
	struct engine_tablelist_item items[LIST_SIZE];
	uint16_t size;
	__be64 link;
};

struct blob_info {
	__be32 len;
	__be64 next;
	bool free;
} __attribute__((packed));

struct engine_super {
	__be32 version;
	__be64 top;
	__be64 free_top;
	__be64 nkey;
	__be64 nfree_table;
	__be64 crc_zero_key;
	__be64 list_top;
	__be64 list_size;
	char instance[INSTANCE_LENGTH];
} __attribute__((packed));

struct engine_dbsuper {
	__be32 version;
	__be64 last;
} __attribute__((packed));

struct engine_stats {
	uint64_t keys;
	uint64_t free_tables;
	uint64_t list_size;
};

struct engine {
	uint64_t top;
	uint64_t free_top;
	uint64_t alloc;
	uint64_t db_alloc;
	uint64_t list_top;
	int fd;
	int db_fd;
	bool lock;
	struct engine_stats stats;
	struct engine_cache cache[CACHE_SLOTS];
	struct engine_dbcache dbcache[DBCACHE_SLOTS];
};

/*
 * Open or Creat an existing database file.
 */
void engine_init(struct engine *e, const char *fname, const char *dbname);

/*
 * Close a database file opened with engine_create() or engine_open().
 */
void engine_close(struct engine *e);

/*
 * Insert a new item with key 'quid' with the contents in 'data' to the
 * database file.
 */
int engine_insert_data(struct engine *e, quid_t *quid, const void *data, size_t len);
int engine_insert_meta_data(struct engine *e, quid_t *quid, struct metadata *meta, const void *data, size_t len);
int engine_insert_meta(struct engine *e, quid_t *quid, struct metadata *meta);
int engine_insert(struct engine *e, quid_t *quid);

/*
 * Look up item with the given key 'quid' in the database file. Length of the
 * item is stored in 'len'. Returns a pointer to the contents of the item.
 * The returned pointer should be released with free() after use.
 */
uint64_t engine_get(struct engine *e, const quid_t *quid);
void *get_data(struct engine *e, uint64_t offset, size_t *len);

/*
 * Remove item with the given key 'quid' from the database file.
 */
int engine_purge(struct engine *e, quid_t *quid);

void engine_sync(struct engine *e);

//uint64_t engine_get_data_offset(struct engine *e, const quid_t *quid);

int engine_getmeta(struct engine *e, const quid_t *quid, struct metadata *md);

int engine_setmeta(struct engine *e, const quid_t *quid, const struct metadata *data);

int engine_delete(struct engine *e, const quid_t *quid);

int engine_recover_storage(struct engine *e);
int engine_vacuum(struct engine *e, const char *fname, const char *nfname);
int engine_update_data(struct engine *e, const quid_t *quid, const void *data, size_t len);

int engine_list_insert(struct engine *e, const quid_t *c_quid, const char *name, size_t len);
char *engine_list_get_val(struct engine *e, const quid_t *c_quid);
int engine_list_get_key(struct engine *e, quid_t *key, const char *name, size_t len);
int engine_list_update(struct engine *e, const quid_t *c_quid, const char *name, size_t len);
int engine_list_delete(struct engine *e, const quid_t *c_quid);
marshall_t *engine_list_all(struct engine *e);

char *get_str_lifecycle(enum key_lifecycle lifecycle);
char *get_str_type(enum key_type key_type);
enum key_lifecycle get_meta_lifecycle(char *lifecycle);
enum key_type get_meta_type(char *key_type);

#endif // ENGINE_H_INCLUDED
