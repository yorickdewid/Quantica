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

#define TABLE_SIZE	((4096 - 1) / sizeof(struct _engine_item))

static int delete_larger = 0;
static uint64_t last_blob = 0;

struct _engine_item {
	quid_t quid;
	struct metadata meta;
	__be64 offset;
	__be64 child;
} __attribute__((packed));

struct _engine_table {
	struct _engine_item items[TABLE_SIZE];
	uint16_t size;
} __attribute__((packed));

struct _blob_info {
	__be32 len;
	__be64 next;
	bool free;
} __attribute__((packed));

struct _engine_super {
	__be32 version;
	__be64 top;
	__be64 free_top;
	__be64 nkey; //DEPRECATED almost
	__be64 nfree_table;
	__be64 crc_zero_key;
	__be64 list_top; //DEPRECATED
	__be64 list_size; //DEPRECATED
	__be64 index_list_top; //DEPRECATED
	__be64 index_list_size; //DEPRECATED
	char instance[INSTANCE_LENGTH];
} __attribute__((packed));

struct _engine_dbsuper {
	__be32 version;
	__be64 last;
} __attribute__((packed));

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

static void flush_super(engine_t *engine, bool fast);
static void flush_dbsuper(engine_t *engine);
static uint64_t remove_table(engine_t *engine, struct _engine_table *table, size_t i, quid_t *quid);

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

static struct _engine_table *get_table(engine_t *engine, uint64_t offset) {
	zassert(offset != 0);

	/* take from cache */
	struct engine_cache *slot = &engine->cache[offset % CACHE_SLOTS];
	if (slot->offset == offset) {
		slot->offset = 0;
		return slot->table;
	}

	struct _engine_table *table = (struct _engine_table *)zcalloc(1, sizeof(struct _engine_table));
	if (!table) {
		zfree(table);
		error_throw_fatal("7b8a6ac440e2", "Failed to request memory");
		return NULL;
	}

	if (lseek(engine->fd, offset, SEEK_SET) < 0) {
		zfree(table);
		error_throw_fatal("a7df40ba3075", "Failed to read disk");
		return NULL;
	}
	if (read(engine->fd, table, sizeof(struct _engine_table)) != sizeof(struct _engine_table)) {
		zfree(table);
		error_throw_fatal("a7df40ba3075", "Failed to read disk");
		return NULL;
	}
	return table;
}

/* Free a table or put it into the cache */
static void put_table(engine_t *engine, struct _engine_table *table, uint64_t offset) {
	zassert(offset != 0);

	/* overwrite cache */
	struct engine_cache *slot = &engine->cache[offset % CACHE_SLOTS];
	if (slot->offset != 0) {
		zfree(slot->table);
	}
	slot->offset = offset;
	slot->table = table;
}

/* Write a table and free it */
static void flush_table(engine_t *engine, struct _engine_table *table, uint64_t offset) {
	zassert(offset != 0);

	if (lseek(engine->fd, offset, SEEK_SET) < 0) {
		error_throw_fatal("1fd531fa70c1", "Failed to write disk");
		return;
	}
	if (write(engine->fd, table, sizeof(struct _engine_table)) != sizeof(struct _engine_table)) {
		error_throw_fatal("1fd531fa70c1", "Failed to write disk");
		return;
	}
	put_table(engine, table, offset);
}

