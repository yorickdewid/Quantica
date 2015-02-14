#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "bswap.h"
#include "track.h"
#include "quid.h"
#include "engine.h"

#define DBEXT	".db"
#define IDXEXT	".idx"
#define LOGEXT	".log"
#define CDBEXT	"._db"
#define CIDXEXT	"._idx"
#define CLOGEXT	"._log"
#define BDBEXT	".db1"
#define BIDXEXT	".idx1"
#define BLOGEXT	".log1"

static int delete_larger = 0;
static struct etrace error = { .code = NO_ERROR};

static void flush_super(struct btree *btree);
static void flush_dbsuper(struct btree *btree);
static void free_chunk(struct btree *btree, uint64_t offset);
static void free_dbchunk(struct btree *btree, uint64_t offset);
static uint64_t remove_table(struct btree *btree, struct btree_table *table,
							size_t i, struct quid *quid);
static uint64_t delete_table(struct btree *btree, uint64_t table_offset,
                             struct quid *quid);
static uint64_t lookup(struct btree *btree, uint64_t table_offset,
                       const struct quid *quid);
uint64_t insert_toplevel(struct btree *btree, uint64_t *table_offset,
                         struct quid *quid, const void *data, size_t len);

static uint64_t collapse(struct btree *btree, uint64_t table_offset);

static int file_exists(const char *path)
{
	int fd = open(path, O_RDWR);
	if(fd>-1) {
		close(fd);
		return 1;
	}
	return 0;
}

static struct btree_table *alloc_table() {
	struct btree_table *table = malloc(sizeof *table);
	memset(table, 0, sizeof *table);
	return table;
}

static struct btree_table *get_table(struct btree *btree, uint64_t offset) {
	assert(offset != 0);

	/* take from cache */
	struct btree_cache *slot = &btree->cache[offset % CACHE_SLOTS];
	if (slot->offset == offset) {
		slot->offset = 0;
		return slot->table;
	}

	struct btree_table *table = malloc(sizeof *table);

	lseek(btree->fd, offset, SEEK_SET);
	if (read(btree->fd, table, sizeof *table) != (ssize_t) sizeof *table) {
		fprintf(stderr, "btree: I/O error\n");
		abort();
	}
	return table;
}

/* Free a table acquired with alloc_table() or get_table() */
static void put_table(struct btree *btree, struct btree_table *table,
                      uint64_t offset)
{
	assert(offset != 0);

	/* overwrite cache */
	struct btree_cache *slot = &btree->cache[offset % CACHE_SLOTS];
	if (slot->offset != 0) {
		free(slot->table);
	}
	slot->offset = offset;
	slot->table = table;
}

/* Write a table and free it */
static void flush_table(struct btree *btree, struct btree_table *table,
                        uint64_t offset)
{
	assert(offset != 0);

	lseek(btree->fd, offset, SEEK_SET);
	if (write(btree->fd, table, sizeof *table) != (ssize_t) sizeof *table) {
		fprintf(stderr, "btree: I/O error offset:%ld\n", offset);
		abort();
	}
	put_table(btree, table, offset);
}

static void create_backup(const char *fname)
{
	char ddbname[1024], didxname[1024], dwalname[1024];
	char sdbname[1024], sidxname[1024], swalname[1024];
	sprintf(sidxname, "%s%s", fname, IDXEXT);
	sprintf(sdbname, "%s%s", fname, DBEXT);
	sprintf(swalname, "%s%s", fname, LOGEXT);
	sprintf(didxname, "%s%s", fname, BIDXEXT);
	sprintf(ddbname, "%s%s", fname, BDBEXT);
	sprintf(dwalname, "%s%s", fname, BLOGEXT);
	rename(sidxname, didxname);
	rename(sdbname, ddbname);
	rename(swalname, dwalname);
}

static void restore_tmpdb(const char *fname)
{
	char ddbname[1024], didxname[1024], dwalname[1024];
	char sdbname[1024], sidxname[1024], swalname[1024];
	sprintf(sidxname, "%s%s", fname, CIDXEXT);
	sprintf(sdbname, "%s%s", fname, CDBEXT);
	sprintf(swalname, "%s%s", fname, CLOGEXT);
	sprintf(didxname, "%s%s", fname, IDXEXT);
	sprintf(ddbname, "%s%s", fname, DBEXT);
	sprintf(dwalname, "%s%s", fname, LOGEXT);
	rename(sidxname, didxname);
	rename(sdbname, ddbname);
	rename(swalname, dwalname);
}

