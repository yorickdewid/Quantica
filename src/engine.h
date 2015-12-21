#ifndef ENGINE_H_INCLUDED
#define ENGINE_H_INCLUDED

#include <stdio.h>
#include <fcntl.h>
#include <stdint.h>

#include <config.h>
#include <common.h>
#include "quid.h"
#include "marshall.h"
#include "base.h"

#define DBNAME_SIZE	64
#define INSTANCE_LENGTH 32
#define LIST_NAME_LENGTH 48 // deprecated by alias

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
	unsigned int lifecycle	: 5;	/* Record lifecycle, key_lifecycle */
	unsigned int importance	: 4;	/* Relative importance, key_importance */
	unsigned int syslock	: 1;	/* System lock */
	unsigned int exec		: 1;	/* Is executable UNUSED */
	unsigned int freeze		: 1;	/* Management lock */
	unsigned int nodata		: 1;	/* Indicates if the key contains data */
	unsigned int alias		: 1;	/* Key is aliased */
	unsigned int type		: 3;	/* Additional flags, key_type */
	unsigned int _res		: 15;	/* Reserved */
};

struct engine_cache {
	uint64_t offset;
	struct _engine_table *table;
};

struct engine_dbcache {
	unsigned int len;
	uint64_t offset;
};

struct engine_stats { //TODO deprecated by base.stats
	uint64_t keys;
	uint64_t free_tables;
	uint64_t list_size;
	uint64_t index_list_size;
};

typedef struct {
	uint64_t top;
	uint64_t free_top;
	uint64_t alloc;
	uint64_t db_alloc;
	uint64_t list_top;
	uint64_t index_list_top;
	int fd;
	int db_fd;
	bool lock;
	base_t *base; //TODO overgangsregeling :)
	struct engine_stats stats;
	struct engine_cache cache[CACHE_SLOTS];
	struct engine_dbcache dbcache[DBCACHE_SLOTS];
} engine_t;

bool engine_keytype_hasdata(enum key_type type);

/*
 * Open or Creat an existing database file.
 */
void engine_init(engine_t *e, const char *fname, const char *dbname);

/*
 * Close a database file opened with engine_create() or engine_open().
 */
void engine_close(engine_t *e);

/*
 * Insert a new item with key 'quid' with the contents in 'data' to the
 * database file.
 */
int engine_insert_data(engine_t *e, quid_t *quid, const void *data, size_t len);
int engine_insert_meta_data(engine_t *e, quid_t *quid, struct metadata *meta, const void *data, size_t len);
int engine_insert_meta(engine_t *e, quid_t *quid, struct metadata *meta);
int engine_insert(engine_t *e, quid_t *quid);

/*
 * Look up item with the given key 'quid' in the database file. Length of the
 * item is stored in 'len'. Returns a pointer to the contents of the item.
 * The returned pointer should be released with free() after use.
 */
uint64_t engine_get(engine_t *e, const quid_t *quid, struct metadata *meta);
void *get_data_block(engine_t *e, uint64_t offset, size_t *len);

/*
 * Remove item with the given key 'quid' from the database file.
 */
int engine_purge(engine_t *e, quid_t *quid);

void engine_sync(engine_t *e);

int engine_setmeta(engine_t *e, const quid_t *quid, const struct metadata *data);

int engine_delete(engine_t *e, const quid_t *quid);

int engine_recover_storage(engine_t *e);
int engine_vacuum(engine_t *e, const char *fname, const char *nfname);
int engine_update_data(engine_t *e, const quid_t *quid, const void *data, size_t len);

char *get_str_lifecycle(enum key_lifecycle lifecycle);
char *get_str_type(enum key_type key_type);
enum key_lifecycle get_meta_lifecycle(char *lifecycle);
enum key_type get_meta_type(char *key_type);

#endif // ENGINE_H_INCLUDED