static int engine_open(engine_t *engine, const char *idxname, const char *dbname) {
	memset(engine, 0, sizeof(engine_t));
	engine->fd = open(idxname, O_RDWR | O_BINARY);
	engine->db_fd = open(dbname, O_RDWR | O_BINARY);
	if (engine->fd < 0 || engine->db_fd < 0)
		return -1;

	struct _engine_super super;
	if (read(engine->fd, &super, sizeof(struct _engine_super)) != sizeof(struct _engine_super)) {
		error_throw_fatal("a7df40ba3075", "Failed to read disk");
		return -1;
	}
	engine->top = from_be64(super.top);
	engine->free_top = from_be64(super.free_top);
	engine->stats.keys = from_be64(super.nkey);
	engine->stats.free_tables = from_be64(super.nfree_table);
	engine->stats.list_size = from_be64(super.list_size);
	engine->stats.index_list_size = from_be64(super.index_list_size);
	engine->list_top = from_be64(super.list_top);
	engine->index_list_top = from_be64(super.index_list_top);
	zassert(from_be64(super.version) == VERSION_MAJOR);

	uint64_t crc64sum;
	if (!crc_file(engine->fd, &crc64sum)) {
		lprint("[erro] Failed to calculate CRC\n");
		return -1;
	}
	/* TODO: run the diagnose process */
	if (from_be64(super.crc_zero_key) != crc64sum)
		lprint("[erro] Index CRC doenst match\n");

	struct _engine_dbsuper dbsuper;
	if (read(engine->db_fd, &dbsuper, sizeof(struct _engine_dbsuper)) != sizeof(struct _engine_dbsuper)) {
		error_throw_fatal("a7df40ba3075", "Failed to read disk");
		return -1;
	}
	zassert(from_be64(dbsuper.version) == VERSION_MAJOR);
	last_blob = from_be64(dbsuper.last);

	long _alloc = lseek(engine->fd, 0, SEEK_END);
	long _db_alloc = lseek(engine->db_fd, 0, SEEK_END);

	if (_alloc < 0 || _db_alloc < 0) {
		error_throw_fatal("a7df40ba3075", "Failed to read disk");
		return -1;
	}

	engine->alloc = _alloc;
	engine->db_alloc = _db_alloc;
	return 0;
}

static int engine_create(engine_t *engine, const char *idxname, const char *dbname) {
	memset(engine, 0, sizeof(engine_t));
	engine->fd = open(idxname, O_RDWR | O_TRUNC | O_CREAT | O_BINARY, 0644);
	engine->db_fd = open(dbname, O_RDWR | O_TRUNC | O_CREAT | O_BINARY, 0644);
	if (engine->fd < 0 || engine->db_fd < 0)
		return -1;

	last_blob = 0;
	flush_super(engine, TRUE);
	flush_dbsuper(engine);

	engine->alloc = sizeof(struct _engine_super);
	engine->db_alloc = sizeof(struct _engine_dbsuper);
	return 0;
}

void engine_init(engine_t *engine, const char *fname, const char *dbname) {
	if (file_exists(fname) && file_exists(dbname)) {
		engine_open(engine, fname, dbname);
	} else {
		engine_create(engine, fname, dbname);
	}
}

void engine_close(engine_t *engine) {
	engine_sync(engine);
	close(engine->fd);
	close(engine->db_fd);

	for (size_t i = 0; i < CACHE_SLOTS; ++i) {
		if (engine->cache[i].offset) {
			zfree(engine->cache[i].table);
		}
	}
}

void engine_sync(engine_t *engine) {
	flush_super(engine, FALSE);
	flush_dbsuper(engine);
}

/* Return a value that is greater or equal to 'val' and is power-of-two. */
static size_t page_align(size_t val) {
	size_t i = 1;
	while (i < val)
		i <<= 1;
	return i;
}

static uint64_t page_swap(engine_t *engine, size_t len) {
	len = page_align(len);
	uint64_t offset = engine->alloc;

	/* this is important to performance */
	if (offset & (len - 1)) {
		offset += len - (offset & (len - 1));
	}
	engine->alloc = offset + len;
	return offset;
}

/* Allocate a chunk from the index file for new table */
static uint64_t alloc_table_chunk(engine_t *engine, size_t len) {
	zassert(len > 0);

	/* Use blocks from freelist instead of allocation */
	if (engine->free_top) {
		uint64_t offset = engine->free_top;
		struct _engine_table *table = get_table(engine, offset);
		engine->free_top = from_be64(table->items[0].child);
		engine->stats.free_tables--;

		zfree(table);
		return offset;
	}

	return page_swap(engine, len);
}

/* Allocate a chunk from the database file */
static uint64_t alloc_dbchunk(engine_t *engine, size_t len) {
	zassert(len > 0);

	if (!engine->dbcache[DBCACHE_SLOTS - 1].len)
		goto new_block;

	for (int i = 0; i < DBCACHE_SLOTS; ++i) {
		struct engine_dbcache *slot = &engine->dbcache[i];
		if (len <= slot->len) {
			int diff = (((double)(len) / (double)slot->len) * 100);
			if (diff >= DBCACHE_DENSITY) {
				slot->len = 0;
				return slot->offset;
			}
		}
	}

new_block:
	len = page_align(sizeof(struct _blob_info) + len);

	uint64_t offset = engine->db_alloc;
	engine->db_alloc = offset + len;
	return offset;
}