static int btree_open(struct btree *btree, const char *idxname,
					const char *dbname, const char* walname)
{
	memset(btree, 0, sizeof *btree);
	btree->fd = open(idxname, O_RDWR | O_BINARY);
	btree->db_fd = open(dbname, O_RDWR | O_BINARY);
	btree->wal_fd = open(walname, O_RDWR | O_BINARY);
	if (btree->fd < 0)
		return -1;

	struct btree_super super;
	if (read(btree->fd, &super, sizeof super) != (ssize_t) sizeof super)
		return -1;
	btree->top = from_be64(super.top);
	btree->free_top = from_be64(super.free_top);
	assert(!strcmp(super.signature, IDXVERSION));

	struct btree_dbsuper dbsuper;
	if (read(btree->db_fd, &dbsuper, sizeof dbsuper) != (ssize_t) sizeof dbsuper)
		return -1;
	assert(!strcmp(dbsuper.signature, DBVERSION));

	btree->alloc = lseek(btree->fd, 0, SEEK_END);
	btree->db_alloc = lseek(btree->db_fd, 0, SEEK_END);
	return 0;
}

static int btree_create(struct btree *btree, const char *idxname,
						const char* dbname, const char* walname)
{
	memset(btree, 0, sizeof *btree);
	btree->fd = open(idxname, O_RDWR | O_TRUNC | O_CREAT | O_BINARY, 0644);
	btree->db_fd = open(dbname, O_RDWR | O_TRUNC | O_CREAT | O_BINARY, 0644);
	btree->wal_fd = open(walname, O_RDWR | O_TRUNC | O_CREAT | O_BINARY, 0644);
	if (btree->fd < 0)
		return -1;

	flush_super(btree);
	flush_dbsuper(btree);

	btree->alloc = sizeof(struct btree_super);
	btree->db_alloc = sizeof(struct btree_dbsuper);

	return 0;
}

void btree_init(struct btree *btree, const char *fname)
{
	char dbname[1024],idxname[1024],walname[1024];
	sprintf(idxname, "%s%s", fname, IDXEXT);
	sprintf(dbname, "%s%s", fname, DBEXT);
	sprintf(walname, "%s%s", fname, LOGEXT);

	restore_tmpdb(fname);
	if(file_exists(idxname) && file_exists(dbname)) {
		btree_open(btree, idxname, dbname, walname);
	} else {
		btree_create(btree, idxname, dbname, walname);
	}
}

void btree_close(struct btree *btree)
{
	flush_super(btree);
	close(btree->fd);
	close(btree->db_fd);
	close(btree->wal_fd);

	size_t i;
	for (i = 0; i < CACHE_SLOTS; ++i) {
		if (btree->cache[i].offset)
			free(btree->cache[i].table);
	}
}

void btree_purge(const char* fname)
{
	char dbname[1024],idxname[1024],walname[1024];
	sprintf(idxname, "%s%s", fname, IDXEXT);
	sprintf(dbname, "%s%s", fname, DBEXT);
	sprintf(walname, "%s%s", fname, LOGEXT);
	unlink(idxname);
	unlink(dbname);
	unlink(walname);
}

/* Return a value that is greater or equal to 'val' and is power-of-two. */
static size_t page_align(size_t val)
{
	size_t i = 1;
	while (i < val)
		i <<= 1;
	return i;
}

/* Allocate a chunk from the index file */
static uint64_t alloc_chunk(struct btree *btree, size_t len)
{
	assert(len > 0);

	/* Use blocks from freelist instead of allocation */
	if (btree->free_top){
		uint64_t offset = btree->free_top;
		struct btree_table *table = get_table(btree, offset);
		btree->free_top = from_be64(table->items[0].child);

		return offset;
	}

	len = page_align(len);
	uint64_t offset = btree->alloc;

	/* this is important to performance */
	if (offset & (len - 1)) {
		offset += len - (offset & (len - 1));
	}
	btree->alloc = offset + len;
	return offset;
}

/*Allocate a chunk from the database file*/
static uint64_t alloc_dbchunk(struct btree *btree, size_t len)
{
	assert(len > 0);

	if(!btree->dbcache[DBCACHE_SLOTS-1].len)
		goto new_block;

	int i;
	for(i=0; i<DBCACHE_SLOTS; ++i) {
		struct btree_dbcache *slot = &btree->dbcache[i];
		if (len <= slot->len) {
			int diff = (((double)(len)/(double)slot->len)*100);
			if (diff >= DBCACHE_DENSITY) {
				slot->len = 0;
				return slot->offset;
			}
		}
	}

new_block:
	len = page_align(sizeof(struct blob_info) + len);

	uint64_t offset = btree->db_alloc;
	btree->db_alloc = offset + len;
	return offset;
}

