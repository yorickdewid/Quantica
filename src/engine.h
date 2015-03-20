#ifndef ENGINE_H_INCLUDED
#define ENGINE_H_INCLUDED

#include <stdio.h>
#include <fcntl.h>
#include <stdint.h>
#include <config.h>
#include <common.h>

#define TABLE_SIZE	((4096 - 1) / sizeof(struct btree_item))

#define DBEXT	".db"
#define IDXEXT	".idx"
#define LOGEXT	".log"
#define CDBEXT	"._db"
#define CIDXEXT	"._idx"
#define CLOGEXT	"._log"
#define BDBEXT	".db1"
#define BIDXEXT	".idx1"
#define BLOGEXT	".log1"

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
    MD_TYPE_DATA = 0,
	MD_TYPE_SIGNED,
	MD_TYPE_BOOL_FALSE,
	MD_TYPE_BOOL_TRUE,
	MD_TYPE_POINTER
};

struct microdata {
	uint16_t lifecycle	: 5;
	uint16_t importance	: 4;
	uint16_t syslock	: 1;
	uint16_t exec		: 1;
	uint16_t freeze		: 1;
	uint16_t error		: 1;
	uint16_t type	    : 3;
};

struct btree_item {
     quid_t quid;
     struct microdata meta;
     __be64 offset;
     __be64 child;
} __attribute__((packed));

struct btree_table {
     struct btree_item items[TABLE_SIZE];
     uint8_t size;
} __attribute__((packed));

struct btree_cache {
     uint64_t offset;
     struct btree_table *table;
};

struct btree_dbcache {
	__be32 len;
	uint64_t offset;
};

struct blob_info {
     __be32 len;
	uint8_t free;
} __attribute__((packed));

struct btree_super {
	char signature[8];
     __be64 top;
     __be64 free_top;
     __be64 nkey;
     __be64 nfree_table;
} __attribute__((packed));

struct btree_dbsuper {
     char signature[8];
} __attribute__((packed));

struct btree_stats {
     uint64_t keys;
     uint64_t free_tables;
};

struct btree {
     uint64_t top;
     uint64_t free_top;
     uint64_t alloc;
     uint64_t db_alloc;
     int fd;
     int db_fd;
     int wal_fd;
     uint8_t lock;
     struct btree_stats stats;
     struct btree_cache cache[CACHE_SLOTS];
     struct btree_dbcache dbcache[DBCACHE_SLOTS];
};

/*
 * Open or Creat an existing database file.
 */
void btree_init(struct btree *btree, const char *file);

/*
 * Close a database file opened with btree_create() or btree_open().
 */
void btree_close(struct btree *btree);

/*
 * Delete the database from disk
 */
void btree_purge(const char *fname);

/*
 * Insert a new item with key 'quid' with the contents in 'data' to the
 * database file.
 */
int btree_insert(struct btree *btree, const quid_t *quid, const void *data, size_t len);

/*
 * Look up item with the given key 'quid' in the database file. Length of the
 * item is stored in 'len'. Returns a pointer to the contents of the item.
 * The returned pointer should be released with free() after use.
 */
void *btree_get(struct btree *btree, const quid_t *quid, size_t *len);

/*
 * Remove item with the given key 'quid' from the database file.
 */
int btree_delete(struct btree *btree, const quid_t *quid);

int btree_get_meta(struct btree *btree, const quid_t *quid, struct microdata *md);

int btree_meta(struct btree *btree, const quid_t *quid, const struct microdata *data);

int btree_remove(struct btree *btree, const quid_t *quid);

void walk_dbstorage(struct btree *btree);
int btree_vacuum(struct btree *btree, const char *fname);
int btree_update(struct btree *btree, const quid_t *quid, const void *data, size_t len);

#endif // ENGINE_H_INCLUDED