/* Mark a chunk as unused in the database file */
static void free_index_chunk(engine_t *engine, uint64_t offset) {
	zassert(offset > 0);
	struct _engine_table *table = get_table(engine, offset);

	quid_t quid;
	memset(&quid, 0, sizeof(quid_t));

	memcpy(&table->items[0].quid, &quid, sizeof(quid_t));
	table->size++;
	table->items[0].offset = 0;
	table->items[0].child = to_be64(engine->free_top);

	flush_table(engine, table, offset);
	engine->free_top = offset;
	engine->stats.free_tables++;
}

static void free_dbchunk(engine_t *engine, uint64_t offset) {
	struct _blob_info info;

	if (lseek(engine->db_fd, offset, SEEK_SET) < 0) {
		error_throw_fatal("a7df40ba3075", "Failed to read disk");
		return;
	}
	if (read(engine->db_fd, &info, sizeof(struct _blob_info)) != sizeof(struct _blob_info)) {
		error_throw_fatal("a7df40ba3075", "Failed to read disk");
		return;
	}

	info.free = 1;
	info.next = 0;
	struct engine_dbcache dbinfo;
	size_t len = from_be32(info.len);
	dbinfo.offset = offset;
	for (int i = DBCACHE_SLOTS - 1; i >= 0; --i) {
		struct engine_dbcache *slot = &engine->dbcache[i];
		if (len > slot->len) {
			if (slot->len) {
				for (int j = 0; j < i; ++j) {
					engine->dbcache[j] = engine->dbcache[j + 1];
				}
			}
			dbinfo.len = len;
			engine->dbcache[i] = dbinfo;
			break;
		}
	}

	if (lseek(engine->db_fd, offset, SEEK_SET) < 0) {
		error_throw_fatal("1fd531fa70c1", "Failed to write disk");
		return;
	}
	if (write(engine->db_fd, &info, sizeof(struct _blob_info)) != sizeof(struct _blob_info)) {
		error_throw_fatal("1fd531fa70c1", "Failed to write disk");
		return;
	}
}

static void flush_super(engine_t *engine, bool fast_flush) {
	uint64_t crc64sum;
	struct _engine_super super;
	memset(&super, 0, sizeof(struct _engine_super));

	super.version = to_be64(VERSION_MAJOR);
	super.top = to_be64(engine->top);
	super.free_top = to_be64(engine->free_top);
	super.nkey = to_be64(engine->stats.keys);
	super.nfree_table = to_be64(engine->stats.free_tables);
	super.list_size = to_be64(engine->stats.list_size);
	super.index_list_size = to_be64(engine->stats.index_list_size);
	super.list_top = to_be64(engine->list_top);
	super.index_list_top = to_be64(engine->index_list_top);
	if (fast_flush)
		goto flush_disk;

	if (lseek(engine->fd, sizeof(struct _engine_super), SEEK_SET) < 0) {
		lprint("[erro] Failed to calculate CRC\n");
		return;
	}
	if (!crc_file(engine->fd, &crc64sum)) {
		lprint("[erro] Failed to calculate CRC\n");
		return;
	}
	super.crc_zero_key = to_be64(crc64sum);

flush_disk:
	if (lseek(engine->fd, 0, SEEK_SET) < 0) {
		error_throw_fatal("1fd531fa70c1", "Failed to write disk");
		return;
	}
	if (write(engine->fd, &super, sizeof(struct _engine_super)) != sizeof(struct _engine_super)) {
		error_throw_fatal("1fd531fa70c1", "Failed to write disk");
		return;
	}
}