/* Mark a chunk as unused in the database file */
static void free_chunk(struct btree *btree, uint64_t offset)
{
	assert(offset > 0);
	struct btree_table *table = get_table(btree, offset);

	struct quid quid;
	memset(&quid, 0, sizeof(struct quid));

	memcpy(&table->items[0].quid, &quid, sizeof(struct quid));
	table->size++;
	table->items[0].offset = 0;
	table->items[0].child = to_be64(btree->free_top);

	flush_table(btree, table, offset);
	btree->free_top = offset;
}

static void free_dbchunk(struct btree *btree, uint64_t offset)
{
	lseek(btree->db_fd, offset, SEEK_SET);
	struct blob_info info;
	if (read(btree->db_fd, &info, sizeof(struct blob_info)) != sizeof(struct blob_info)) {
		fprintf(stderr, "btree: I/O error\n");
		abort();
	}

	int i, j;
	info.free = 1;
	struct btree_dbcache dbinfo;
	size_t len = from_be32(info.len);
	dbinfo.offset = offset;
	for(i=DBCACHE_SLOTS-1; i>=0; --i) {
		struct btree_dbcache *slot = &btree->dbcache[i];
		if (len > slot->len) {
			if (slot->len) {
				for(j=0; j<i; ++j) {
					btree->dbcache[j] = btree->dbcache[j+1];
				}
			}
			dbinfo.len = len;
			btree->dbcache[i] = dbinfo;
			break;
		}
	}

	lseek(btree->db_fd, offset, SEEK_SET);
	if (write(btree->db_fd, &info, sizeof(struct blob_info)) != sizeof(struct blob_info)) {
		fprintf(stderr, "btree: I/O error\n");
		abort();
	}
}

static void flush_super(struct btree *btree)
{
	struct btree_super super;
	memset(&super, 0, sizeof super);
	strcpy(super.signature, IDXVERSION);
	super.top = to_be64(btree->top);
	super.free_top = to_be64(btree->free_top);

	lseek(btree->fd, 0, SEEK_SET);
	if (write(btree->fd, &super, sizeof super) != sizeof super) {
		fprintf(stderr, "btree: I/O error\n");
		abort();
	}
}

static void flush_dbsuper(struct btree *btree)
{
	struct btree_dbsuper dbsuper;
	memset(&dbsuper, 0, sizeof(struct btree_dbsuper));
	strcpy(dbsuper.signature, DBVERSION);

	lseek(btree->db_fd, 0, SEEK_SET);
	if (write(btree->db_fd, &dbsuper, sizeof(struct btree_dbsuper)) != sizeof(struct btree_dbsuper)) {
		fprintf(stderr, "btree: I/O error\n");
		abort();
	}
}

static uint64_t insert_data(struct btree *btree, const void *data, size_t len)
{
	if (data == NULL || len == 0) {
		error.code = VAL_EMPTY;
		return len;
	}

	struct blob_info info;
	memset(&info, 0, sizeof info);
	info.len = to_be32(len);
	info.free = 0;

	uint64_t offset = alloc_dbchunk(btree, len);

	lseek(btree->db_fd, offset, SEEK_SET);
	if (write(btree->db_fd, &info, sizeof info) != sizeof info) {
		fprintf(stderr, "btree: I/O error\n");
		abort();
	}
	if (write(btree->db_fd, data, len) != (ssize_t) len) {
		fprintf(stderr, "btree: I/O error\n");
		abort();
	}

	return offset;
}

/* Split a table. The pivot item is stored to 'quid' and 'offset'.
   Returns offset to the new table. */
static uint64_t split_table(struct btree *btree, struct btree_table *table,
                            struct quid *quid, uint64_t *offset)
{
	memcpy(quid, &table->items[TABLE_SIZE / 2].quid, sizeof(struct quid));
	*offset = from_be64(table->items[TABLE_SIZE / 2].offset);

	struct btree_table *new_table = alloc_table();
	new_table->size = table->size - TABLE_SIZE / 2 - 1;

	table->size = TABLE_SIZE / 2;

