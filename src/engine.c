#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <config.h>
#include <common.h>
#include <log.h>
#include <error.h>
#include "quid.h"
#include "engine.h"
#include "core.h"

static int delete_larger = 0;

static void flush_super(struct engine *e);
static void flush_dbsuper(struct engine *e);
static void free_index_chunk(struct engine *e, uint64_t offset);
static void free_dbchunk(struct engine *e, uint64_t offset);
static uint64_t remove_table(struct engine *e, struct engine_table *table, size_t i, quid_t *quid);
static uint64_t delete_table(struct engine *e, uint64_t table_offset, quid_t *quid);
static uint64_t lookup_key(struct engine *e, uint64_t table_offset, const quid_t *quid);
static uint64_t insert_toplevel(struct engine *e, uint64_t *table_offset, quid_t *quid, const void *data, size_t len);
static uint64_t table_join(struct engine *e, uint64_t table_offset);

static struct engine_table *alloc_table() {
	struct engine_table *table = malloc(sizeof *table);
	if (!table) {
		lprintf("[erro] Failed to request memory\n");
		ERROR(EM_ALLOC, EL_FATAL);
		return NULL;
	}
	memset(table, 0, sizeof *table);
	return table;
}

static struct engine_table *get_table(struct engine *e, uint64_t offset) {
	assert(offset != 0);

	/* take from cache */
	struct engine_cache *slot = &e->cache[offset % CACHE_SLOTS];
	if (slot->offset == offset) {
		slot->offset = 0;
		return slot->table;
	}

	struct engine_table *table = malloc(sizeof *table);
	if (!table) {
		lprintf("[erro] Failed to request memory\n");
		ERROR(EM_ALLOC, EL_FATAL);
		return NULL;
	}

	lseek(e->fd, offset, SEEK_SET);
	if (read(e->fd, table, sizeof(struct engine_table)) != sizeof(struct engine_table)) {
		lprintf("[erro] Failed to read disk\n");
		ERROR(EIO_READ, EL_FATAL);
		return NULL;
	}
	return table;
}

/* Free a table acquired with alloc_table() or get_table() */
static void put_table(struct engine *e, struct engine_table *table, uint64_t offset) {
	assert(offset != 0);

	/* overwrite cache */
	struct engine_cache *slot = &e->cache[offset % CACHE_SLOTS];
	if (slot->offset != 0) {
		free(slot->table);
	}
	slot->offset = offset;
	slot->table = table;
}

/* Write a table and free it */
static void flush_table(struct engine *e, struct engine_table *table, uint64_t offset) {
	assert(offset != 0);

	lseek(e->fd, offset, SEEK_SET);
	if (write(e->fd, table, sizeof(struct engine_table)) != sizeof(struct engine_table)) {
		lprintf("[erro] Failed to write disk\n");
		ERROR(EIO_WRITE, EL_FATAL);
		return;
	}
	put_table(e, table, offset);
}

static void create_backup(const char *fname) {
	char ddbname[DBNAME_SIZE], didxname[DBNAME_SIZE], dwalname[DBNAME_SIZE];
	char sdbname[DBNAME_SIZE], sidxname[DBNAME_SIZE], swalname[DBNAME_SIZE];
	snprintf(sidxname, DBNAME_SIZE, "%s.%s", fname, IDXEXT);
	snprintf(sdbname, DBNAME_SIZE, "%s.%s", fname, DBEXT);
	snprintf(swalname, DBNAME_SIZE, "%s.%s", fname, LOGEXT);
	snprintf(didxname, DBNAME_SIZE, "%s.%s1", fname, IDXEXT);
	snprintf(ddbname, DBNAME_SIZE, "%s.%s1", fname, DBEXT);
	snprintf(dwalname, DBNAME_SIZE, "%s.%s1", fname, LOGEXT);
	rename(sidxname, didxname);
	rename(sdbname, ddbname);
	rename(swalname, dwalname);
}

static void restore_tmpdb(const char *fname) {
	char ddbname[DBNAME_SIZE], didxname[DBNAME_SIZE], dwalname[DBNAME_SIZE];
	char sdbname[DBNAME_SIZE], sidxname[DBNAME_SIZE], swalname[DBNAME_SIZE];
	snprintf(sidxname, DBNAME_SIZE, "%s._%s", fname, IDXEXT);
	snprintf(sdbname, DBNAME_SIZE, "%s._%s", fname, DBEXT);
	snprintf(swalname, DBNAME_SIZE, "%s._%s", fname, LOGEXT);
	snprintf(didxname, DBNAME_SIZE, "%s.%s", fname, IDXEXT);
	snprintf(ddbname, DBNAME_SIZE, "%s.%s", fname, DBEXT);
	snprintf(dwalname, DBNAME_SIZE, "%s.%s", fname, LOGEXT);
	rename(sidxname, didxname);
	rename(sdbname, ddbname);
	rename(swalname, dwalname);
}

