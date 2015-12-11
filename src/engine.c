#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <config.h>
#include <common.h>
#include <log.h>
#include <error.h>
#include "zmalloc.h"
#include "arc4random.h"
#include "crc64.h"
#include "jenhash.h"
#include "quid.h"
#include "dict.h"
#include "marshall.h"
#include "core.h"
#include "engine.h"

static int delete_larger = 0;
static uint64_t last_blob = 0;

static void flush_super(struct engine *e, bool fast);
static void flush_dbsuper(struct engine *e);
static uint64_t remove_table(struct engine *e, struct engine_table *table, size_t i, quid_t *quid);

struct {
	enum key_type type;
	bool dataheap;
} keytype_info[] = {

	/* No data */
	{MD_TYPE_INDEX,		FALSE},
	{MD_TYPE_RAW,		FALSE},

	/* Containing data */
	{MD_TYPE_RECORD,	TRUE},
	{MD_TYPE_GROUP,		TRUE},
};

/*
 * Does marshall type require additional data
 */
bool engine_keytype_hasdata(enum key_type type) {
	for (unsigned int i = 0; i < RSIZE(keytype_info); ++i) {
		if (keytype_info[i].type == type)
			return keytype_info[i].dataheap;
	}
	return FALSE;
}

static struct engine_table *alloc_table() {
	struct engine_table *table = zmalloc(sizeof(struct engine_table));
	if (!table) {
		zfree(table);
		error_throw_fatal("7b8a6ac440e2", "Failed to request memory");
		return NULL;
	}
	memset(table, 0, sizeof(struct engine_table));
	return table;
}

static struct engine_tablelist *alloc_tablelist() {
	struct engine_tablelist *tablelist = zcalloc(1, sizeof(struct engine_tablelist));
	if (!tablelist) {
		zfree(tablelist);
		error_throw_fatal("7b8a6ac440e2", "Failed to request memory");
		return NULL;
	}
	return tablelist;
}

static struct engine_index_list *alloc_indexlist() {
	struct engine_index_list *indexlist = zcalloc(1, sizeof(struct engine_index_list));
	if (!indexlist) {
		zfree(indexlist);
		error_throw_fatal("7b8a6ac440e2", "Failed to request memory");
		return NULL;
	}
	return indexlist;
}

static struct engine_table *get_table(struct engine *e, uint64_t offset) {
	zassert(offset != 0);

	/* take from cache */
	struct engine_cache *slot = &e->cache[offset % CACHE_SLOTS];
	if (slot->offset == offset) {
		slot->offset = 0;
		return slot->table;
	}

	struct engine_table *table = zmalloc(sizeof(struct engine_table));
	if (!table) {
		zfree(table);
		error_throw_fatal("7b8a6ac440e2", "Failed to request memory");
		return NULL;
	}

	if (lseek(e->fd, offset, SEEK_SET) < 0) {
		zfree(table);
		error_throw_fatal("a7df40ba3075", "Failed to read disk");
		return NULL;
	}
	if (read(e->fd, table, sizeof(struct engine_table)) != sizeof(struct engine_table)) {
		zfree(table);
		error_throw_fatal("a7df40ba3075", "Failed to read disk");
		return NULL;
	}
	return table;
}

static struct engine_tablelist *get_tablelist(struct engine *e, uint64_t offset) {
	zassert(offset != 0);

	struct engine_tablelist *tablelist = zmalloc(sizeof(struct engine_tablelist));
	if (!tablelist) {
		zfree(tablelist);
		error_throw_fatal("7b8a6ac440e2", "Failed to request memory");
		return NULL;
	}

	if (lseek(e->fd, offset, SEEK_SET) < 0) {
		zfree(tablelist);
		error_throw_fatal("a7df40ba3075", "Failed to read disk");
		return NULL;
	}
	if (read(e->fd, tablelist, sizeof(struct engine_tablelist)) != sizeof(struct engine_tablelist)) {
		zfree(tablelist);
		error_throw_fatal("a7df40ba3075", "Failed to read disk");
		return NULL;
	}
	return tablelist;
}

static struct engine_index_list *get_indexlist(struct engine *e, uint64_t offset) {
	zassert(offset != 0);

	struct engine_index_list *indexlist = zmalloc(sizeof(struct engine_index_list));
	if (!indexlist) {
		zfree(indexlist);
		error_throw_fatal("7b8a6ac440e2", "Failed to request memory");
		return NULL;
	}

	if (lseek(e->fd, offset, SEEK_SET) < 0) {
		zfree(indexlist);
		error_throw_fatal("a7df40ba3075", "Failed to read disk");
		return NULL;
	}
	if (read(e->fd, indexlist, sizeof(struct engine_index_list)) != sizeof(struct engine_index_list)) {
		zfree(indexlist);
		error_throw_fatal("a7df40ba3075", "Failed to read disk");
		return NULL;
	}
	return indexlist;
}

/* Free a table acquired with alloc_table() or get_table() */
static void put_table(struct engine *e, struct engine_table *table, uint64_t offset) {
	zassert(offset != 0);

	/* overwrite cache */
	struct engine_cache *slot = &e->cache[offset % CACHE_SLOTS];
	if (slot->offset != 0) {
		zfree(slot->table);
	}
	slot->offset = offset;
	slot->table = table;
}

/* Write a table and free it */
static void flush_table(struct engine *e, struct engine_table *table, uint64_t offset) {
	zassert(offset != 0);

	if (lseek(e->fd, offset, SEEK_SET) < 0) {
		error_throw_fatal("1fd531fa70c1", "Failed to write disk");
		return;
	}
	if (write(e->fd, table, sizeof(struct engine_table)) != sizeof(struct engine_table)) {
		error_throw_fatal("1fd531fa70c1", "Failed to write disk");
		return;
	}
	put_table(e, table, offset);
}

/* Write a tablelist and free it */
static void flush_tablelist(struct engine *e, struct engine_tablelist *tablelist, uint64_t offset) {
	zassert(offset != 0);

	if (lseek(e->fd, offset, SEEK_SET) < 0) {
		error_throw_fatal("1fd531fa70c1", "Failed to write disk");
		return;
	}
	if (write(e->fd, tablelist, sizeof(struct engine_tablelist)) != sizeof(struct engine_tablelist)) {
		error_throw_fatal("1fd531fa70c1", "Failed to write disk");
		return;
	}
	zfree(tablelist);
}