	memcpy(new_table->items, &table->items[TABLE_SIZE / 2 + 1],
	       (new_table->size + 1) * sizeof(struct btree_item));

	uint64_t new_table_offset = alloc_chunk(btree, sizeof *new_table);
	flush_table(btree, new_table, new_table_offset);

	return new_table_offset;
}

/* Try to collapse the given table. Returns a new table offset. */
static uint64_t collapse(struct btree *btree, uint64_t table_offset)
{
	struct btree_table *table = get_table(btree, table_offset);
	if (table->size == 0) {
		uint64_t ret = from_be64(table->items[0].child);
		free_chunk(btree, table_offset);
		return ret;
	}
	put_table(btree, table, table_offset);
	return table_offset;
}

/* Find and remove the smallest item from the given table. The key of the item
   is stored to 'quid'. Returns offset to the item */
static uint64_t take_smallest(struct btree *btree, uint64_t table_offset,
                              struct quid *quid)
{
	struct btree_table *table = get_table(btree, table_offset);
	assert(table->size > 0);

	uint64_t offset = 0;
	uint64_t child = from_be64(table->items[0].child);
	if (child == 0) {
		offset = remove_table(btree, table, 0, quid);
	} else {
		/* recursion */
		offset = take_smallest(btree, child, quid);
		table->items[0].child = to_be64(collapse(btree, child));
	}
	flush_table(btree, table, table_offset);
	return offset;
}

/* Find and remove the largest item from the given table. The key of the item
   is stored to 'quid'. Returns offset to the item */
static uint64_t take_largest(struct btree *btree, uint64_t table_offset,
                             struct quid *quid)
{
	struct btree_table *table = get_table(btree, table_offset);
	assert(table->size > 0);

	uint64_t offset = 0;
	uint64_t child = from_be64(table->items[table->size].child);
	if (child == 0) {
		offset = remove_table(btree, table, table->size - 1, quid);
	} else {
		/* recursion */
		offset = take_largest(btree, child, quid);
		table->items[table->size].child = to_be64(collapse(btree, child));
	}
	flush_table(btree, table, table_offset);
	return offset;
}

/* Remove an item in position 'i' from the given table. The key of the
   removed item is stored to 'quid'. Returns offset to the item. */
static uint64_t remove_table(struct btree *btree, struct btree_table *table,
                             size_t i, struct quid *quid)
{
	assert(i < table->size);

	if (quid)
		memcpy(quid, &table->items[i].quid, sizeof(struct quid));

	uint64_t offset = from_be64(table->items[i].offset);
	uint64_t left_child = from_be64(table->items[i].child);
	uint64_t right_child = from_be64(table->items[i + 1].child);

	if (left_child != 0 && right_child != 0) {
		/* replace the removed item by taking an item from one of the
		   child tables */
		uint64_t new_offset;
		if (rand() & 1) {
			new_offset = take_largest(btree, left_child,
			                          &table->items[i].quid);
			table->items[i].child =
			    to_be64(collapse(btree, left_child));
		} else {
			new_offset = take_smallest(btree, right_child,
			                           &table->items[i].quid);
			table->items[i + 1].child =
			    to_be64(collapse(btree, right_child));
		}
		table->items[i].offset = to_be64(new_offset);

	} else {
		memmove(&table->items[i], &table->items[i + 1],
		        (table->size - i) * sizeof(struct btree_item));
		table->size--;

		if (left_child != 0) {
			table->items[i].child = to_be64(left_child);
		} else {
			table->items[i].child = to_be64(right_child);
		}
	}
	return offset;
}

/* Insert a new item with key 'quid' with the contents in 'data' to the given
   table. Returns offset to the new item. */
static uint64_t insert_table(struct btree *btree, uint64_t table_offset,
                             struct quid *quid, const void *data, size_t len)
{
	struct btree_table *table = get_table(btree, table_offset);
	assert(table->size < TABLE_SIZE-1);

	size_t left = 0, right = table->size;
	while (left < right) {
		size_t i = (right - left) / 2 + left;
		int cmp = quidcmp(quid, &table->items[i].quid);
		if (cmp == 0) {
			/* already in the table */
			uint64_t ret = from_be64(table->items[i].offset);
			put_table(btree, table, table_offset);
			error.code = QUID_EXIST;
			return ret;
		}
		if (cmp < 0)
			right = i;
		else
			left = i + 1;
	}
	size_t i = left;