static int engine_open(struct engine *e, const char *idxname, const char *dbname, const char* walname) {
	memset(e, 0, sizeof(struct engine));
	e->fd = open(idxname, O_RDWR | O_BINARY);
	e->db_fd = open(dbname, O_RDWR | O_BINARY);
	e->wal_fd = open(walname, O_RDWR | O_BINARY);
	if (e->fd < 0)
		return -1;

	struct engine_super super;
	if (read(e->fd, &super, sizeof(struct engine_super)) != sizeof(struct engine_super)) {
		lprintf("[erro] Failed to read disk\n");
		ERROR(EIO_READ, EL_FATAL);
		return -1;
	}
	e->top = from_be64(super.top);
	e->free_top = from_be64(super.free_top);
	e->stats.keys = from_be64(super.nkey);
	e->stats.free_tables = from_be64(super.nfree_table);
	assert(from_be64(super.version)==VERSION_RELESE);
	strlcpy(e->ins_name, super.instance, INSTANCE_LENGTH);

	struct engine_dbsuper dbsuper;
	if (read(e->db_fd, &dbsuper, sizeof(struct engine_dbsuper)) != sizeof(struct engine_dbsuper)) {
		lprintf("[erro] Failed to read disk\n");
		ERROR(EIO_READ, EL_FATAL);
		return -1;
	}
	assert(from_be64(dbsuper.version)==VERSION_RELESE);

	e->alloc = lseek(e->fd, 0, SEEK_END);
	e->db_alloc = lseek(e->db_fd, 0, SEEK_END);
	return 0;
}

static int engine_create(struct engine *e, const char *idxname, const char *dbname, const char *walname) {
	memset(e, 0, sizeof(struct engine));
	e->fd = open(idxname, O_RDWR | O_TRUNC | O_CREAT | O_BINARY, 0644);
	e->db_fd = open(dbname, O_RDWR | O_TRUNC | O_CREAT | O_BINARY, 0644);
	e->wal_fd = open(walname, O_RDWR | O_TRUNC | O_CREAT | O_BINARY, 0644);
	if (e->fd < 0)
		return -1;

	flush_super(e);
	flush_dbsuper(e);

	e->alloc = sizeof(struct engine_super);
	e->db_alloc = sizeof(struct engine_dbsuper);
	strlcpy(e->ins_name, INSTANCE, INSTANCE_LENGTH);
	return 0;
}

void engine_init(struct engine *e, const char *fname) {
	char dbname[DBNAME_SIZE], idxname[DBNAME_SIZE], walname[DBNAME_SIZE];
	snprintf(idxname, DBNAME_SIZE, "%s.%s", fname, IDXEXT);
	snprintf(dbname, DBNAME_SIZE, "%s.%s", fname, DBEXT);
	snprintf(walname, DBNAME_SIZE, "%s.%s", fname, LOGEXT);

	restore_tmpdb(fname);
	if(file_exists(idxname) && file_exists(dbname)) {
		engine_open(e, idxname, dbname, walname);
	} else {
		engine_create(e, idxname, dbname, walname);
	}
}

void engine_close(struct engine *e) {
	flush_super(e);
	close(e->fd);
	close(e->db_fd);
	close(e->wal_fd);

	size_t i;
	for(i=0; i<CACHE_SLOTS; ++i) {
		if(e->cache[i].offset) {
			free(e->cache[i].table);
		}
	}
}

void engine_unlink(const char *fname) {
	char dbname[DBNAME_SIZE], idxname[DBNAME_SIZE], walname[DBNAME_SIZE];
	snprintf(idxname, DBNAME_SIZE, "%s.%s", fname, IDXEXT);
	snprintf(dbname, DBNAME_SIZE, "%s.%s", fname, DBEXT);
	snprintf(walname, DBNAME_SIZE, "%s.%s", fname, LOGEXT);
	unlink(idxname);
	unlink(dbname);
	unlink(walname);
}

void engine_flush(struct engine *e) {
	flush_super(e);
	flush_dbsuper(e);
}

/* Return a value that is greater or equal to 'val' and is power-of-two. */
static size_t page_align(size_t val) {
	size_t i = 1;
	while (i < val)
		i <<= 1;
	return i;
}