static void flush_dbsuper(engine_t *engine) {
	struct _engine_dbsuper dbsuper;
	memset(&dbsuper, 0, sizeof(struct _engine_dbsuper));
	dbsuper.version = to_be64(VERSION_MAJOR);
	dbsuper.last = to_be64(last_blob);

	if (lseek(engine->db_fd, 0, SEEK_SET) < 0) {
		error_throw_fatal("1fd531fa70c1", "Failed to write disk");
		return;
	}
	if (write(engine->db_fd, &dbsuper, sizeof(struct _engine_dbsuper)) != sizeof(struct _engine_dbsuper)) {
		error_throw_fatal("1fd531fa70c1", "Failed to write disk");
		return;
	}
}

static uint64_t insert_data(engine_t *engine, const void *data, size_t len) {
	if (!data || len == 0) {
		error_throw("e8880046e019", "No data provided");
		return len;
	}

	struct _blob_info info;
	memset(&info, 0, sizeof(struct _blob_info));
	info.len = to_be32(len);
	info.free = 0;

	uint64_t offset = alloc_dbchunk(engine, len);
	info.next = to_be64(last_blob);
	last_blob = offset;

	if (lseek(engine->db_fd, offset, SEEK_SET) < 0) {
		error_throw_fatal("1fd531fa70c1", "Failed to write disk");
		return 0;
	}
	if (write(engine->db_fd, &info, sizeof(struct _blob_info)) != sizeof(struct _blob_info)) {
		error_throw_fatal("1fd531fa70c1", "Failed to write disk");
		return 0;
	}
	if (write(engine->db_fd, data, len) != (ssize_t)len) {
		error_throw_fatal("1fd531fa70c1", "Failed to write disk");
		return 0;
	}

	return offset;
}

/* Split a table. The pivot item is stored to 'quid' and 'offset'.
   Returns offset to the new table. */
static uint64_t split_table(engine_t *engine, struct _engine_table *table, quid_t *quid, uint64_t *offset) {
	memcpy(quid, &table->items[TABLE_SIZE / 2].quid, sizeof(quid_t));
	*offset = from_be64(table->items[TABLE_SIZE / 2].offset);

	struct _engine_table *new_table = (struct _engine_table *)zcalloc(1, sizeof(struct _engine_table));
	if (!new_table) {
		zfree(new_table);
		error_throw_fatal("7b8a6ac440e2", "Failed to request memory");
		return 0;
	}
	new_table->size = table->size - TABLE_SIZE / 2 - 1;

	table->size = TABLE_SIZE / 2;
	memcpy(new_table->items, &table->items[TABLE_SIZE / 2 + 1], (new_table->size + 1) * sizeof(struct _engine_item));

	uint64_t new_table_offset = alloc_table_chunk(engine, sizeof(struct _engine_table));
	flush_table(engine, new_table, new_table_offset);

	return new_table_offset;
}

/* Try to table_rejoin the given table. Returns a new table offset. */
static uint64_t table_join(engine_t *engine, uint64_t table_offset) {
	struct _engine_table *table = get_table(engine, table_offset);
	if (table->size == 0) {
		uint64_t ret = from_be64(table->items[0].child);
		free_index_chunk(engine, table_offset);

		zfree(table);
		return ret;
	}
	put_table(engine, table, table_offset);
	return table_offset;
}

/* Find and remove the smallest item from the given table. The key of the item
   is stored to 'quid'. Returns offset to the item */
static uint64_t take_smallest(engine_t *engine, uint64_t table_offset, quid_t *quid) {
	struct _engine_table *table = get_table(engine, table_offset);
	zassert(table->size > 0);

	uint64_t offset = 0;
	uint64_t child = from_be64(table->items[0].child);
	if (child == 0) {
		offset = remove_table(engine, table, 0, quid);
	} else {
		/* recursion */
		offset = take_smallest(engine, child, quid);
		table->items[0].child = to_be64(table_join(engine, child));
	}
	flush_table(engine, table, table_offset);
	return offset;
}

/* Find and remove the largest item from the given table. The key of the item
   is stored to 'quid'. Returns offset to the item */
static uint64_t take_largest(engine_t *engine, uint64_t table_offset, quid_t *quid) {
	struct _engine_table *table = get_table(engine, table_offset);
	zassert(table->size > 0);

	uint64_t offset = 0;
	uint64_t child = from_be64(table->items[table->size].child);
	if (child == 0) {
		offset = remove_table(engine, table, table->size - 1, quid);
	} else {
		/* recursion */
		offset = take_largest(engine, child, quid);
		table->items[table->size].child = to_be64(table_join(engine, child));
	}
	flush_table(engine, table, table_offset);
	return offset;
}