	uint64_t offset = 0;
	uint64_t left_child = from_be64(table->items[i].child);
	uint64_t right_child = 0; /* after insertion */
	uint64_t ret = 0;
	if (left_child != 0) {
		/* recursion */
		ret = insert_table(btree, left_child, quid, data, len);

		/* check if we need to split */
		struct btree_table *child = get_table(btree, left_child);
		if (child->size < TABLE_SIZE-1) {
			/* nothing to do */
			put_table(btree, table, table_offset);
			put_table(btree, child, left_child);
			return ret;
		}
		/* overwrites SHA-1 */
		right_child = split_table(btree, child, quid, &offset);
		/* flush just in case changes happened */
		flush_table(btree, child, left_child);
	} else {
		ret = offset = insert_data(btree, data, len);
	}

	table->size++;
	memmove(&table->items[i + 1], &table->items[i],
	        (table->size - i) * sizeof(struct btree_item));
	memcpy(&table->items[i].quid, quid, sizeof(struct quid));
	table->items[i].offset = to_be64(offset);
	table->items[i].child = to_be64(left_child);
	table->items[i + 1].child = to_be64(right_child);

	flush_table(btree, table, table_offset);
	return ret;
}

/*
 * Remove a item with key 'quid' from the given table. The offset to the
 * removed item is returned.
 * Please note that 'quid' is overwritten when called inside the allocator.
 */
static uint64_t delete_table(struct btree *btree, uint64_t table_offset,
                             struct quid *quid)
{
	if (table_offset == 0) {
		error.code = DB_EMPTY; //TODO this may not be always the case
		return 0;
	}
	struct btree_table *table = get_table(btree, table_offset);

	size_t left = 0, right = table->size;
	while (left < right) {
		size_t i = (right - left) / 2 + left;
		int cmp = quidcmp(quid, &table->items[i].quid);
		if (cmp == 0) {
			/* found */
			uint64_t ret = remove_table(btree, table, i, quid);
			flush_table(btree, table, table_offset);
			return ret;
		}
		if (cmp < 0) {
			right = i;
		} else {
			left = i + 1;
		}
	}

	/* not found - recursion */
	size_t i = left;
	uint64_t child = from_be64(table->items[i].child);
	uint64_t ret = delete_table(btree, child, quid);
	if (ret != 0) {
		table->items[i].child = to_be64(collapse(btree, child));
	}

	if (ret == 0 && delete_larger && i < table->size) {
		/* remove the next largest */
		ret = remove_table(btree, table, i, quid);
	}
	if (ret != 0) {
		/* flush just in case changes happened */
		flush_table(btree, table, table_offset);
	} else {
		put_table(btree, table, table_offset);
	}
	return ret;
}

uint64_t insert_toplevel(struct btree *btree, uint64_t *table_offset,
                         struct quid *quid, const void *data, size_t len)
{
	uint64_t offset = 0;
	uint64_t ret = 0;
	uint64_t right_child = 0;
	if (*table_offset != 0) {
		ret = insert_table(btree, *table_offset, quid, data, len);

		/* check if we need to split */
		struct btree_table *table = get_table(btree, *table_offset);
		if (table->size < TABLE_SIZE-1) {
			/* nothing to do */
			put_table(btree, table, *table_offset);
			return ret;
		}
		right_child = split_table(btree, table, quid, &offset);
		flush_table(btree, table, *table_offset);
	} else {
		ret = offset = insert_data(btree, data, len);
	}

	/* create new top level table */
	struct btree_table *new_table = alloc_table();
	new_table->size = 1;
	memcpy(&new_table->items[0].quid, quid, sizeof(struct quid));
	new_table->items[0].offset = to_be64(offset);
	new_table->items[0].child = to_be64(*table_offset);
	new_table->items[1].child = to_be64(right_child);

	uint64_t new_table_offset = alloc_chunk(btree, sizeof *new_table);
	flush_table(btree, new_table, new_table_offset);

	*table_offset = new_table_offset;
	return ret;
}

int btree_insert(struct btree *btree, const struct quid *c_quid, const void *data,
                  size_t len)
{
	error.code = NO_ERROR;
	if (btree->lock == LOCK)
		return -1;
	/* SHA-1 must be in writable memory */
	struct quid quid;
	memcpy(&quid, c_quid, sizeof(struct quid));

	insert_toplevel(btree, &btree->top, &quid, data, len);
	flush_super(btree);
	if (error.code == QUID_EXIST || error.code == VAL_EMPTY)
		return -1;

	return 0;
}