/* Allocate a chunk from the index file */
static uint64_t alloc_index_chunk(struct engine *e, size_t len) {
	assert(len > 0);

	/* Use blocks from freelist instead of allocation */
	if (e->free_top){
		uint64_t offset = e->free_top;
		struct engine_table *table = get_table(e, offset);
		e->free_top = from_be64(table->items[0].child);
		e->stats.free_tables--;

		return offset;
	}

	len = page_align(len);
	uint64_t offset = e->alloc;

	/* this is important to performance */
	if (offset & (len - 1)) {
		offset += len - (offset & (len - 1));
	}
	e->alloc = offset + len;
	return offset;
}

/* Allocate a chunk from the database file */
static uint64_t alloc_dbchunk(struct engine *e, size_t len) {
	assert(len > 0);

	if(!e->dbcache[DBCACHE_SLOTS-1].len)
		goto new_block;

	int i;
	for(i=0; i<DBCACHE_SLOTS; ++i) {
		struct engine_dbcache *slot = &e->dbcache[i];
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

	uint64_t offset = e->db_alloc;
	e->db_alloc = offset + len;
	return offset;
}

/* Mark a chunk as unused in the database file */
static void free_index_chunk(struct engine *e, uint64_t offset) {
	assert(offset > 0);
	struct engine_table *table = get_table(e, offset);

	quid_t quid;
	memset(&quid, 0, sizeof(quid_t));

	memcpy(&table->items[0].quid, &quid, sizeof(quid_t));
	table->size++;
	table->items[0].offset = 0;
	table->items[0].child = to_be64(e->free_top);

	flush_table(e, table, offset);
	e->free_top = offset;
	e->stats.free_tables++;
}

static void free_dbchunk(struct engine *e, uint64_t offset) {
	lseek(e->db_fd, offset, SEEK_SET);
	struct blob_info info;
	if (read(e->db_fd, &info, sizeof(struct blob_info)) != sizeof(struct blob_info)) {
		lprintf("[erro] Failed to read disk\n");
		ERROR(EIO_READ, EL_FATAL);
		return;
	}

	int i, j;
	info.free = 1;
	struct engine_dbcache dbinfo;
	size_t len = from_be32(info.len);
	dbinfo.offset = offset;
	for(i=DBCACHE_SLOTS-1; i>=0; --i) {
		struct engine_dbcache *slot = &e->dbcache[i];
		if (len > slot->len) {
			if (slot->len) {
				for(j=0; j<i; ++j) {
					e->dbcache[j] = e->dbcache[j+1];
				}
			}
			dbinfo.len = len;
			e->dbcache[i] = dbinfo;
			break;
		}
	}

	lseek(e->db_fd, offset, SEEK_SET);
	if (write(e->db_fd, &info, sizeof(struct blob_info)) != sizeof(struct blob_info)) {
		lprintf("[erro] Failed to write disk\n");
		ERROR(EIO_WRITE, EL_FATAL);
		return;
	}
}

static void flush_super(struct engine *e) {
	struct engine_super super;
	memset(&super, 0, sizeof super);
	super.version = to_be64(VERSION_RELESE);
	super.top = to_be64(e->top);
	super.free_top = to_be64(e->free_top);
	super.nkey = to_be64(e->stats.keys);
	super.nfree_table = to_be64(e->stats.free_tables);
	strlcpy(super.instance, e->ins_name, INSTANCE_LENGTH);

	lseek(e->fd, 0, SEEK_SET);
	if (write(e->fd, &super, sizeof super) != sizeof super) {
		lprintf("[erro] Failed to write disk\n");
		ERROR(EIO_WRITE, EL_FATAL);
		return;
	}
}

static void flush_dbsuper(struct engine *e) {
	struct engine_dbsuper dbsuper;
	memset(&dbsuper, 0, sizeof(struct engine_dbsuper));
	dbsuper.version = to_be64(VERSION_RELESE);

	lseek(e->db_fd, 0, SEEK_SET);
	if (write(e->db_fd, &dbsuper, sizeof(struct engine_dbsuper)) != sizeof(struct engine_dbsuper)) {
		lprintf("[erro] Failed to write disk\n");
		ERROR(EIO_WRITE, EL_FATAL);
		return;
	}
}

static uint64_t insert_data(struct engine *e, const void *data, size_t len) {
	if (data == NULL || len == 0) {
		ERROR(ENO_DATA, EL_WARN);
		return len;
	}

	struct blob_info info;
	memset(&info, 0, sizeof info);
	info.len = to_be32(len);
	info.free = 0;

	uint64_t offset = alloc_dbchunk(e, len);

	lseek(e->db_fd, offset, SEEK_SET);
	if (write(e->db_fd, &info, sizeof(struct blob_info)) != sizeof(struct blob_info)) {
		lprintf("[erro] Failed to write disk\n");
		ERROR(EIO_WRITE, EL_FATAL);
		return 0;
	}
	if (write(e->db_fd, data, len) != (ssize_t)len) {
		lprintf("[erro] Failed to write disk\n");
		ERROR(EIO_WRITE, EL_FATAL);
		return 0;
	}

	return offset;
}

/* Split a table. The pivot item is stored to 'quid' and 'offset'.
   Returns offset to the new table. */
static uint64_t split_table(struct engine *e, struct engine_table *table, quid_t *quid, uint64_t *offset) {
	memcpy(quid, &table->items[TABLE_SIZE / 2].quid, sizeof(quid_t));
	*offset = from_be64(table->items[TABLE_SIZE / 2].offset);

	struct engine_table *new_table = alloc_table();
	new_table->size = table->size - TABLE_SIZE / 2 - 1;

	table->size = TABLE_SIZE / 2;
	memcpy(new_table->items, &table->items[TABLE_SIZE / 2 + 1], (new_table->size + 1) * sizeof(struct engine_item));

	uint64_t new_table_offset = alloc_index_chunk(e, sizeof *new_table);
	flush_table(e, new_table, new_table_offset);

	return new_table_offset;
}

/* Try to table_rejoin the given table. Returns a new table offset. */
static uint64_t table_join(struct engine *e, uint64_t table_offset) {
	struct engine_table *table = get_table(e, table_offset);
	if (table->size == 0) {
		uint64_t ret = from_be64(table->items[0].child);
		free_index_chunk(e, table_offset);
		return ret;
	}
	put_table(e, table, table_offset);
	return table_offset;
}

/* Find and remove the smallest item from the given table. The key of the item
   is stored to 'quid'. Returns offset to the item */
static uint64_t take_smallest(struct engine *e, uint64_t table_offset, quid_t *quid) {
	struct engine_table *table = get_table(e, table_offset);
	assert(table->size > 0);

	uint64_t offset = 0;
	uint64_t child = from_be64(table->items[0].child);
	if (child == 0) {
		offset = remove_table(e, table, 0, quid);
	} else {
		/* recursion */
		offset = take_smallest(e, child, quid);
		table->items[0].child = to_be64(table_join(e, child));
	}
	flush_table(e, table, table_offset);
	return offset;
}

/* Find and remove the largest item from the given table. The key of the item
   is stored to 'quid'. Returns offset to the item */
static uint64_t take_largest(struct engine *e, uint64_t table_offset, quid_t *quid) {
	struct engine_table *table = get_table(e, table_offset);
	assert(table->size > 0);

	uint64_t offset = 0;
	uint64_t child = from_be64(table->items[table->size].child);
	if (child == 0) {
		offset = remove_table(e, table, table->size - 1, quid);
	} else {
		/* recursion */
		offset = take_largest(e, child, quid);
		table->items[table->size].child = to_be64(table_join(e, child));
	}
	flush_table(e, table, table_offset);
	return offset;
}

/* Remove an item in position 'i' from the given table. The key of the
   removed item is stored to 'quid'. Returns offset to the item. */
static uint64_t remove_table(struct engine *e, struct engine_table *table, size_t i, quid_t *quid) {
	assert(i < table->size);

	if (quid)
		memcpy(quid, &table->items[i].quid, sizeof(quid_t));

	uint64_t offset = from_be64(table->items[i].offset);
	uint64_t left_child = from_be64(table->items[i].child);
	uint64_t right_child = from_be64(table->items[i + 1].child);

	if (left_child != 0 && right_child != 0) {
		/* replace the removed item by taking an item from one of the
		   child tables */
		uint64_t new_offset;
		if (RANDOM() & 1) {
			new_offset = take_largest(e, left_child, &table->items[i].quid);
			table->items[i].child = to_be64(table_join(e, left_child));
		} else {
			new_offset = take_smallest(e, right_child, &table->items[i].quid);
			table->items[i+1].child = to_be64(table_join(e, right_child));
		}
		table->items[i].offset = to_be64(new_offset);
	} else {
		memmove(&table->items[i], &table->items[i+1], (table->size - i) * sizeof(struct engine_item));
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
static uint64_t insert_table(struct engine *e, uint64_t table_offset, quid_t *quid, const void *data, size_t len) {
	struct engine_table *table = get_table(e, table_offset);
	assert(table->size < TABLE_SIZE-1);

	size_t left = 0, right = table->size;
	while (left < right) {
		size_t i = (right - left) / 2 + left;
		int cmp = quidcmp(quid, &table->items[i].quid);
		if (cmp == 0) {
			/* already in the table */
			uint64_t ret = from_be64(table->items[i].offset);
			put_table(e, table, table_offset);
			ERROR(EQUID_EXIST, EL_WARN);
			return ret;
		}
		if (cmp < 0) {
			right = i;
		} else {
			left = i + 1;
		}
	}

	size_t i = left;
	uint64_t offset = 0;
	uint64_t left_child = from_be64(table->items[i].child);
	uint64_t right_child = 0; /* after insertion */
	uint64_t ret = 0;
	if (left_child != 0) {
		/* recursion */
		ret = insert_table(e, left_child, quid, data, len);

		/* check if we need to split */
		struct engine_table *child = get_table(e, left_child);
		if (child->size < TABLE_SIZE-1) {
			/* nothing to do */
			put_table(e, table, table_offset);
			put_table(e, child, left_child);
			return ret;
		}
		/* overwrites SHA-1 */
		right_child = split_table(e, child, quid, &offset);
		/* flush just in case changes happened */
		flush_table(e, child, left_child);
	} else {
		ret = offset = insert_data(e, data, len);
	}

	table->size++;
	memmove(&table->items[i+1], &table->items[i], (table->size-i) * sizeof(struct engine_item));
	memcpy(&table->items[i].quid, quid, sizeof(quid_t));
	table->items[i].offset = to_be64(offset);
	memset(&table->items[i].meta, 0, sizeof(struct metadata));
	table->items[i].meta.importance = MD_IMPORTANT_NORMAL;
	table->items[i].child = to_be64(left_child);
	table->items[i+1].child = to_be64(right_child);

	flush_table(e, table, table_offset);
	return ret;
}

/*
 * Remove a item with key 'quid' from the given table. The offset to the
 * removed item is returned.
 * Please note that 'quid' is overwritten when called inside the allocator.
 */
static uint64_t delete_table(struct engine *e, uint64_t table_offset, quid_t *quid) {
	if (!table_offset) {
		ERROR(EREC_NOTFOUND, EL_WARN);
		return 0;
	}
	struct engine_table *table = get_table(e, table_offset);

	size_t left = 0, right = table->size;
	while (left < right) {
		size_t i = (right - left) / 2 + left;
		int cmp = quidcmp(quid, &table->items[i].quid);
		if (cmp == 0) {
			/* found */
			if (table->items[i].meta.syslock || table->items[i].meta.freeze) {
				ERROR(EREC_LOCKED, EL_WARN);
				put_table(e, table, table_offset);
				return 0;
			}
			uint64_t ret = remove_table(e, table, i, quid);
			flush_table(e, table, table_offset);
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
	uint64_t ret = delete_table(e, child, quid);
	if (ret != 0)
		table->items[i].child = to_be64(table_join(e, child));

	if (ret == 0 && delete_larger && i < table->size) {
		/* remove the next largest */
		ret = remove_table(e, table, i, quid);
	}
	if (ret != 0) {
		/* flush just in case changes happened */
		flush_table(e, table, table_offset);
	} else {
		put_table(e, table, table_offset);
	}
	return ret;
}

static uint64_t insert_toplevel(struct engine *e, uint64_t *table_offset, quid_t *quid, const void *data, size_t len) {
	uint64_t offset = 0;
	uint64_t ret = 0;
	uint64_t right_child = 0;
	if (*table_offset != 0) {
		ret = insert_table(e, *table_offset, quid, data, len);

		/* check if we need to split */
		struct engine_table *table = get_table(e, *table_offset);
		if (table->size < TABLE_SIZE-1) {
			/* nothing to do */
			put_table(e, table, *table_offset);
			return ret;
		}
		right_child = split_table(e, table, quid, &offset);
		flush_table(e, table, *table_offset);
	} else {
		ret = offset = insert_data(e, data, len);
	}

	/* create new top level table */
	struct engine_table *new_table = alloc_table();
	new_table->size = 1;
	memcpy(&new_table->items[0].quid, quid, sizeof(quid_t));
	new_table->items[0].offset = to_be64(offset);
	new_table->items[0].meta.importance = MD_IMPORTANT_NORMAL;
	new_table->items[0].child = to_be64(*table_offset);
	new_table->items[1].child = to_be64(right_child);

	uint64_t new_table_offset = alloc_index_chunk(e, sizeof *new_table);
	flush_table(e, new_table, new_table_offset);

	*table_offset = new_table_offset;
	return ret;
}

int engine_insert(struct engine *e, const quid_t *c_quid, const void *data, size_t len) {
	ERRORZEOR();
	if (e->lock == LOCK) {
		ERROR(EDB_LOCKED, EL_WARN);
		return -1;
	}

	/* QUID must be in writable memory */
	quid_t quid;
	memcpy(&quid, c_quid, sizeof(quid_t));

	insert_toplevel(e, &e->top, &quid, data, len);
	flush_super(e);
	if(ISERROR())
		return -1;

    e->stats.keys++;
	return 0;
}

/*
 * Look up item with the given key 'quid' in the given table. Returns offset
 * to the item.
 */
static uint64_t lookup_key(struct engine *e, uint64_t table_offset, const quid_t *quid) {
	while (table_offset) {
		struct engine_table *table = get_table(e, table_offset);
		size_t left = 0, right = table->size, i;
		while (left < right) {
			i = (right - left) / 2 + left;
			int cmp = quidcmp(quid, &table->items[i].quid);
			if (cmp == 0) {
				/* found */
				if (table->items[i].meta.lifecycle != MD_LIFECYCLE_FINITE) {
					ERROR(EREC_NOTFOUND, EL_WARN);
					put_table(e, table, table_offset);
					return 0;
				}
				uint64_t ret = from_be64(table->items[i].offset);
				put_table(e, table, table_offset);
				return ret;
			}
			if (cmp < 0) {
				right = i;
			} else {
				left = i + 1;
			}
		}
		uint64_t child = from_be64(table->items[left].child);
		put_table(e, table, table_offset);
		table_offset = child;
	}
	ERROR(EREC_NOTFOUND, EL_WARN);
	return 0;
}

void *engine_get(struct engine *e, const quid_t *quid, size_t *len) {
	ERRORZEOR();
	if (e->lock == LOCK) {
		ERROR(EDB_LOCKED, EL_WARN);
		return NULL;
	}
	uint64_t offset = lookup_key(e, e->top, quid);
	if(ISERROR())
		return NULL;

	lseek(e->db_fd, offset, SEEK_SET);
	struct blob_info info;
	if (read(e->db_fd, &info, sizeof info) != (ssize_t) sizeof info) {
		lprintf("[erro] Failed to read disk\n");
		ERROR(EIO_READ, EL_FATAL);
		return NULL;
	}
	*len = from_be32(info.len);
	assert(*len > 0);

	void *data = malloc(*len);
	if (!data) {
		lprintf("[erro] Failed to request memory\n");
		ERROR(EM_ALLOC, EL_FATAL);
		return NULL;
	}
	if (read(e->db_fd, data, *len) != (ssize_t) *len) {
		lprintf("[erro] Failed to read disk\n");
		ERROR(EIO_READ, EL_FATAL);
		free(data);
		data = NULL;
		return NULL;
	}
	return data;
}

int engine_purge(struct engine *e, quid_t *quid) {
	ERRORZEOR();
	if (e->lock == LOCK) {
		ERROR(EDB_LOCKED, EL_WARN);
		return -1;
	}

	uint64_t offset = delete_table(e, e->top, quid);
	if(ISERROR())
		return -1;

	e->top = table_join(e, e->top);
	e->stats.keys--;

	free_dbchunk(e, offset);
	flush_super(e);
	return 0;
}

static struct metadata *get_meta(struct engine *e, uint64_t table_offset, const quid_t *quid, struct metadata *meta) {
	while (table_offset) {
		struct engine_table *table = get_table(e, table_offset);
		size_t left = 0, right = table->size, i;
		while (left < right) {
			i = (right - left) / 2 + left;
			int cmp = quidcmp(quid, &table->items[i].quid);
			if (cmp == 0) {
				if (table->items[i].meta.lifecycle != MD_LIFECYCLE_FINITE) {
					ERROR(EREC_NOTFOUND, EL_WARN);
					put_table(e, table, table_offset);
					return 0;
				}
				memcpy(meta, &table->items[i].meta, sizeof(struct metadata));
				put_table(e, table, table_offset);
				return meta;//&table->items[i].meta;
			}
			if (cmp < 0) {
				right = i;
			} else {
				left = i + 1;
			}
		}
		uint64_t child = from_be64(table->items[left].child);
		put_table(e, table, table_offset);
		table_offset = child;
	}
	ERROR(EREC_NOTFOUND, EL_WARN);
	return 0;
}

int engine_getmeta(struct engine *e, const quid_t *quid, struct metadata *md) {
	ERRORZEOR();
	if (e->lock == LOCK) {
		ERROR(EDB_LOCKED, EL_WARN);
		return -1;
	}
	get_meta(e, e->top, quid, md);
	if(ISERROR())
		return -1;
	return 0;
}

static int set_meta(struct engine *e, uint64_t table_offset, const quid_t *quid, const struct metadata *md) {
	while (table_offset) {
		struct engine_table *table = get_table(e, table_offset);
		size_t left = 0, right = table->size, i;
		while (left < right) {
			i = (right - left) / 2 + left;
			int cmp = quidcmp(quid, &table->items[i].quid);
			if (cmp == 0) {
				if (table->items[i].meta.syslock) {
					ERROR(EREC_LOCKED, EL_WARN);
					put_table(e, table, table_offset);
					return -1;
				}
				memcpy(&table->items[i].meta, md, sizeof(struct metadata));
				flush_table(e, table, table_offset);
				return 0;
			}
			if (cmp < 0) {
				right = i;
			} else {
				left = i + 1;
			}
		}
		uint64_t child = from_be64(table->items[left].child);
		put_table(e, table, table_offset);
		table_offset = child;
	}
	ERROR(EREC_NOTFOUND, EL_WARN);
	return -1;
}

int engine_setmeta(struct engine *e, const quid_t *quid, const struct metadata *data) {
	ERRORZEOR();
	if (e->lock == LOCK) {
		ERROR(EDB_LOCKED, EL_WARN);
		return -1;
	}
	set_meta(e, e->top, quid, data);
	if(ISERROR())
		return -1;
	return 0;
}

int engine_delete(struct engine *e, const quid_t *quid) {
	ERRORZEOR();
	if (e->lock == LOCK) {
		ERROR(EDB_LOCKED, EL_WARN);
		return -1;
	}

	struct metadata nmd;
	get_meta(e, e->top, quid, &nmd);
	if(ISERROR())
		return -1;

    nmd.lifecycle = MD_LIFECYCLE_RECYCLE;
	set_meta(e, e->top, quid, &nmd);
	if(ISERROR())
		return -1;

	flush_super(e);
	return 0;
}

#if 0
static void walk_dbstorage(struct engine *e) {
	uint64_t offset = sizeof(struct engine_dbsuper);
	struct blob_info info;

	while(1) {
		lseek(e->db_fd, offset, SEEK_SET);
		if (read(e->db_fd, &info, sizeof(struct blob_info)) != (ssize_t) sizeof(struct blob_info))
			return;

		offset = offset+page_align(sizeof(struct blob_info)+from_be32(info.len));
	}
}
#endif // 0

static void tree_traversal(struct engine *e, struct engine *ce, uint64_t table_offset) {
	int i;
	struct engine_table *table = get_table(e, table_offset);
	size_t sz = table->size;
	for(i=0; i<(int)sz; ++i) {
		uint64_t child = from_be64(table->items[i].child);
		uint64_t right = from_be64(table->items[i+1].child);
		uint64_t dboffset = from_be64(table->items[i].offset);

		lseek(e->db_fd, dboffset, SEEK_SET);
		struct blob_info info;
		if (read(e->db_fd, &info, sizeof(struct blob_info)) != sizeof(struct blob_info)) {
			lprintf("[erro] Failed to read disk\n");
			ERROR(EIO_READ, EL_FATAL);
			put_table(e, table, table_offset);
			return;
		}
		size_t len = from_be32(info.len);
		void *data = malloc(len);
		if (!data) {
			lprintf("[erro] Failed to request memory\n");
			ERROR(EM_ALLOC, EL_FATAL);
			put_table(e, table, table_offset);
			return;
		}
		if (read(e->db_fd, data, len) != (ssize_t) len) {
			lprintf("[erro] Failed to read disk\n");
			ERROR(EIO_READ, EL_FATAL);
			free(data);
			data = NULL;
			put_table(e, table, table_offset);
			return;
		}

		if (table->items[i].meta.lifecycle == MD_LIFECYCLE_FINITE) {
            insert_toplevel(ce, &ce->top, &table->items[i].quid, data, len);
            ce->stats.keys++;
            flush_super(ce);
		}

		free(data);
		if (child)
			tree_traversal(e, ce, child);
		if (right)
			tree_traversal(e, ce, right);
	}
	put_table(e, table, table_offset);
}

int engine_vacuum(struct engine *e, const char *fname) {
    char dbname[DBNAME_SIZE], idxname[DBNAME_SIZE], walname[DBNAME_SIZE];
	struct engine ce;
	struct engine tmp;

	ERRORZEOR();
	if (e->lock == LOCK) {
		ERROR(EDB_LOCKED, EL_WARN);
		return -1;
	}

	if (!e->stats.keys)
		return 0;

	e->lock = LOCK;
	snprintf(idxname, DBNAME_SIZE, "%s._%s", fname, IDXEXT);
	snprintf(dbname, DBNAME_SIZE, "%s._%s", fname, DBEXT);
	snprintf(walname, DBNAME_SIZE, "%s._%s", fname, LOGEXT);

	engine_create(&ce, idxname, dbname, walname);
	tree_traversal(e, &ce, e->top);

	memcpy(&tmp, e, sizeof(struct engine));
	memcpy(e, &ce, sizeof(struct engine));
	engine_close(&tmp);

	create_backup(fname);

	return 0;
}

int engine_update(struct engine *e, const quid_t *quid, const void *data, size_t len) {
	ERRORZEOR();
	if (e->lock == LOCK) {
		ERROR(EDB_LOCKED, EL_WARN);
		return -1;
	}

    uint64_t offset = 0;
    uint64_t table_offset = e->top;
	while (table_offset) {
		struct engine_table *table = get_table(e, table_offset);
		size_t left = 0, right = table->size, i;
		while (left < right) {
			i = (right - left) / 2 + left;
			int cmp = quidcmp(quid, &table->items[i].quid);
			if (cmp == 0) {
				if (table->items[i].meta.syslock) {
					ERROR(EREC_LOCKED, EL_WARN);
					put_table(e, table, table_offset);
					return -1;
				}
				offset = from_be64(table->items[i].offset);
				free_dbchunk(e, offset);
				offset = insert_data(e, data, len);
				table->items[i].offset = to_be64(offset);
				flush_table(e, table, table_offset);
				goto done;
			}
			if (cmp < 0) {
				right = i;
			} else {
				left = i + 1;
			}
		}
		uint64_t child = from_be64(table->items[left].child);
		put_table(e, table, table_offset);
		table_offset = child;
	}

done:
	flush_super(e);
	if(ISERROR())
		return -1;
	return 0;
}

char *get_str_lifecycle(enum key_lifecycle lifecycle) {
	static char buf[STATUS_LIFECYCLE_SIZE];
	switch (lifecycle) {
		case MD_LIFECYCLE_FINITE:
			strlcpy(buf, "FINITE", STATUS_LIFECYCLE_SIZE);
			break;
		case MD_LIFECYCLE_INVALID:
			strlcpy(buf, "INVALID", STATUS_LIFECYCLE_SIZE);
			break;
		case MD_LIFECYCLE_CORRUPT:
			strlcpy(buf, "CORRUPT", STATUS_LIFECYCLE_SIZE);
			break;
		case MD_LIFECYCLE_RECYCLE:
			strlcpy(buf, "RECYCLE", STATUS_LIFECYCLE_SIZE);
			break;
		case MD_LIFECYCLE_INACTIVE:
			strlcpy(buf, "INACTIVE", STATUS_LIFECYCLE_SIZE);
			break;
		case MD_LIFECYCLE_UNKNOWN:
		default:
			strlcpy(buf, "UNKNOWN", STATUS_LIFECYCLE_SIZE);
			break;
	}
	return buf;
}

char *get_str_type(enum key_type key_type) {
	static char buf[STATUS_TYPE_SIZE];
	switch (key_type) {
		case MD_TYPE_SIGNED:
			strlcpy(buf, "SIGNED", STATUS_TYPE_SIZE);
			break;
		case MD_TYPE_BOOL_FALSE:
			strlcpy(buf, "BOOL_FALSE", STATUS_TYPE_SIZE);
			break;
		case MD_TYPE_BOOL_TRUE:
			strlcpy(buf, "BOOL_TRUE", STATUS_TYPE_SIZE);
			break;
		case MD_TYPE_POINTER:
			strlcpy(buf, "POINTER", STATUS_TYPE_SIZE);
			break;
		case MD_TYPE_DATA:
		default:
			strlcpy(buf, "DATA", STATUS_TYPE_SIZE);
			break;
	}
	return buf;
}

enum key_lifecycle get_meta_lifecycle(char *lifecycle) {
	if (!strcmp(lifecycle, "FINITE"))
		return MD_LIFECYCLE_FINITE;
	else if (!strcmp(lifecycle, "INVALID"))
		return MD_LIFECYCLE_INVALID;
	else if (!strcmp(lifecycle, "CORRUPT"))
		return MD_LIFECYCLE_CORRUPT;
	else if (!strcmp(lifecycle, "RECYCLE"))
		return MD_LIFECYCLE_RECYCLE;
	else if (!strcmp(lifecycle, "INACTIVE"))
		return MD_LIFECYCLE_INACTIVE;
	else
		return MD_LIFECYCLE_UNKNOWN;
}

enum key_type get_meta_type(char *key_type) {
	if (!strcmp(key_type, "SIGNED"))
		return MD_TYPE_SIGNED;
	else if (!strcmp(key_type, "BOOL_FALSE"))
		return MD_TYPE_BOOL_FALSE;
	else if (!strcmp(key_type, "BOOL_TRUE"))
		return MD_TYPE_BOOL_TRUE;
	else if (!strcmp(key_type, "POINTER"))
		return MD_TYPE_POINTER;
	else
		return MD_TYPE_DATA;
}
