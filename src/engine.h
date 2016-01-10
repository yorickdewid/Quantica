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

#define INSTANCE_LENGTH 32

typedef struct base base_t;

enum key_lifecycle {
	MD_LIFECYCLE_FINITE = 0,
	MD_LIFECYCLE_INVALID,
	MD_LIFECYCLE_CORRUPT,
	MD_LIFECYCLE_RECYCLE,
	MD_LIFECYCLE_INACTIVE,
	MD_LIFECYCLE_TIMETOLIVE,
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
	unsigned long long offset;
	struct _engine_table *table;
};

struct engine_dbcache {
	unsigned int len;
	unsigned long long offset;
};

typedef struct engine {
	unsigned long long top;
	unsigned long long free_top;
	unsigned long long last_block;
	bool lock;
	struct engine_cache cache[CACHE_SLOTS];
	struct engine_dbcache dbcache[DBCACHE_SLOTS];
} engine_t;

bool engine_keytype_hasdata(enum key_type type);

/*
 * Open or Creat an existing database file.
 */
// void engine_init(engine_t *engine, const char *fname, const char *dbname);
void engine_init(base_t *base);

/*
 * Close a database file opened with engine_create() or engine_open().
 */
void engine_close(base_t *base);

/*
 * Insert a new item with key 'quid' with the contents in 'data' to the
 * database file.
 */
int engine_insert_data(base_t *base, quid_t *quid, const void *data, size_t len);
int engine_insert_meta_data(base_t *base, quid_t *quid, struct metadata *meta, const void *data, size_t len);
int engine_insert_meta(base_t *base, quid_t *quid, struct metadata *meta);
int engine_insert(base_t *base, quid_t *quid);

/*
 * Look up item with the given key 'quid' in the database file. Length of the
 * item is stored in 'len'. Returns a pointer to the contents of the item.
 * The returned pointer should be released with free() after use.
 */
void *get_data_block(base_t *base, unsigned long long offset, size_t *len);
unsigned long long engine_get(base_t *base, const quid_t *quid, struct metadata *meta);
unsigned long long engine_get_force(base_t *base, const quid_t *quid, struct metadata *meta);

/*
 * Remove item with the given key 'quid' from the database file.
 */
int engine_purge(base_t *base, quid_t *quid);

void engine_sync(base_t *base);

int engine_setmeta(base_t *base, const quid_t *quid, const struct metadata *data);

int engine_delete(base_t *base, const quid_t *quid);

#ifdef DEBUG
void engine_traverse(const base_t *base, unsigned long long table_offset);
#endif

int engine_rebuild(base_t *base, base_t *new_base);
int engine_update_data(base_t *base, const quid_t *quid, const void *data, size_t len);

char *get_str_lifecycle(enum key_lifecycle lifecycle);
char *get_str_type(enum key_type key_type);
enum key_lifecycle get_meta_lifecycle(char *lifecycle);
enum key_type get_meta_type(char *key_type);

#endif // ENGINE_H_INCLUDED
