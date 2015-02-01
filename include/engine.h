#ifndef ENGINE_H_INCLUDED
#define ENGINE_H_INCLUDED

#include <stdio.h>
#include <fcntl.h>

#include "bswap.h"

#define CACHE_SLOTS	23
#define TABLE_SIZE	((4096 - 1) / sizeof(struct btree_item))

struct btree_item {
     struct quid quid;
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

struct blob_info {
     __be32 len;
	uint8_t free;
} __attribute__((packed));

struct btree_super {
	char signature[8];
     __be64 top;
     __be64 free_top;
} __attribute__((packed));

struct btree_dbsuper {
     char signature[8];
} __attribute__((packed));

struct btree {
     uint64_t top;
     uint64_t free_top;
     uint64_t alloc;
     uint64_t db_alloc;
     int fd;
     int db_fd;
     int wal_fd;
     struct btree_cache cache[CACHE_SLOTS];
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
int btree_insert(struct btree *btree, const struct quid *quid, const void *data,
                  size_t len);

/*
 * Look up item with the given key 'quid' in the database file. Length of the
 * item is stored in 'len'. Returns a pointer to the contents of the item.
 * The returned pointer should be released with free() after use.
 */
void *btree_get(struct btree *btree, const struct quid *quid, size_t *len);

/*
 * Remove item with the given key 'quid' from the database file.
 */
int btree_delete(struct btree *btree, const struct quid *quid);

/*
 * Descent into the database index
 */
void btree_traversal(struct btree *btree, uint64_t offset);

#endif // ENGINE_H_INCLUDED