/*
 * Look up item with the given key 'quid' in the given table. Returns offset
 * to the item.
 */
static uint64_t lookup(struct btree *btree, uint64_t table_offset,
                       const struct quid *quid)
{
	while (table_offset) {
		struct btree_table *table = get_table(btree, table_offset);
		size_t left = 0, right = table->size, i;
		while (left < right) {
			i = (right - left) / 2 + left;
			int cmp = quidcmp(quid, &table->items[i].quid);
			if (cmp == 0) {
				/* found */
				uint64_t ret = from_be64(table->items[i].offset);
				put_table(btree, table, table_offset);
				return ret;
			}
			if (cmp < 0) {
				right = i;
			} else {
				left = i + 1;
			}
		}
		uint64_t child = from_be64(table->items[left].child);
		put_table(btree, table, table_offset);
		table_offset = child;
	}
	error.code = QUID_NOTFOUND;
	return 0;
}

void *btree_get(struct btree *btree, const struct quid *quid, size_t *len)
{
	error.code = NO_ERROR;
	if (btree->lock == LOCK)
		return NULL;
	uint64_t offset = lookup(btree, btree->top, quid);
	if (error.code == QUID_NOTFOUND)
		return NULL;

	lseek(btree->db_fd, offset, SEEK_SET);
	struct blob_info info;
	if (read(btree->db_fd, &info, sizeof info) != (ssize_t) sizeof info)
		return NULL;
	*len = from_be32(info.len);
	assert(*len > 0);

	void *data = malloc(*len);
	if (data == NULL)
		return NULL;
	if (read(btree->db_fd, data, *len) != (ssize_t) *len) {
		free(data);
		data = NULL;
	}
	return data;
}

int btree_delete(struct btree *btree, const struct quid *c_quid)
{
	struct quid quid;
	if (btree->lock == LOCK)
		return -1;
	memcpy(&quid, c_quid, sizeof(struct quid));

	uint64_t offset = delete_table(btree, btree->top, &quid);
	btree->top = collapse(btree, btree->top);

	free_dbchunk(btree, offset);
	flush_super(btree);
	return 0;
}

void walk_dbstorage(struct btree *btree)
{
	uint64_t offset = sizeof(struct btree_dbsuper);
	struct blob_info info;

	while(1){
		lseek(btree->db_fd, offset, SEEK_SET);
		if (read(btree->db_fd, &info, sizeof(struct blob_info)) != (ssize_t) sizeof(struct blob_info))
			return;

		offset = offset+page_align(sizeof(struct blob_info)+from_be32(info.len));
	}
}

static void tree_traversal(struct btree *btree, struct btree *cbtree, uint64_t offset)
{
	int i;
	struct btree_table *table = get_table(btree, offset);
	size_t sz = table->size;
	for(i=0; i<(int)sz; ++i) {
		uint64_t child = from_be64(table->items[i].child);
		uint64_t right = from_be64(table->items[i+1].child);
		uint64_t dboffset = from_be64(table->items[i].offset);

		lseek(btree->db_fd, dboffset, SEEK_SET);
		struct blob_info info;
		if (read(btree->db_fd, &info, sizeof(struct blob_info)) != (ssize_t) sizeof(struct blob_info))
			abort();
		size_t len = from_be32(info.len);
		void *data = malloc(len);
		if (data == NULL)
			abort();
		if (read(btree->db_fd, data, len) != (ssize_t) len) {
			free(data);
			data = NULL;
		}
		insert_toplevel(cbtree, &cbtree->top, &table->items[i].quid, data, len);
		free(data);

		if (child) tree_traversal(btree, cbtree, child);
		if (right) tree_traversal(btree, cbtree, right);
	}
}

int btree_vacuum(struct btree *btree, const char *fname)
{
	struct btree cbtree;
	struct btree tmp;
	if (btree->lock == LOCK)
		return -1;
	btree->lock = LOCK;
	char dbname[1024], idxname[1024], walname[1024];
	sprintf(idxname, "%s%s", fname, CIDXEXT);
	sprintf(dbname, "%s%s", fname, CDBEXT);
	sprintf(walname, "%s%s", fname, CLOGEXT);
	btree_create(&cbtree, idxname, dbname, walname);
	tree_traversal(btree, &cbtree, btree->top);

	memcpy(&tmp, btree, sizeof(struct btree));
	memcpy(btree, &cbtree, sizeof(struct btree));
	btree_close(&tmp);

	create_backup(fname);

	return 0;
}