/* Remove an item in position 'i' from the given table. The key of the
   removed item is stored to 'quid'. Returns offset to the item. */
static uint64_t remove_table(engine_t *engine, struct _engine_table *table, size_t i, quid_t *quid) {
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
			new_offset = take_largest(engine, left_child, &table->items[i].quid);
			table->items[i].child = to_be64(table_join(engine, left_child));
		} else {
			new_offset = take_smallest(engine, right_child, &table->items[i].quid);
			table->items[i + 1].child = to_be64(table_join(engine, right_child));
		}
		table->items[i].offset = to_be64(new_offset);
	} else {
		memmove(&table->items[i], &table->items[i + 1], (table->size - i) * sizeof(struct _engine_item));
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
static uint64_t insert_table(engine_t *engine, uint64_t table_offset, quid_t *quid, struct metadata *meta, const void *data, size_t len) {
	struct _engine_table *table = get_table(engine, table_offset);
	zassert(table->size < TABLE_SIZE - 1);

	size_t left = 0, right = table->size;
	while (left < right) {
		size_t i = (right - left) / 2 + left;
		int cmp = quidcmp(quid, &table->items[i].quid);
		if (cmp == 0) {
			/* already in the table */
			uint64_t ret = from_be64(table->items[i].offset);
			put_table(engine, table, table_offset);
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
		ret = insert_table(engine, left_child, quid, meta, data, len);

		/* check if we need to split */
		struct _engine_table *child = get_table(engine, left_child);
		if (child->size < TABLE_SIZE - 1) {
			/* nothing to do */
			put_table(engine, table, table_offset);
			put_table(engine, child, left_child);
			return ret;
		}
		/* overwrites QUID */
		right_child = split_table(engine, child, quid, &offset);
		/* flush just in case changes happened */
		flush_table(engine, child, left_child);
	} else {
		if (data && len > 0) {
			ret = offset = insert_data(engine, data, len);
		}
	}

	table->size++;
	memmove(&table->items[i + 1], &table->items[i], (table->size - i) * sizeof(struct _engine_item));
	memcpy(&table->items[i].quid, quid, sizeof(quid_t));
	table->items[i].offset = to_be64(offset);
	memcpy(&table->items[i].meta, meta, sizeof(struct metadata));
	table->items[i].child = to_be64(left_child);
	table->items[i + 1].child = to_be64(right_child);

	flush_table(engine, table, table_offset);
	return ret;
}

/*
 * Remove a item with key 'quid' from the given table. The offset to the
 * removed item is returned.
 * Please note that 'quid' is overwritten when called inside the allocator.
 */
static uint64_t delete_table(engine_t *engine, uint64_t table_offset, quid_t *quid) {
	if (!table_offset) {
		error_throw("6ef42da7901f", "Record not found");
		return 0;
	}
	struct _engine_table *table = get_table(engine, table_offset);

	size_t left = 0, right = table->size;
	while (left < right) {
		size_t i = (right - left) / 2 + left;
		int cmp = quidcmp(quid, &table->items[i].quid);
		if (cmp == 0) {
			/* found */
			if (table->items[i].meta.syslock || table->items[i].meta.freeze) {
				error_throw("4987a3310049", "Record locked");
				put_table(engine, table, table_offset);
				return 0;
			}
			uint64_t ret = remove_table(engine, table, i, quid);
			flush_table(engine, table, table_offset);
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
	uint64_t ret = delete_table(engine, child, quid);
	if (ret != 0)
		table->items[i].child = to_be64(table_join(engine, child));

	if (ret == 0 && delete_larger && i < table->size) {
		/* remove the next largest */
		ret = remove_table(engine, table, i, quid);
	}
	if (ret != 0) {
		/* flush just in case changes happened */
		flush_table(engine, table, table_offset);
	} else {
		put_table(engine, table, table_offset);
	}
	return ret;
}

static uint64_t insert_toplevel(engine_t *engine, uint64_t *table_offset, quid_t *quid, struct metadata *meta, const void *data, size_t len) {
	uint64_t offset = 0;
	uint64_t ret = 0;
	uint64_t right_child = 0;
	if (*table_offset != 0) {
		ret = insert_table(engine, *table_offset, quid, meta, data, len);

		/* check if we need to split */
		struct _engine_table *table = get_table(engine, *table_offset);
		if (table->size < TABLE_SIZE - 1) {
			/* nothing to do */
			put_table(engine, table, *table_offset);
			return ret;
		}
		right_child = split_table(engine, table, quid, &offset);
		flush_table(engine, table, *table_offset);
	} else {
		if (data && len > 0) {
			ret = offset = insert_data(engine, data, len);
		}
	}

	/* create new top level table */
	struct _engine_table *new_table = (struct _engine_table *)zcalloc(1, sizeof(struct _engine_table));
	if (!new_table) {
		zfree(new_table);
		error_throw_fatal("7b8a6ac440e2", "Failed to request memory");
		return 0;
	}
	new_table->size = 1;
	memcpy(&new_table->items[0].quid, quid, sizeof(quid_t));
	new_table->items[0].offset = to_be64(offset);
	memcpy(&new_table->items[0].meta, meta, sizeof(struct metadata));
	new_table->items[0].child = to_be64(*table_offset);
	new_table->items[1].child = to_be64(right_child);

	uint64_t new_table_offset = alloc_table_chunk(engine, sizeof(struct _engine_table));
	flush_table(engine, new_table, new_table_offset);

	*table_offset = new_table_offset;
	return ret;
}

int engine_insert_data(engine_t *engine, quid_t *quid, const void *data, size_t len) {
	if (engine->lock == LOCK) {
		error_throw("986154f80058", "Database locked");
		return -1;
	}

	struct metadata meta;
	memset(&meta, 0, sizeof(struct metadata));
	meta.importance = MD_IMPORTANT_NORMAL;

	insert_toplevel(engine, &engine->top, quid, &meta, data, len);
	flush_super(engine, TRUE);
	if (iserror())
		return -1;

	engine->stats.keys++;
	return 0;
}

int engine_insert_meta_data(engine_t *engine, quid_t *quid, struct metadata *meta, const void *data, size_t len) {
	if (engine->lock == LOCK) {
		error_throw("986154f80058", "Database locked");
		return -1;
	}

	insert_toplevel(engine, &engine->top, quid, meta, data, len);
	flush_super(engine, TRUE);
	if (iserror())
		return -1;

	engine->stats.keys++;
	return 0;
}

int engine_insert(engine_t *engine, quid_t *quid) {
	if (engine->lock == LOCK) {
		error_throw("986154f80058", "Database locked");
		return -1;
	}

	struct metadata meta;
	memset(&meta, 0, sizeof(struct metadata));
	meta.nodata = TRUE;
	meta.importance = MD_IMPORTANT_NORMAL;

	insert_toplevel(engine, &engine->top, quid, &meta, NULL, 0);
	flush_super(engine, TRUE);
	if (iserror())
		return -1;

	engine->stats.keys++;
	return 0;
}

int engine_insert_meta(engine_t *engine, quid_t *quid, struct metadata *meta) {
	if (engine->lock == LOCK) {
		error_throw("986154f80058", "Database locked");
		return -1;
	}

	insert_toplevel(engine, &engine->top, quid, meta, NULL, 0);
	flush_super(engine, TRUE);
	if (iserror())
		return -1;

	engine->stats.keys++;
	return 0;
}

/*
 * Look up item with the given key 'quid' in the given table. Returns offset
 * to the item.
 */
static uint64_t lookup_key(engine_t *engine, uint64_t table_offset, const quid_t *quid, bool *nodata, struct metadata *meta) {
	while (table_offset) {
		struct _engine_table *table = get_table(engine, table_offset);
		size_t left = 0, right = table->size;
		while (left < right) {
			size_t i;
			i = (right - left) / 2 + left;
			int cmp = quidcmp(quid, &table->items[i].quid);
			if (cmp == 0) {
				/* found */
				if (table->items[i].meta.lifecycle != MD_LIFECYCLE_FINITE) {
					error_throw("6ef42da7901f", "Record not found");
					put_table(engine, table, table_offset);
					return 0;
				}
				uint64_t ret = from_be64(table->items[i].offset);
				*nodata = table->items[i].meta.nodata;
				memcpy(meta, &table->items[i].meta, sizeof(struct metadata));
				put_table(engine, table, table_offset);
				return ret;
			}
			if (cmp < 0) {
				right = i;
			} else {
				left = i + 1;
			}
		}
		uint64_t child = from_be64(table->items[left].child);
		put_table(engine, table, table_offset);
		table_offset = child;
	}
	error_throw("6ef42da7901f", "Record not found");
	return 0;
}

static void *get_data(engine_t *engine, uint64_t offset, size_t *len) {
	struct _blob_info info;

	if (lseek(engine->db_fd, offset, SEEK_SET) < 0) {
		error_throw_fatal("a7df40ba3075", "Failed to read disk");
		return NULL;
	}
	if (read(engine->db_fd, &info, sizeof(struct _blob_info)) != (ssize_t)sizeof(struct _blob_info)) {
		error_throw_fatal("a7df40ba3075", "Failed to read disk");
		return NULL;
	}

	*len = from_be32(info.len);
	if (!*len)
		return NULL;

	void *data = zcalloc(*len, sizeof(char));
	if (!data) {
		error_throw_fatal("7b8a6ac440e2", "Failed to request memory");
		return NULL;
	}

	if (read(engine->db_fd, data, *len) != (ssize_t) *len) {
		error_throw_fatal("a7df40ba3075", "Failed to read disk");
		zfree(data);
		data = NULL;
		return NULL;
	}

	return data;
}

void *get_data_block(engine_t *engine, uint64_t offset, size_t *len) {
	if (engine->lock == LOCK) {
		error_throw("986154f80058", "Database locked");
		return NULL;
	}

	if (!offset)
		return NULL;

	return get_data(engine, offset, len);
}

uint64_t engine_get(engine_t *engine, const quid_t *quid, struct metadata *meta) {
	if (engine->lock == LOCK) {
		error_throw("986154f80058", "Database locked");
		return 0;
	}
	bool nodata = 0;
	memset(meta, 0, sizeof(struct metadata));
	uint64_t offset = lookup_key(engine, engine->top, quid, &nodata, meta);
	if (iserror())
		return 0;

	if (nodata)
		return 0;

	return offset;
}

int engine_purge(engine_t *engine, quid_t *quid) {
	if (engine->lock == LOCK) {
		error_throw("986154f80058", "Database locked");
		return -1;
	}

	uint64_t offset = delete_table(engine, engine->top, quid);
	if (iserror())
		return -1;

	engine->top = table_join(engine, engine->top);
	engine->stats.keys--;

	free_dbchunk(engine, offset);
	flush_super(engine, TRUE);
	return 0;
}

static int set_meta(engine_t *engine, uint64_t table_offset, const quid_t *quid, const struct metadata *md) {
	while (table_offset) {
		struct _engine_table *table = get_table(engine, table_offset);
		size_t left = 0, right = table->size;
		while (left < right) {
			size_t i = (right - left) / 2 + left;
			int cmp = quidcmp(quid, &table->items[i].quid);
			if (cmp == 0) {
				if (table->items[i].meta.syslock) {
					error_throw("4987a3310049", "Record locked");
					put_table(engine, table, table_offset);
					return -1;
				}
				memcpy(&table->items[i].meta, md, sizeof(struct metadata));
				flush_table(engine, table, table_offset);
				return 0;
			}
			if (cmp < 0) {
				right = i;
			} else {
				left = i + 1;
			}
		}
		uint64_t child = from_be64(table->items[left].child);
		put_table(engine, table, table_offset);
		table_offset = child;
	}
	error_throw("6ef42da7901f", "Record not found");
	return -1;
}

int engine_setmeta(engine_t *engine, const quid_t *quid, const struct metadata *data) {
	if (engine->lock == LOCK) {
		error_throw("986154f80058", "Database locked");
		return -1;
	}

	set_meta(engine, engine->top, quid, data);
	if (iserror())
		return -1;
	return 0;
}

int engine_delete(engine_t *engine, const quid_t *quid) {
	if (engine->lock == LOCK) {
		error_throw("986154f80058", "Database locked");
		return -1;
	}

	bool nodata = 0;
	struct metadata meta;
	lookup_key(engine, engine->top, quid, &nodata, &meta);
	if (iserror())
		return -1;

	meta.lifecycle = MD_LIFECYCLE_RECYCLE;
	set_meta(engine, engine->top, quid, &meta);
	if (iserror())
		return -1;

	flush_super(engine, TRUE);
	return 0;
}

int engine_recover_storage(engine_t *engine) {
	uint64_t offset = last_blob;
	struct _blob_info info;
	int cnt = 0;

	if (engine->lock == LOCK) {
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
		if (lseek(engine->db_fd, offset, SEEK_SET) < 0) {
			error_throw_fatal("a7df40ba3075", "Failed to read disk");
			return -1;
		}
		if (read(engine->db_fd, &info, sizeof(struct _blob_info)) != (ssize_t) sizeof(struct _blob_info)) {
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
	lprintf("[info] Lost %d records\n", engine->stats.keys - cnt);
	return 0;
}

//TODO Copy over other keytypes, indexes, aliasses..
static void engine_copy(engine_t *engine, engine_t *new_engine, uint64_t table_offset) {
	struct _engine_table *table = get_table(engine, table_offset);
	size_t sz = table->size;
	for (int i = 0; i < (int)sz; ++i) {
		uint64_t child = from_be64(table->items[i].child);
		uint64_t right = from_be64(table->items[i + 1].child);
		uint64_t dboffset = from_be64(table->items[i].offset);

		size_t len;
		void *data = get_data(engine, dboffset, &len);

		/* Only copy active keys */
		if (table->items[i].meta.lifecycle == MD_LIFECYCLE_FINITE) {
			insert_toplevel(new_engine, &new_engine->top, &table->items[i].quid, &table->items[i].meta, data, len);
			new_engine->stats.keys++;
			flush_super(new_engine, TRUE);
		}

		zfree(data);
		if (child)
			engine_copy(engine, new_engine, child);
		if (right)
			engine_copy(engine, new_engine, right);
	}
	put_table(engine, table, table_offset);
}

int engine_vacuum(engine_t *engine, const char *fname, const char *dbname) {
	engine_t new_engine;
	engine_t tmp;

	if (engine->lock == LOCK) {
		error_throw("986154f80058", "Database locked");
		return -1;
	}

	if (!engine->stats.keys)
		return 0;

	lprint("[info] Start vacuum process\n");
	engine->lock = LOCK;
	engine_create(&new_engine, fname, dbname);
	engine_copy(engine, &new_engine, engine->top);

	memcpy(&tmp, engine, sizeof(engine_t));
	memcpy(engine, &new_engine, sizeof(engine_t));
	engine_close(&tmp);

	return 0;
}

int engine_update_data(engine_t *engine, const quid_t *quid, const void *data, size_t len) {
	if (engine->lock == LOCK) {
		error_throw("986154f80058", "Database locked");
		return -1;
	}

	uint64_t offset = 0;
	uint64_t table_offset = engine->top;
	while (table_offset) {
		struct _engine_table *table = get_table(engine, table_offset);
		size_t left = 0, right = table->size;
		while (left < right) {
			size_t i = (right - left) / 2 + left;
			int cmp = quidcmp(quid, &table->items[i].quid);
			if (cmp == 0) {
				if (table->items[i].meta.syslock) {
					error_throw("4987a3310049", "Record locked");
					put_table(engine, table, table_offset);
					return -1;
				}
				offset = from_be64(table->items[i].offset);
				free_dbchunk(engine, offset);
				offset = insert_data(engine, data, len);
				table->items[i].offset = to_be64(offset);
				flush_table(engine, table, table_offset);
				flush_super(engine, TRUE);
				return 0;
			}
			if (cmp < 0) {
				right = i;
			} else {
				left = i + 1;
			}
		}
		uint64_t child = from_be64(table->items[left].child);
		put_table(engine, table, table_offset);
		table_offset = child;
	}
	error_throw("6ef42da7901f", "Record not found");
	return -1;
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