/* Write a tablelist and free it */
static void flush_indexlist(struct engine *e, struct engine_index_list *indexlist, uint64_t offset) {
	zassert(offset != 0);

	if (lseek(e->fd, offset, SEEK_SET) < 0) {
		error_throw_fatal("1fd531fa70c1", "Failed to write disk");
		return;
	}
	if (write(e->fd, indexlist, sizeof(struct engine_index_list)) != sizeof(struct engine_index_list)) {
		error_throw_fatal("1fd531fa70c1", "Failed to write disk");
		return;
	}
	zfree(indexlist);
}

static int engine_open(struct engine *e, const char *idxname, const char *dbname) {
	memset(e, 0, sizeof(struct engine));
	e->fd = open(idxname, O_RDWR | O_BINARY);
	e->db_fd = open(dbname, O_RDWR | O_BINARY);
	if (e->fd < 0 || e->db_fd < 0)
		return -1;

	struct engine_super super;
	if (read(e->fd, &super, sizeof(struct engine_super)) != sizeof(struct engine_super)) {
		error_throw_fatal("a7df40ba3075", "Failed to read disk");
		return -1;
	}
	e->top = from_be64(super.top);
	e->free_top = from_be64(super.free_top);
	e->stats.keys = from_be64(super.nkey);
	e->stats.free_tables = from_be64(super.nfree_table);
	e->stats.list_size = from_be64(super.list_size);
	e->stats.index_list_size = from_be64(super.index_list_size);
	e->list_top = from_be64(super.list_top);
	e->index_list_top = from_be64(super.index_list_top);
	zassert(from_be64(super.version) == VERSION_MAJOR);

	uint64_t crc64sum;
	if (!crc_file(e->fd, &crc64sum)) {
		lprint("[erro] Failed to calculate CRC\n");
		return -1;
	}
	/* TODO: run the diagnose process */
	if (from_be64(super.crc_zero_key) != crc64sum)
		lprint("[erro] Index CRC doenst match\n");

	struct engine_dbsuper dbsuper;
	if (read(e->db_fd, &dbsuper, sizeof(struct engine_dbsuper)) != sizeof(struct engine_dbsuper)) {
		error_throw_fatal("a7df40ba3075", "Failed to read disk");
		return -1;
	}
	zassert(from_be64(dbsuper.version) == VERSION_MAJOR);
	last_blob = from_be64(dbsuper.last);

	long _alloc = lseek(e->fd, 0, SEEK_END);
	long _db_alloc = lseek(e->db_fd, 0, SEEK_END);

	if (_alloc < 0 || _db_alloc < 0) {
		error_throw_fatal("a7df40ba3075", "Failed to read disk");
		return -1;
	}

	e->alloc = _alloc;
	e->db_alloc = _db_alloc;
	return 0;
}

static int engine_create(struct engine *e, const char *idxname, const char *dbname) {
	memset(e, 0, sizeof(struct engine));
	e->fd = open(idxname, O_RDWR | O_TRUNC | O_CREAT | O_BINARY, 0644);
	e->db_fd = open(dbname, O_RDWR | O_TRUNC | O_CREAT | O_BINARY, 0644);
	if (e->fd < 0 || e->db_fd < 0)
		return -1;

	last_blob = 0;
	flush_super(e, TRUE);
	flush_dbsuper(e);

	e->alloc = sizeof(struct engine_super);
	e->db_alloc = sizeof(struct engine_dbsuper);
	return 0;
}

void engine_init(struct engine *e, const char *fname, const char *dbname) {
	if (file_exists(fname) && file_exists(dbname)) {
		engine_open(e, fname, dbname);
	} else {
		engine_create(e, fname, dbname);
	}
}

void engine_close(struct engine *e) {
	engine_sync(e);
	close(e->fd);
	close(e->db_fd);

	for (size_t i = 0; i < CACHE_SLOTS; ++i) {
		if (e->cache[i].offset) {
			zfree(e->cache[i].table);
		}
	}
}

void engine_sync(struct engine *e) {
	flush_super(e, FALSE);
	flush_dbsuper(e);
}

/* Return a value that is greater or equal to 'val' and is power-of-two. */
static size_t page_align(size_t val) {
	size_t i = 1;
	while (i < val)
		i <<= 1;
	return i;
}

static uint64_t page_swap(struct engine *e, size_t len) {
	len = page_align(len);
	uint64_t offset = e->alloc;

	/* this is important to performance */
	if (offset & (len - 1)) {
		offset += len - (offset & (len - 1));
	}
	e->alloc = offset + len;
	return offset;
}

/* Allocate a chunk from the index file for new table */
static uint64_t alloc_table_chunk(struct engine *e, size_t len) {
	zassert(len > 0);

	/* Use blocks from freelist instead of allocation */
	if (e->free_top) {
		uint64_t offset = e->free_top;
		struct engine_table *table = get_table(e, offset);
		e->free_top = from_be64(table->items[0].child);
		e->stats.free_tables--;

		zfree(table);
		return offset;
	}

	return page_swap(e, len);
}

/* Allocate a chunk from the index file */
static uint64_t alloc_raw_chunk(struct engine *e, size_t len) {
	zassert(len > 0);

	return page_swap(e, len);
}

/* Allocate a chunk from the database file */
static uint64_t alloc_dbchunk(struct engine *e, size_t len) {
	zassert(len > 0);

	if (!e->dbcache[DBCACHE_SLOTS - 1].len)
		goto new_block;

	for (int i = 0; i < DBCACHE_SLOTS; ++i) {
		struct engine_dbcache *slot = &e->dbcache[i];
		if (len <= slot->len) {
			int diff = (((double)(len) / (double)slot->len) * 100);
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
	zassert(offset > 0);
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
	struct blob_info info;

	if (lseek(e->db_fd, offset, SEEK_SET) < 0) {
		error_throw_fatal("a7df40ba3075", "Failed to read disk");
		return;
	}
	if (read(e->db_fd, &info, sizeof(struct blob_info)) != sizeof(struct blob_info)) {
		error_throw_fatal("a7df40ba3075", "Failed to read disk");
		return;
	}

	info.free = 1;
	info.next = 0;
	struct engine_dbcache dbinfo;
	size_t len = from_be32(info.len);
	dbinfo.offset = offset;
	for (int i = DBCACHE_SLOTS - 1; i >= 0; --i) {
		struct engine_dbcache *slot = &e->dbcache[i];
		if (len > slot->len) {
			if (slot->len) {
				for (int j = 0; j < i; ++j) {
					e->dbcache[j] = e->dbcache[j + 1];
				}
			}
			dbinfo.len = len;
			e->dbcache[i] = dbinfo;
			break;
		}
	}

	if (lseek(e->db_fd, offset, SEEK_SET) < 0) {
		error_throw_fatal("1fd531fa70c1", "Failed to write disk");
		return;
	}
	if (write(e->db_fd, &info, sizeof(struct blob_info)) != sizeof(struct blob_info)) {
		error_throw_fatal("1fd531fa70c1", "Failed to write disk");
		return;
	}
}

static void flush_super(struct engine *e, bool fast_flush) {
	uint64_t crc64sum;
	struct engine_super super;
	memset(&super, 0, sizeof(struct engine_super));
	super.version = to_be64(VERSION_MAJOR);
	super.top = to_be64(e->top);
	super.free_top = to_be64(e->free_top);
	super.nkey = to_be64(e->stats.keys);
	super.nfree_table = to_be64(e->stats.free_tables);
	super.list_size = to_be64(e->stats.list_size);
	super.index_list_size = to_be64(e->stats.index_list_size);
	super.list_top = to_be64(e->list_top);
	super.index_list_top = to_be64(e->index_list_top);
	if (fast_flush)
		goto flush_disk;

	if (lseek(e->fd, sizeof(struct engine_super), SEEK_SET) < 0) {
		lprint("[erro] Failed to calculate CRC\n");
		return;
	}
	if (!crc_file(e->fd, &crc64sum)) {
		lprint("[erro] Failed to calculate CRC\n");
		return;
	}
	super.crc_zero_key = to_be64(crc64sum);

flush_disk:
	if (lseek(e->fd, 0, SEEK_SET) < 0) {
		error_throw_fatal("1fd531fa70c1", "Failed to write disk");
		return;
	}
	if (write(e->fd, &super, sizeof(struct engine_super)) != sizeof(struct engine_super)) {
		error_throw_fatal("1fd531fa70c1", "Failed to write disk");
		return;
	}
}

static void flush_dbsuper(struct engine *e) {
	struct engine_dbsuper dbsuper;
	memset(&dbsuper, 0, sizeof(struct engine_dbsuper));
	dbsuper.version = to_be64(VERSION_MAJOR);
	dbsuper.last = to_be64(last_blob);

	if (lseek(e->db_fd, 0, SEEK_SET) < 0) {
		error_throw_fatal("1fd531fa70c1", "Failed to write disk");
		return;
	}
	if (write(e->db_fd, &dbsuper, sizeof(struct engine_dbsuper)) != sizeof(struct engine_dbsuper)) {
		error_throw_fatal("1fd531fa70c1", "Failed to write disk");
		return;
	}
}

static uint64_t insert_data(struct engine *e, const void *data, size_t len) {
	if (data == NULL || len == 0) {
		error_throw("e8880046e019", "No data provided");
		return len;
	}

	struct blob_info info;
	memset(&info, 0, sizeof(struct blob_info));
	info.len = to_be32(len);
	info.free = 0;

	uint64_t offset = alloc_dbchunk(e, len);
	info.next = to_be64(last_blob);
	last_blob = offset;

	if (lseek(e->db_fd, offset, SEEK_SET) < 0) {
		error_throw_fatal("1fd531fa70c1", "Failed to write disk");
		return 0;
	}
	if (write(e->db_fd, &info, sizeof(struct blob_info)) != sizeof(struct blob_info)) {
		error_throw_fatal("1fd531fa70c1", "Failed to write disk");
		return 0;
	}
	if (write(e->db_fd, data, len) != (ssize_t)len) {
		error_throw_fatal("1fd531fa70c1", "Failed to write disk");
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

	uint64_t new_table_offset = alloc_table_chunk(e, sizeof(struct engine_table));
	flush_table(e, new_table, new_table_offset);

	return new_table_offset;
}

/* Try to table_rejoin the given table. Returns a new table offset. */
static uint64_t table_join(struct engine *e, uint64_t table_offset) {
	struct engine_table *table = get_table(e, table_offset);
	if (table->size == 0) {
		uint64_t ret = from_be64(table->items[0].child);
		free_index_chunk(e, table_offset);

		zfree(table);
		return ret;
	}
	put_table(e, table, table_offset);
	return table_offset;
}

/* Find and remove the smallest item from the given table. The key of the item
   is stored to 'quid'. Returns offset to the item */
static uint64_t take_smallest(struct engine *e, uint64_t table_offset, quid_t *quid) {
	struct engine_table *table = get_table(e, table_offset);
	zassert(table->size > 0);

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
	zassert(table->size > 0);

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
	zassert(i < table->size);

	if (quid)
		memcpy(quid, &table->items[i].quid, sizeof(quid_t));

	uint64_t offset = from_be64(table->items[i].offset);
	uint64_t left_child = from_be64(table->items[i].child);
	uint64_t right_child = from_be64(table->items[i + 1].child);

	if (left_child != 0 && right_child != 0) {
		/* replace the removed item by taking an item from one of the child tables */
		uint64_t new_offset;
		if (arc4random() & 1) {
			new_offset = take_largest(e, left_child, &table->items[i].quid);
			table->items[i].child = to_be64(table_join(e, left_child));
		} else {
			new_offset = take_smallest(e, right_child, &table->items[i].quid);
			table->items[i + 1].child = to_be64(table_join(e, right_child));
		}
		table->items[i].offset = to_be64(new_offset);
	} else {
		memmove(&table->items[i], &table->items[i + 1], (table->size - i) * sizeof(struct engine_item));
		table->size--;

		if (left_child != 0) {
			table->items[i].child = to_be64(left_child);
		} else {
			table->items[i].child = to_be64(right_child);
		}
	}
	return offset;
}

/* Insert a new item with key 'quid' with the contents in 'data' to the given table.
   Returns offset to the new item. */
static uint64_t insert_table(struct engine *e, uint64_t table_offset, quid_t *quid, struct metadata *meta, const void *data, size_t len) {
	struct engine_table *table = get_table(e, table_offset);
	zassert(table->size < TABLE_SIZE - 1);

	size_t left = 0, right = table->size;
	while (left < right) {
		size_t i = (right - left) / 2 + left;
		int cmp = quidcmp(quid, &table->items[i].quid);
		if (cmp == 0) {
			/* already in the table */
			uint64_t ret = from_be64(table->items[i].offset);
			put_table(e, table, table_offset);
			error_throw("a475446c70e8", "Key exists");
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
		ret = insert_table(e, left_child, quid, meta, data, len);

		/* check if we need to split */
		struct engine_table *child = get_table(e, left_child);
		if (child->size < TABLE_SIZE - 1) {
			/* nothing to do */
			put_table(e, table, table_offset);
			put_table(e, child, left_child);
			return ret;
		}
		/* overwrites QUID */
		right_child = split_table(e, child, quid, &offset);
		/* flush just in case changes happened */
		flush_table(e, child, left_child);
	} else {
		if (data && len > 0) {
			ret = offset = insert_data(e, data, len);
		}
	}

	table->size++;
	memmove(&table->items[i + 1], &table->items[i], (table->size - i) * sizeof(struct engine_item));
	memcpy(&table->items[i].quid, quid, sizeof(quid_t));
	table->items[i].offset = to_be64(offset);
	memcpy(&table->items[i].meta, meta, sizeof(struct metadata));
	table->items[i].child = to_be64(left_child);
	table->items[i + 1].child = to_be64(right_child);

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
		error_throw("6ef42da7901f", "Record not found");
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
				error_throw("4987a3310049", "Record locked");
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

static uint64_t insert_toplevel(struct engine *e, uint64_t *table_offset, quid_t *quid, struct metadata *meta, const void *data, size_t len) {
	uint64_t offset = 0;
	uint64_t ret = 0;
	uint64_t right_child = 0;
	if (*table_offset != 0) {
		ret = insert_table(e, *table_offset, quid, meta, data, len);

		/* check if we need to split */
		struct engine_table *table = get_table(e, *table_offset);
		if (table->size < TABLE_SIZE - 1) {
			/* nothing to do */
			put_table(e, table, *table_offset);
			return ret;
		}
		right_child = split_table(e, table, quid, &offset);
		flush_table(e, table, *table_offset);
	} else {
		if (data && len > 0) {
			ret = offset = insert_data(e, data, len);
		}
	}

	/* create new top level table */
	struct engine_table *new_table = alloc_table();
	new_table->size = 1;
	memcpy(&new_table->items[0].quid, quid, sizeof(quid_t));
	new_table->items[0].offset = to_be64(offset);
	memcpy(&new_table->items[0].meta, meta, sizeof(struct metadata));
	new_table->items[0].child = to_be64(*table_offset);
	new_table->items[1].child = to_be64(right_child);

	uint64_t new_table_offset = alloc_table_chunk(e, sizeof(struct engine_table));
	flush_table(e, new_table, new_table_offset);

	*table_offset = new_table_offset;
	return ret;
}

int engine_insert_data(struct engine *e, quid_t *quid, const void *data, size_t len) {
	if (e->lock == LOCK) {
		error_throw("986154f80058", "Database locked");
		return -1;
	}

	struct metadata meta;
	memset(&meta, 0, sizeof(struct metadata));
	meta.importance = MD_IMPORTANT_NORMAL;

	insert_toplevel(e, &e->top, quid, &meta, data, len);
	flush_super(e, TRUE);
	if (iserror())
		return -1;

	e->stats.keys++;
	return 0;
}

int engine_insert_meta_data(struct engine *e, quid_t *quid, struct metadata *meta, const void *data, size_t len) {
	if (e->lock == LOCK) {
		error_throw("986154f80058", "Database locked");
		return -1;
	}

	insert_toplevel(e, &e->top, quid, meta, data, len);
	flush_super(e, TRUE);
	if (iserror())
		return -1;

	e->stats.keys++;
	return 0;
}

int engine_insert(struct engine *e, quid_t *quid) {
	if (e->lock == LOCK) {
		error_throw("986154f80058", "Database locked");
		return -1;
	}

	struct metadata meta;
	memset(&meta, 0, sizeof(struct metadata));
	meta.nodata = TRUE;
	meta.importance = MD_IMPORTANT_NORMAL;

	insert_toplevel(e, &e->top, quid, &meta, NULL, 0);
	flush_super(e, TRUE);
	if (iserror())
		return -1;

	e->stats.keys++;
	return 0;
}

int engine_insert_meta(struct engine *e, quid_t *quid, struct metadata *meta) {
	if (e->lock == LOCK) {
		error_throw("986154f80058", "Database locked");
		return -1;
	}

	insert_toplevel(e, &e->top, quid, meta, NULL, 0);
	flush_super(e, TRUE);
	if (iserror())
		return -1;

	e->stats.keys++;
	return 0;
}

/*
 * Look up item with the given key 'quid' in the given table. Returns offset
 * to the item.
 */
static uint64_t lookup_key(struct engine *e, uint64_t table_offset, const quid_t *quid, bool *nodata, struct metadata *meta) {
	while (table_offset) {
		struct engine_table *table = get_table(e, table_offset);
		size_t left = 0, right = table->size;
		while (left < right) {
			size_t i;
			i = (right - left) / 2 + left;
			int cmp = quidcmp(quid, &table->items[i].quid);
			if (cmp == 0) {
				/* found */
				if (table->items[i].meta.lifecycle != MD_LIFECYCLE_FINITE) {
					error_throw("6ef42da7901f", "Record not found");
					put_table(e, table, table_offset);
					return 0;
				}
				uint64_t ret = from_be64(table->items[i].offset);
				*nodata = table->items[i].meta.nodata;
				memcpy(meta, &table->items[i].meta, sizeof(struct metadata));
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
	error_throw("6ef42da7901f", "Record not found");
	return 0;
}

static void *get_data(struct engine *e, uint64_t offset, size_t *len) {
	struct blob_info info;

	if (lseek(e->db_fd, offset, SEEK_SET) < 0) {
		error_throw_fatal("a7df40ba3075", "Failed to read disk");
		return NULL;
	}
	if (read(e->db_fd, &info, sizeof(struct blob_info)) != (ssize_t)sizeof(struct blob_info)) {
		error_throw_fatal("a7df40ba3075", "Failed to read disk");
		return NULL;
	}

	*len = from_be32(info.len);
	if (!*len)
		return NULL;

	void *data = zmalloc(*len);
	if (!data) {
		error_throw_fatal("7b8a6ac440e2", "Failed to request memory");
		return NULL;
	}

	if (read(e->db_fd, data, *len) != (ssize_t) *len) {
		error_throw_fatal("a7df40ba3075", "Failed to read disk");
		zfree(data);
		data = NULL;
		return NULL;
	}

	return data;
}

void *get_data_block(struct engine *e, uint64_t offset, size_t *len) {
	if (e->lock == LOCK) {
		error_throw("986154f80058", "Database locked");
		return NULL;
	}

	if (!offset)
		return NULL;

	return get_data(e, offset, len);
}

uint64_t engine_get(struct engine *e, const quid_t *quid, struct metadata *meta) {
	if (e->lock == LOCK) {
		error_throw("986154f80058", "Database locked");
		return 0;
	}
	bool nodata = 0;
	memset(meta, 0, sizeof(struct metadata));
	uint64_t offset = lookup_key(e, e->top, quid, &nodata, meta);
	if (iserror())
		return 0;

	if (nodata)
		return 0;

	return offset;
}

int engine_purge(struct engine *e, quid_t *quid) {
	if (e->lock == LOCK) {
		error_throw("986154f80058", "Database locked");
		return -1;
	}

	uint64_t offset = delete_table(e, e->top, quid);
	if (iserror())
		return -1;

	e->top = table_join(e, e->top);
	e->stats.keys--;

	free_dbchunk(e, offset);
	flush_super(e, TRUE);
	return 0;
}

static int set_meta(struct engine *e, uint64_t table_offset, const quid_t *quid, const struct metadata *md) {
	while (table_offset) {
		struct engine_table *table = get_table(e, table_offset);
		size_t left = 0, right = table->size;
		while (left < right) {
			size_t i = (right - left) / 2 + left;
			int cmp = quidcmp(quid, &table->items[i].quid);
			if (cmp == 0) {
				if (table->items[i].meta.syslock) {
					error_throw("4987a3310049", "Record locked");
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
	error_throw("6ef42da7901f", "Record not found");
	return -1;
}

int engine_setmeta(struct engine *e, const quid_t *quid, const struct metadata *data) {
	if (e->lock == LOCK) {
		error_throw("986154f80058", "Database locked");
		return -1;
	}
	set_meta(e, e->top, quid, data);
	if (iserror())
		return -1;
	return 0;
}

int engine_delete(struct engine *e, const quid_t *quid) {
	if (e->lock == LOCK) {
		error_throw("986154f80058", "Database locked");
		return -1;
	}

	bool nodata = 0;
	struct metadata meta;
	lookup_key(e, e->top, quid, &nodata, &meta);
	if (iserror())
		return -1;

	meta.lifecycle = MD_LIFECYCLE_RECYCLE;
	set_meta(e, e->top, quid, &meta);
	if (iserror())
		return -1;

	flush_super(e, TRUE);
	return 0;
}

int engine_recover_storage(struct engine *e) {
	uint64_t offset = last_blob;
	struct blob_info info;
	int cnt = 0;

	if (e->lock == LOCK) {
		error_throw("986154f80058", "Database locked");
		return -1;
	}

	lprint("[info] Start recovery process\n");
	if (!offset) {
		lprint("[erro] Metadata lost\n");
		return -1;
	}

	while (1) {
		cnt++;
		if (lseek(e->db_fd, offset, SEEK_SET) < 0) {
			error_throw_fatal("a7df40ba3075", "Failed to read disk");
			return -1;
		}
		if (read(e->db_fd, &info, sizeof(struct blob_info)) != (ssize_t) sizeof(struct blob_info)) {
			error_throw_fatal("a7df40ba3075", "Failed to read disk");
			return -1;
		}

		size_t len = from_be32(info.len);
		uint64_t next = from_be64(info.next);
		zassert(len > 0);
		if (next)
			offset = next;
		else
			break;
	}
	lprintf("[info] Lost %d records\n", e->stats.keys - cnt);
	return 0;
}

//TODO Copy over other keytypes, indexes, aliasses..
static void engine_copy(struct engine *e, struct engine *ce, uint64_t table_offset) {
	struct engine_table *table = get_table(e, table_offset);
	size_t sz = table->size;
	for (int i = 0; i < (int)sz; ++i) {
		uint64_t child = from_be64(table->items[i].child);
		uint64_t right = from_be64(table->items[i + 1].child);
		uint64_t dboffset = from_be64(table->items[i].offset);

		size_t len;
		void *data = get_data(e, dboffset, &len);

		/* Only copy active keys */
		if (table->items[i].meta.lifecycle == MD_LIFECYCLE_FINITE) {
			insert_toplevel(ce, &ce->top, &table->items[i].quid, &table->items[i].meta, data, len);
			ce->stats.keys++;
			flush_super(ce, TRUE);
		}

		zfree(data);
		if (child)
			engine_copy(e, ce, child);
		if (right)
			engine_copy(e, ce, right);
	}
	put_table(e, table, table_offset);
}

int engine_vacuum(struct engine *e, const char *fname, const char *dbname) {
	struct engine ce;
	struct engine tmp;

	if (e->lock == LOCK) {
		error_throw("986154f80058", "Database locked");
		return -1;
	}

	if (!e->stats.keys)
		return 0;

	lprint("[info] Start vacuum process\n");
	e->lock = LOCK;
	engine_create(&ce, fname, dbname);
	engine_copy(e, &ce, e->top);

	memcpy(&tmp, e, sizeof(struct engine));
	memcpy(e, &ce, sizeof(struct engine));
	engine_close(&tmp);

	return 0;
}

int engine_update_data(struct engine *e, const quid_t *quid, const void *data, size_t len) {
	if (e->lock == LOCK) {
		error_throw("986154f80058", "Database locked");
		return -1;
	}

	uint64_t offset = 0;
	uint64_t table_offset = e->top;
	while (table_offset) {
		struct engine_table *table = get_table(e, table_offset);
		size_t left = 0, right = table->size;
		while (left < right) {
			size_t i = (right - left) / 2 + left;
			int cmp = quidcmp(quid, &table->items[i].quid);
			if (cmp == 0) {
				if (table->items[i].meta.syslock) {
					error_throw("4987a3310049", "Record locked");
					put_table(e, table, table_offset);
					return -1;
				}
				offset = from_be64(table->items[i].offset);
				free_dbchunk(e, offset);
				offset = insert_data(e, data, len);
				table->items[i].offset = to_be64(offset);
				flush_table(e, table, table_offset);
				flush_super(e, TRUE);
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
	error_throw("6ef42da7901f", "Record not found");
	return -1;
}

int engine_list_insert(struct engine *e, const quid_t *c_quid, const char *name, size_t len) {
	if (e->lock == LOCK) {
		error_throw("986154f80058", "Database locked");
		return -1;
	}

	/* Name is max 32 */
	if (len > LIST_NAME_LENGTH) {
		len = LIST_NAME_LENGTH;
	}

	unsigned int hash = jen_hash((unsigned char *)name, len);

	/* does tablelist exist */
	if (e->list_top != 0) {
		struct engine_tablelist *tablelist = get_tablelist(e, e->list_top);
		zassert(tablelist->size <= LIST_SIZE - 1);

		memcpy(&tablelist->items[tablelist->size].quid, c_quid, sizeof(quid_t));
		memcpy(&tablelist->items[tablelist->size].name, name, len);
		tablelist->items[tablelist->size].len = to_be32(len);
		tablelist->items[tablelist->size].hash = to_be32(hash);
		tablelist->size++;

		e->stats.list_size++;

		/* check if we need to add a new table*/
		if (tablelist->size >= LIST_SIZE) {
			flush_tablelist(e, tablelist, e->list_top);

			struct engine_tablelist *new_tablelist = alloc_tablelist();
			new_tablelist->link = to_be64(e->list_top);
			uint64_t new_table_offset = alloc_raw_chunk(e, sizeof(struct engine_tablelist));
			flush_tablelist(e, new_tablelist, new_table_offset);

			e->list_top = new_table_offset;
		} else {
			flush_tablelist(e, tablelist, e->list_top);
		}
	} else {
		struct engine_tablelist *new_tablelist = alloc_tablelist();
		new_tablelist->size = 1;
		memcpy(&new_tablelist->items[0].quid, c_quid, sizeof(quid_t));
		memcpy(&new_tablelist->items[0].name, name, len);
		new_tablelist->items[0].len = to_be32(len);
		new_tablelist->items[0].hash = to_be32(hash);

		uint64_t new_table_offset = alloc_raw_chunk(e, sizeof(struct engine_tablelist));
		flush_tablelist(e, new_tablelist, new_table_offset);

		e->list_top = new_table_offset;
		e->stats.list_size = 1;
	}
	flush_super(e, TRUE);

	return 0;
}

char *engine_list_get_val(struct engine *e, const quid_t *c_quid) {
	if (e->lock == LOCK) {
		error_throw("986154f80058", "Database locked");
		return NULL;
	}

	uint64_t offset = e->list_top;
	while (offset) {
		struct engine_tablelist *tablelist = get_tablelist(e, offset);
		zassert(tablelist->size <= LIST_SIZE);

		for (int i = 0; i < tablelist->size; ++i) {
			int cmp = quidcmp(c_quid, &tablelist->items[i].quid);
			if (cmp == 0) {
				size_t len = from_be32(tablelist->items[i].len);
				char *name = (char *)zmalloc(len + 1);
				name[len] = '\0';
				memcpy(name, tablelist->items[i].name, len);
				zfree(tablelist);
				return name;
			}
		}
		if (tablelist->link) {
			offset = from_be64(tablelist->link);
		} else
			offset = 0;
		zfree(tablelist);
	}

	error_throw("2836444cd009", "Alias not found");
	return NULL;
}

int engine_list_get_key(struct engine *e, quid_t *key, const char *name, size_t len) {
	if (e->lock == LOCK) {
		error_throw("986154f80058", "Database locked");
		return -1;
	}

	unsigned int hash = jen_hash((unsigned char *)name, len);
	uint64_t offset = e->list_top;
	while (offset) {
		struct engine_tablelist *tablelist = get_tablelist(e, offset);
		zassert(tablelist->size <= LIST_SIZE);

		for (int i = 0; i < tablelist->size; ++i) {
			if (from_be32(tablelist->items[i].hash) == hash) {
				memcpy(key, &tablelist->items[i].quid, sizeof(quid_t));
				zfree(tablelist);
				return 0;
			}
		}
		if (tablelist->link) {
			offset = from_be64(tablelist->link);
		} else
			offset = 0;
		zfree(tablelist);
	}

	error_throw("2836444cd009", "Alias not found");
	return -1;
}

int engine_list_update(struct engine *e, const quid_t *c_quid, const char *name, size_t len) {
	if (e->lock == LOCK) {
		error_throw("986154f80058", "Database locked");
		return -1;
	}

	unsigned int hash = jen_hash((unsigned char *)name, len);
	uint64_t offset = e->list_top;
	while (offset) {
		struct engine_tablelist *tablelist = get_tablelist(e, offset);
		zassert(tablelist->size <= LIST_SIZE);

		for (int i = 0; i < tablelist->size; ++i) {
			int cmp = quidcmp(c_quid, &tablelist->items[i].quid);
			if (cmp == 0) {
				memcpy(&tablelist->items[i].name, name, len);
				tablelist->items[i].len = to_be32(len);
				tablelist->items[i].hash = to_be32(hash);
				flush_tablelist(e, tablelist, offset);
				return 0;
			}
		}
		if (tablelist->link) {
			offset = from_be64(tablelist->link);
		} else
			offset = 0;
		zfree(tablelist);
	}

	error_throw("2836444cd009", "Alias not found");
	return -1;
}

int engine_list_delete(struct engine *e, const quid_t *c_quid) {
	if (e->lock == LOCK) {
		error_throw("986154f80058", "Database locked");
		return -1;
	}

	uint64_t offset = e->list_top;
	while (offset) {
		struct engine_tablelist *tablelist = get_tablelist(e, offset);
		zassert(tablelist->size <= LIST_SIZE);

		for (int i = 0; i < tablelist->size; ++i) {
			int cmp = quidcmp(c_quid, &tablelist->items[i].quid);
			if (cmp == 0) {
				memset(&tablelist->items[i].quid, 0, sizeof(quid_t));
				tablelist->items[i].len = 0;
				flush_tablelist(e, tablelist, offset);
				e->stats.list_size--;
				return 0;
			}
		}
		if (tablelist->link) {
			offset = from_be64(tablelist->link);
		} else
			offset = 0;
		zfree(tablelist);
	}

	error_throw("2836444cd009", "Alias not found");
	return -1;
}

marshall_t *engine_list_all(struct engine *e) {
	if (e->lock == LOCK) {
		error_throw("986154f80058", "Database locked");
		return NULL;
	}

	if (!e->stats.list_size)
		return NULL;

	marshall_t *marshall = (marshall_t *)tree_zcalloc(1, sizeof(marshall_t), NULL);
	marshall->child = (marshall_t **)tree_zcalloc(e->stats.list_size, sizeof(marshall_t *), marshall);
	marshall->type = MTYPE_OBJECT;

	uint64_t offset = e->list_top;
	while (offset) {
		struct engine_tablelist *tablelist = get_tablelist(e, offset);
		zassert(tablelist->size <= LIST_SIZE);

		for (int i = 0; i < tablelist->size; ++i) {
			char squid[QUID_LENGTH + 1];
			quidtostr(squid, &tablelist->items[i].quid);
			size_t len = from_be32(tablelist->items[i].len);
			if (!len)
				continue;
			if (tablelist->items[i].name[0] == '_')
				continue;
			tablelist->items[i].name[len] = '\0';

			marshall->child[marshall->size] = tree_zcalloc(1, sizeof(marshall_t), marshall);
			marshall->child[marshall->size]->type = MTYPE_QUID;
			marshall->child[marshall->size]->name = tree_zstrdup(squid, marshall);
			marshall->child[marshall->size]->name_len = QUID_LENGTH;
			marshall->child[marshall->size]->data = tree_zstrdup(tablelist->items[i].name, marshall);
			marshall->child[marshall->size]->data_len = len;
			marshall->size++;
		}

		if (tablelist->link) {
			offset = from_be64(tablelist->link);
		} else
			offset = 0;

		zfree(tablelist);
	}

	return marshall;
}

int engine_index_list_insert(struct engine *e, const quid_t *index, const quid_t *group, char *element) {
	if (e->lock == LOCK) {
		error_throw("986154f80058", "Database locked");
		return -1;
	}

	/* does indexlist exist */
	if (e->index_list_top != 0) {
		struct engine_index_list *indexlist = get_indexlist(e, e->index_list_top);
		zassert(indexlist->size <= LIST_SIZE - 1);

		size_t psz = strlen(element);
		memcpy(&indexlist->items[indexlist->size].index, index, sizeof(quid_t));
		memcpy(&indexlist->items[indexlist->size].group, group, sizeof(quid_t));
		memcpy(&indexlist->items[indexlist->size].element, element, psz);
		indexlist->items[indexlist->size].element_len = to_be32(psz);
		indexlist->size++;

		e->stats.index_list_size++;

		/* check if we need to add a new table*/
		if (indexlist->size >= LIST_SIZE) {
			flush_indexlist(e, indexlist, e->index_list_top);

			struct engine_index_list *new_indexlist = alloc_indexlist();
			new_indexlist->link = to_be64(e->index_list_top);
			uint64_t new_index_table_offset = alloc_raw_chunk(e, sizeof(struct engine_index_list));
			flush_indexlist(e, new_indexlist, new_index_table_offset);

			e->index_list_top = new_index_table_offset;
		} else {
			flush_indexlist(e, indexlist, e->index_list_top);
		}
	} else {
		size_t psz = strlen(element);
		struct engine_index_list *new_indexlist = alloc_indexlist();
		new_indexlist->size = 1;
		memcpy(&new_indexlist->items[0].index, index, sizeof(quid_t));
		memcpy(&new_indexlist->items[0].group, group, sizeof(quid_t));
		memcpy(&new_indexlist->items[0].element, element, psz);
		new_indexlist->items[0].element_len = to_be32(psz);

		uint64_t new_index_table_offset = alloc_raw_chunk(e, sizeof(struct engine_index_list));
		flush_indexlist(e, new_indexlist, new_index_table_offset);

		e->index_list_top = new_index_table_offset;
		e->stats.index_list_size = 1;
	}
	flush_super(e, TRUE);

	return 0;
}

quid_t *engine_index_list_get_index(struct engine *e, const quid_t *c_quid) {
	if (e->lock == LOCK) {
		error_throw("986154f80058", "Database locked");
		return NULL;
	}

	uint64_t offset = e->index_list_top;
	while (offset) {
		struct engine_index_list *indexlist = get_indexlist(e, offset);
		zassert(indexlist->size <= LIST_SIZE);

		for (int i = 0; i < indexlist->size; ++i) {
			if (!indexlist->items[i].element_len)
				continue;

			int cmp = quidcmp(c_quid, &indexlist->items[i].group);
			if (cmp == 0) {
				quid_t *index = (quid_t *)zmalloc(sizeof(quid_t));
				memcpy(index, &indexlist->items[i].index, sizeof(quid_t));
				zfree(indexlist);
				return index;
			}
		}
		if (indexlist->link) {
			offset = from_be64(indexlist->link);
		} else
			offset = 0;
		zfree(indexlist);
	}

	error_throw("e553d927706a", "Index not found");
	return NULL;
}

marshall_t *engine_index_list_get_element(struct engine *e, const quid_t *c_quid) {
	if (e->lock == LOCK) {
		error_throw("986154f80058", "Database locked");
		return NULL;
	}

	int alloc_children = 10;

	marshall_t *marshall = (marshall_t *)tree_zcalloc(1, sizeof(marshall_t), NULL);
	marshall->child = (marshall_t **)tree_zcalloc(alloc_children, sizeof(marshall_t *), marshall);
	marshall->type = MTYPE_ARRAY;

	uint64_t offset = e->index_list_top;
	while (offset) {
		struct engine_index_list *indexlist = get_indexlist(e, offset);
		zassert(indexlist->size <= LIST_SIZE);

		for (int i = 0; i < indexlist->size; ++i) {
			if (!indexlist->items[i].element_len)
				continue;

			int cmp = quidcmp(c_quid, &indexlist->items[i].group);
			if (cmp == 0) {
				marshall->child[marshall->size] = tree_zcalloc(1, sizeof(marshall_t), marshall);
				marshall->child[marshall->size]->type = MTYPE_STRING;
				marshall->child[marshall->size]->data = tree_zstrdup(indexlist->items[i].element, marshall);
				marshall->child[marshall->size]->data_len = from_be32(indexlist->items[i].element_len);
				marshall->size++;
			}
		}

		if (indexlist->link) {
			offset = from_be64(indexlist->link);
		} else
			offset = 0;

		zfree(indexlist);
	}

	error_throw("e553d927706a", "Index not found");
	return marshall;
}

int engine_index_list_delete(struct engine *e, const quid_t *index) {
	if (e->lock == LOCK) {
		error_throw("986154f80058", "Database locked");
		return -1;
	}

	uint64_t offset = e->index_list_top;
	while (offset) {
		struct engine_index_list *indexlist = get_indexlist(e, offset);
		zassert(indexlist->size <= LIST_SIZE);

		for (int i = 0; i < indexlist->size; ++i) {
			if (!indexlist->items[i].element_len)
				continue;

			int cmp = quidcmp(index, &indexlist->items[i].index);
			if (cmp == 0) {
				memset(&indexlist->items[i].index, 0, sizeof(quid_t));
				memset(&indexlist->items[i].group, 0, sizeof(quid_t));
				memset(&indexlist->items[i].element, 0, 64);
				indexlist->items[i].element_len = 0;
				flush_indexlist(e, indexlist, offset);
				e->stats.index_list_size--;
				return 0;
			}
		}

		if (indexlist->link) {
			offset = from_be64(indexlist->link);
		} else
			offset = 0;

		zfree(indexlist);
	}

	error_throw("e553d927706a", "Index not found");
	return -1;
}

marshall_t *engine_index_list_all(struct engine *e) {
	if (e->lock == LOCK) {
		error_throw("986154f80058", "Database locked");
		return NULL;
	}

	if (!e->stats.index_list_size)
		return NULL;

	marshall_t *marshall = (marshall_t *)tree_zcalloc(1, sizeof(marshall_t), NULL);
	marshall->child = (marshall_t **)tree_zcalloc(e->stats.index_list_size, sizeof(marshall_t *), marshall);
	marshall->type = MTYPE_OBJECT;

	uint64_t offset = e->index_list_top;
	while (offset) {
		struct engine_index_list *indexlist = get_indexlist(e, offset);
		zassert(indexlist->size <= LIST_SIZE);

		for (int i = 0; i < indexlist->size; ++i) {
			char index_squid[QUID_LENGTH + 1];
			char group_squid[QUID_LENGTH + 1];

			if (!indexlist->items[i].element_len)
				continue;

			quidtostr(index_squid, &indexlist->items[i].index);
			quidtostr(group_squid, &indexlist->items[i].group);

			marshall->child[marshall->size] = tree_zcalloc(1, sizeof(marshall_t), marshall);
			marshall->child[marshall->size]->child = (marshall_t **)tree_zcalloc(2, sizeof(marshall_t *), marshall);
			marshall->child[marshall->size]->type = MTYPE_OBJECT;
			marshall->child[marshall->size]->name = tree_zstrdup(index_squid, marshall);
			marshall->child[marshall->size]->name_len = QUID_LENGTH;
			marshall->child[marshall->size]->size = 2;

			/* Indexed group */
			marshall->child[marshall->size]->child[0] = tree_zcalloc(1, sizeof(marshall_t), marshall);
			marshall->child[marshall->size]->child[0]->type = MTYPE_QUID;
			marshall->child[marshall->size]->child[0]->name = tree_zstrdup("group", marshall);
			marshall->child[marshall->size]->child[0]->name_len = QUID_LENGTH;
			marshall->child[marshall->size]->child[0]->data = tree_zstrdup(group_squid, marshall);
			marshall->child[marshall->size]->child[0]->data_len = 5;

			/* Indexed element */
			marshall->child[marshall->size]->child[1] = tree_zcalloc(1, sizeof(marshall_t), marshall);
			marshall->child[marshall->size]->child[1]->type = MTYPE_STRING;
			marshall->child[marshall->size]->child[1]->name = tree_zstrdup("element", marshall);
			marshall->child[marshall->size]->child[1]->name_len = 7;
			marshall->child[marshall->size]->child[1]->data = tree_zstrdup(indexlist->items[i].element, marshall);
			marshall->child[marshall->size]->child[1]->data_len = strlen(indexlist->items[i].element);
			marshall->size++;
		}

		if (indexlist->link) {
			offset = from_be64(indexlist->link);
		} else
			offset = 0;

		zfree(indexlist);
	}

	return marshall;
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
		case MD_TYPE_GROUP:
			strlcpy(buf, "GROUP", STATUS_TYPE_SIZE);
			break;
		case MD_TYPE_RAW:
			strlcpy(buf, "RAW", STATUS_TYPE_SIZE);
			break;
		case MD_TYPE_INDEX:
			strlcpy(buf, "INDEX", STATUS_TYPE_SIZE);
			break;
		case MD_TYPE_RECORD:
		default:
			strlcpy(buf, "RECORD", STATUS_TYPE_SIZE);
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
	if (!strcmp(key_type, "GROUP"))
		return MD_TYPE_GROUP;
	else if (!strcmp(key_type, "RAW"))
		return MD_TYPE_RAW;
	else if (!strcmp(key_type, "INDEX"))
		return MD_TYPE_INDEX;
	else
		return MD_TYPE_RECORD;
}
