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
#include "pager.h"
#include "history.h"
#include "core.h"
#include "engine.h"

#define TABLE_SIZE			128
#define TABLE_DELETE_LARGE	1

struct _engine_item {
	quid_t quid;
	struct metadata meta;
	__be64 offset;
	__be64 child;
} __attribute__((packed));

struct _engine_table {
	struct _engine_item items[TABLE_SIZE];
	__be16 size;
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

static void flush_super(base_t *base);
static void flush_dbsuper(base_t *base);
static unsigned long long remove_table(base_t *base, struct _engine_table *table, size_t i, quid_t *quid);

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

static struct _engine_table *get_table(const base_t *base, uint64_t offset) {
	zassert(offset != 0);

	/* take from cache */
	struct engine_cache *slot = &base->engine->cache[offset % CACHE_SLOTS];
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

	int fd = pager_get_fd(base, &offset);
	if (lseek(fd, offset, SEEK_SET) < 0) {
		zfree(table);
		error_throw_fatal("a7df40ba3075", "Failed to read disk");
		return NULL;
	}
	if (read(fd, table, sizeof(struct _engine_table)) != sizeof(struct _engine_table)) {
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
static void flush_table(base_t *base, struct _engine_table *table, uint64_t offset) {
	zassert(offset != 0);

	int fd = pager_get_fd(base, &offset);
	if (lseek(fd, offset, SEEK_SET) < 0) {
		error_throw_fatal("1fd531fa70c1", "Failed to write disk");
		return;
	}
	if (write(fd, table, sizeof(struct _engine_table)) != sizeof(struct _engine_table)) {
		error_throw_fatal("1fd531fa70c1", "Failed to write disk");
		return;
	}
	put_table(base->engine, table, offset);
}

static int engine_open(base_t *base) {
	memset(base->engine, 0, sizeof(engine_t));
	struct _engine_super super;
	struct _engine_dbsuper dbsuper;

	uint64_t offset = base->offset.zero;
	int fd = pager_get_fd(base, &offset);
	if (lseek(fd, offset, SEEK_SET) < 0) {
		error_throw_fatal("1fd531fa70c1", "Failed to write disk");
		return -1;
	}
	if (read(fd, &super, sizeof(struct _engine_super)) != sizeof(struct _engine_super)) {
		error_throw_fatal("a7df40ba3075", "Failed to read disk");
		return -1;
	}

	base->engine->top = from_be64(super.top);
	base->engine->free_top = from_be64(super.free_top);
	zassert(from_be64(super.version) == VERSION_MAJOR);

	uint64_t offseth = base->offset.heap;
	int fdh = pager_get_fd(base, &offseth);
	if (lseek(fd, offseth, SEEK_SET) < 0) {
		error_throw_fatal("1fd531fa70c1", "Failed to write disk");
		return -1;
	}
	if (read(fdh, &dbsuper, sizeof(struct _engine_dbsuper)) != sizeof(struct _engine_dbsuper)) {
		error_throw_fatal("a7df40ba3075", "Failed to read disk");
		return -1;
	}

	zassert(from_be64(dbsuper.version) == VERSION_MAJOR);
	base->engine->last_block = from_be64(dbsuper.last);
	return 0;
}

static int engine_create(base_t *base) {
	memset(base->engine, 0, sizeof(engine_t));
	base->engine->last_block = 0;

	lprint("[info] Creating core index\n");
	flush_super(base);
	flush_dbsuper(base);

	base->offset.zero = zpalloc(base, sizeof(struct _engine_super));
	base->offset.heap = zpalloc(base, sizeof(struct _engine_dbsuper));
	return 0;
}

void engine_init(base_t *base) {
	if (base->offset.zero != 0) {
		engine_open(base);
	} else {
		engine_create(base);
	}
}

void engine_close(base_t *base) {
	flush_super(base);
	flush_dbsuper(base);

	for (size_t i = 0; i < CACHE_SLOTS; ++i) {
		if (base->engine->cache[i].offset) {
			zfree(base->engine->cache[i].table);
		}
	}
}

void engine_sync(base_t *base) {
	flush_super(base);
	flush_dbsuper(base);
}

/* Allocate a chunk from the index file for new table */
static unsigned long long alloc_table_chunk(base_t *base, size_t len) {
	zassert(len > 0);

	/* Use blocks from freelist instead of allocation */
	if (base->engine->free_top) {
		unsigned long long offset = base->engine->free_top;
		struct _engine_table *table = get_table(base, offset);
		base->engine->free_top = from_be64(table->items[0].child);
		base->stats.zero_free_size--;

		zfree(table);
		return offset;
	}

	return zpalloc(base, len);
}

/* Allocate a chunk from the database file */
static unsigned long long alloc_dbchunk(base_t *base, size_t len) {
	zassert(len > 0);

	if (base->engine->dbcache[DBCACHE_SLOTS - 1].len) {
		for (int i = 0; i < DBCACHE_SLOTS; ++i) {
			struct engine_dbcache *slot = &base->engine->dbcache[i];
			if (len <= slot->len) {
				int diff = (((double)(len) / (double)slot->len) * 100);
				if (diff >= DBCACHE_DENSITY) {
					slot->len = 0;
					base->stats.heap_free_size--;

					/* Datablock reused so remove from history */
					history_delete(base, slot->offset);
					return slot->offset;
				}
			}
		}
	}

	return zpalloc(base, sizeof(struct _blob_info) + len);
}

/* Mark a chunk as unused in the database file */
static void free_index_chunk(base_t *base, uint64_t offset) {
	zassert(offset > 0);
	struct _engine_table *table = get_table(base, offset);

	quid_t quid;
	memset(&quid, 0, sizeof(quid_t));

	memcpy(&table->items[0].quid, &quid, sizeof(quid_t));
	table->size = incr_be16(table->size);
	table->items[0].offset = 0;
	table->items[0].child = to_be64(base->engine->free_top);

	flush_table(base, table, offset);
	base->engine->free_top = offset;
	base->stats.zero_free_size++;
}

static void free_dbchunk(base_t *base, uint64_t offset) {
	struct _blob_info info;

	int fd = pager_get_fd(base, &offset);
	if (lseek(fd, offset, SEEK_SET) < 0) {
		error_throw_fatal("a7df40ba3075", "Failed to read disk");
		return;
	}
	if (read(fd, &info, sizeof(struct _blob_info)) != sizeof(struct _blob_info)) {
		error_throw_fatal("a7df40ba3075", "Failed to read disk");
		return;
	}

	info.free = 1;
	info.next = 0;
	struct engine_dbcache dbinfo;
	size_t len = from_be32(info.len);
	dbinfo.offset = offset;
	base->stats.heap_free_size++;
	for (int i = DBCACHE_SLOTS - 1; i >= 0; --i) {
		struct engine_dbcache *slot = &base->engine->dbcache[i];
		if (len > slot->len) {
			if (slot->len) {
				for (int j = 0; j < i; ++j) {
					base->engine->dbcache[j] = base->engine->dbcache[j + 1];
				}
			}
			dbinfo.len = len;
			base->engine->dbcache[i] = dbinfo;
			break;
		}
	}

	if (lseek(fd, offset, SEEK_SET) < 0) {
		error_throw_fatal("1fd531fa70c1", "Failed to write disk");
		return;
	}
	if (write(fd, &info, sizeof(struct _blob_info)) != sizeof(struct _blob_info)) {
		error_throw_fatal("1fd531fa70c1", "Failed to write disk");
		return;
	}
}

static void flush_super(base_t *base) {
	struct _engine_super super;
	memset(&super, 0, sizeof(struct _engine_super));
	uint64_t offset = base->offset.zero;
	int fd = pager_get_fd(base, &offset);

	super.version = to_be64(VERSION_MAJOR);
	super.top = to_be64(base->engine->top);
	super.free_top = to_be64(base->engine->free_top);

	if (lseek(fd, offset, SEEK_SET) < 0) {
		error_throw_fatal("1fd531fa70c1", "Failed to write disk");
		return;
	}
	if (write(fd, &super, sizeof(struct _engine_super)) != sizeof(struct _engine_super)) {
		error_throw_fatal("1fd531fa70c1", "Failed to write disk");
		return;
	}
}

static void flush_dbsuper(base_t *base) {
	struct _engine_dbsuper dbsuper;
	memset(&dbsuper, 0, sizeof(struct _engine_dbsuper));
	uint64_t offset = base->offset.heap;
	int fd = pager_get_fd(base, &offset);

	dbsuper.version = to_be64(VERSION_MAJOR);
	dbsuper.last = to_be64(base->engine->last_block);
	if (lseek(fd, offset, SEEK_SET) < 0) {
		error_throw_fatal("1fd531fa70c1", "Failed to write disk");
		return;
	}
	if (write(fd, &dbsuper, sizeof(struct _engine_dbsuper)) != sizeof(struct _engine_dbsuper)) {
		error_throw_fatal("1fd531fa70c1", "Failed to write disk");
		return;
	}
}

static unsigned long long insert_data(base_t *base, const void *data, size_t len) {
	if (!data || len == 0) {
		error_throw("e8880046e019", "No data provided");
		return len;
	}

	struct _blob_info info;
	memset(&info, 0, sizeof(struct _blob_info));
	info.len = to_be32(len);
	info.free = 0;

	uint64_t offset = alloc_dbchunk(base, len);
	info.next = to_be64(base->engine->last_block);
	base->engine->last_block = offset;

	int fd = pager_get_fd(base, &offset);
	if (lseek(fd, offset, SEEK_SET) < 0) {
		error_throw_fatal("1fd531fa70c1", "Failed to write disk");
		return 0;
	}
	if (write(fd, &info, sizeof(struct _blob_info)) != sizeof(struct _blob_info)) {
		error_throw_fatal("1fd531fa70c1", "Failed to write disk");
		return 0;
	}
	if (write(fd, data, len) != (ssize_t)len) {
		error_throw_fatal("1fd531fa70c1", "Failed to write disk");
		return 0;
	}

	return base->engine->last_block;
}

/* Split a table. The pivot item is stored to 'quid' and 'offset'.
   Returns offset to the new table. */
static unsigned long long split_table(base_t *base, struct _engine_table *table, quid_t *quid, unsigned long long *offset) {
	memcpy(quid, &table->items[TABLE_SIZE / 2].quid, sizeof(quid_t));
	*offset = from_be64(table->items[TABLE_SIZE / 2].offset);

	struct _engine_table *new_table = (struct _engine_table *)zcalloc(1, sizeof(struct _engine_table));
	if (!new_table) {
		zfree(new_table);
		error_throw_fatal("7b8a6ac440e2", "Failed to request memory");
		return 0;
	}
	new_table->size = to_be16(from_be16(table->size) - TABLE_SIZE / 2 - 1);

	table->size = to_be16(TABLE_SIZE / 2);
	memcpy(new_table->items, &table->items[TABLE_SIZE / 2 + 1], (from_be16(new_table->size) + 1) * sizeof(struct _engine_item));

	unsigned long long new_table_offset = alloc_table_chunk(base, sizeof(struct _engine_table));
	flush_table(base, new_table, new_table_offset);

	return new_table_offset;
}

/* Try to table_rejoin the given table. Returns a new table offset. */
static unsigned long long table_join(base_t *base, unsigned long long offset) {
	struct _engine_table *table = get_table(base, offset);
	if (from_be16(table->size) == 0) {
		unsigned long long ret = from_be64(table->items[0].child);
		free_index_chunk(base, offset);

		zfree(table);
		return ret;
	}
	put_table(base->engine, table, offset);
	return offset;
}

/* Find and remove the smallest item from the given table. The key of the item
   is stored to 'quid'. Returns offset to the item */
static unsigned long long take_smallest(base_t *base, unsigned long long table_offset, quid_t *quid) {
	struct _engine_table *table = get_table(base, table_offset);
	zassert(from_be16(table->size) > 0);

	unsigned long long offset = 0;
	unsigned long long child = from_be64(table->items[0].child);
	if (child == 0) {
		offset = remove_table(base, table, 0, quid);
	} else {
		/* recursion */
		offset = take_smallest(base, child, quid);
		table->items[0].child = to_be64(table_join(base, child));
	}
	flush_table(base, table, table_offset);
	return offset;
}

/* Find and remove the largest item from the given table. The key of the item
   is stored to 'quid'. Returns offset to the item */
static unsigned long long take_largest(base_t *base, unsigned long long table_offset, quid_t *quid) {
	struct _engine_table *table = get_table(base, table_offset);
	zassert(from_be16(table->size) > 0);

	unsigned long long offset = 0;
	unsigned long long child = from_be64(table->items[from_be16(table->size)].child);
	if (child == 0) {
		offset = remove_table(base, table, from_be16(table->size) - 1, quid);
	} else {
		/* recursion */
		offset = take_largest(base, child, quid);
		table->items[from_be16(table->size)].child = to_be64(table_join(base, child));
	}
	flush_table(base, table, table_offset);
	return offset;
}

/* Remove an item in position 'i' from the given table. The key of the
   removed item is stored to 'quid'. Returns offset to the item. */
static unsigned long long remove_table(base_t *base, struct _engine_table *table, size_t i, quid_t *quid) {
	zassert(i < from_be16(table->size));

	if (quid)
		memcpy(quid, &table->items[i].quid, sizeof(quid_t));

	unsigned long long offset = from_be64(table->items[i].offset);
	unsigned long long left_child = from_be64(table->items[i].child);
	unsigned long long right_child = from_be64(table->items[i + 1].child);

	if (left_child != 0 && right_child != 0) {
		/* replace the removed item by taking an item from one of the child tables */
		unsigned long long new_offset;
		if (arc4random() & 1) {
			new_offset = take_largest(base, left_child, &table->items[i].quid);
			table->items[i].child = to_be64(table_join(base, left_child));
		} else {
			new_offset = take_smallest(base, right_child, &table->items[i].quid);
			table->items[i + 1].child = to_be64(table_join(base, right_child));
		}
		table->items[i].offset = to_be64(new_offset);
	} else {
		memmove(&table->items[i], &table->items[i + 1], (from_be16(table->size) - i) * sizeof(struct _engine_item));
		table->size = decr_be16(table->size);

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
static unsigned long long insert_table(base_t *base, unsigned long long table_offset, quid_t *quid, struct metadata *meta, const void *data, size_t len) {
	struct _engine_table *table = get_table(base, table_offset);
	zassert(from_be16(table->size) < TABLE_SIZE - 1);

	size_t left = 0, right = from_be16(table->size);
	while (left < right) {
		size_t i = (right - left) / 2 + left;
		int cmp = quidcmp(quid, &table->items[i].quid);
		if (cmp == 0) {
			/* already in the table */
			unsigned long long ret = from_be64(table->items[i].offset);
			put_table(base->engine, table, table_offset);
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
	unsigned long long offset = 0;
	unsigned long long left_child = from_be64(table->items[i].child);
	unsigned long long right_child = 0; /* after insertion */
	unsigned long long ret = 0;
	if (left_child != 0) {
		/* recursion */
		ret = insert_table(base, left_child, quid, meta, data, len);

		/* check if we need to split */
		struct _engine_table *child = get_table(base, left_child);
		if (from_be16(child->size) < TABLE_SIZE - 1) {
			/* nothing to do */
			put_table(base->engine, table, table_offset);
			put_table(base->engine, child, left_child);
			return ret;
		}
		/* overwrites QUID */
		right_child = split_table(base, child, quid, &offset);
		/* flush just in case changes happened */
		flush_table(base, child, left_child);
	} else {
		if (data && len > 0) {
			ret = offset = insert_data(base, data, len);
		}
	}

	table->size = incr_be16(table->size);
	memmove(&table->items[i + 1], &table->items[i], (from_be16(table->size) - i) * sizeof(struct _engine_item));
	memcpy(&table->items[i].quid, quid, sizeof(quid_t));
	table->items[i].offset = to_be64(offset);
	memcpy(&table->items[i].meta, meta, sizeof(struct metadata));
	table->items[i].child = to_be64(left_child);
	table->items[i + 1].child = to_be64(right_child);

	flush_table(base, table, table_offset);
	return ret;
}

/*
 * Remove a item with key 'quid' from the given table. The offset to the
 * removed item is returned.
 * Please note that 'quid' is overwritten when called inside the allocator.
 */
static unsigned long long delete_table(base_t *base, unsigned long long table_offset, quid_t *quid) {
	if (!table_offset) {
		error_throw("6ef42da7901f", "Record not found");
		return 0;
	}
	struct _engine_table *table = get_table(base, table_offset);

	size_t left = 0, right = from_be16(table->size);
	while (left < right) {
		size_t i = (right - left) / 2 + left;
		int cmp = quidcmp(quid, &table->items[i].quid);
		if (cmp == 0) {
			/* found */
			if (table->items[i].meta.syslock || table->items[i].meta.freeze) {
				error_throw("4987a3310049", "Record locked");
				put_table(base->engine, table, table_offset);
				return 0;
			}
			unsigned long long ret = remove_table(base, table, i, quid);
			flush_table(base, table, table_offset);
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
	unsigned long long child = from_be64(table->items[i].child);
	unsigned long long ret = delete_table(base, child, quid);
	if (ret != 0)
		table->items[i].child = to_be64(table_join(base, child));

	if (ret == 0 && TABLE_DELETE_LARGE && i < from_be16(table->size)) {
		/* remove the next largest */
		ret = remove_table(base, table, i, quid);
	}
	if (ret != 0) {
		/* flush just in case changes happened */
		flush_table(base, table, table_offset);
	} else {
		put_table(base->engine, table, table_offset);
	}
	return ret;
}

static unsigned long long insert_toplevel(base_t *base, unsigned long long *table_offset, quid_t *quid, struct metadata *meta, const void *data, size_t len) {
	unsigned long long offset = 0;
	unsigned long long ret = 0;
	unsigned long long right_child = 0;
	if (*table_offset != 0) {
		ret = insert_table(base, *table_offset, quid, meta, data, len);

		/* check if we need to split */
		struct _engine_table *table = get_table(base, *table_offset);
		if (from_be16(table->size) < TABLE_SIZE - 1) {
			/* nothing to do */
			put_table(base->engine, table, *table_offset);
			return ret;
		}
		right_child = split_table(base, table, quid, &offset);
		flush_table(base, table, *table_offset);
	} else {
		if (data && len > 0) {
			ret = offset = insert_data(base, data, len);
		}
	}

	/* create new top level table */
	struct _engine_table *new_table = (struct _engine_table *)zcalloc(1, sizeof(struct _engine_table));
	if (!new_table) {
		zfree(new_table);
		error_throw_fatal("7b8a6ac440e2", "Failed to request memory");
		return 0;
	}
	new_table->size = to_be16(1);
	memcpy(&new_table->items[0].quid, quid, sizeof(quid_t));
	new_table->items[0].offset = to_be64(offset);
	memcpy(&new_table->items[0].meta, meta, sizeof(struct metadata));
	new_table->items[0].child = to_be64(*table_offset);
	new_table->items[1].child = to_be64(right_child);

	unsigned long long new_table_offset = alloc_table_chunk(base, sizeof(struct _engine_table));
	flush_table(base, new_table, new_table_offset);

	*table_offset = new_table_offset;
	return ret;
}

static bool islocked(base_t *base) {
	if (base->engine->lock == LOCK) {
		error_throw("986154f80058", "Database locked");
		return TRUE;
	}

	return FALSE;
}

int engine_insert_data(base_t *base, quid_t *quid, const void *data, size_t len) {
	if (islocked(base))
		return -1;

	struct metadata meta;
	memset(&meta, 0, sizeof(struct metadata));
	meta.importance = MD_IMPORTANT_NORMAL;

	insert_toplevel(base, &base->engine->top, quid, &meta, data, len);
	if (iserror())
		return -1;

	base->stats.zero_size++;
	flush_super(base);
	return 0;
}

int engine_insert_meta_data(base_t *base, quid_t *quid, struct metadata *meta, const void *data, size_t len) {
	if (islocked(base))
		return -1;

	insert_toplevel(base, &base->engine->top, quid, meta, data, len);
	if (iserror())
		return -1;

	base->stats.zero_size++;
	flush_super(base);
	return 0;
}

int engine_insert(base_t *base, quid_t *quid) {
	if (islocked(base))
		return -1;

	struct metadata meta;
	memset(&meta, 0, sizeof(struct metadata));
	meta.nodata = TRUE;
	meta.importance = MD_IMPORTANT_NORMAL;

	insert_toplevel(base, &base->engine->top, quid, &meta, NULL, 0);
	if (iserror())
		return -1;

	base->stats.zero_size++;
	flush_super(base);
	return 0;
}

int engine_insert_meta(base_t *base, quid_t *quid, struct metadata *meta) {
	if (islocked(base))
		return -1;

	insert_toplevel(base, &base->engine->top, quid, meta, NULL, 0);
	if (iserror())
		return -1;

	base->stats.zero_size++;
	flush_super(base);
	return 0;
}

/*
 * Look up item with the given key 'quid' in the given table. Returns offset
 * to the item.
 */
static unsigned long long lookup_key(base_t *base, unsigned long long table_offset, const quid_t *quid, bool *nodata, bool force, struct metadata *meta) {
	while (table_offset) {
		struct _engine_table *table = get_table(base, table_offset);
		size_t left = 0, right = from_be16(table->size);
		while (left < right) {
			size_t i;
			i = (right - left) / 2 + left;
			int cmp = quidcmp(quid, &table->items[i].quid);
			if (cmp == 0) {
				/* found */
				if (!force && table->items[i].meta.lifecycle != MD_LIFECYCLE_FINITE) {
					error_throw("6ef42da7901f", "Record not found");
					put_table(base->engine, table, table_offset);
					return 0;
				}
				unsigned long long ret = from_be64(table->items[i].offset);
				*nodata = table->items[i].meta.nodata;
				memcpy(meta, &table->items[i].meta, sizeof(struct metadata));
				put_table(base->engine, table, table_offset);
				return ret;
			}
			if (cmp < 0) {
				right = i;
			} else {
				left = i + 1;
			}
		}
		unsigned long long child = from_be64(table->items[left].child);
		put_table(base->engine, table, table_offset);
		table_offset = child;
	}
	error_throw("6ef42da7901f", "Record not found");
	return 0;
}

static void *get_data(base_t *base, uint64_t offset, size_t *len) {
	struct _blob_info info;

	int fd = pager_get_fd(base, &offset);
	if (lseek(fd, offset, SEEK_SET) < 0) {
		error_throw_fatal("a7df40ba3075", "Failed to read disk");
		return NULL;
	}
	if (read(fd, &info, sizeof(struct _blob_info)) != (ssize_t)sizeof(struct _blob_info)) {
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

	if (read(fd, data, *len) != (ssize_t) *len) {
		error_throw_fatal("a7df40ba3075", "Failed to read disk");
		zfree(data);
		data = NULL;
		return NULL;
	}

	return data;
}

void *get_data_block(base_t *base, unsigned long long offset, size_t *len) {
	if (islocked(base))
		return NULL;

	if (!offset)
		return NULL;

	return get_data(base, offset, len);
}

unsigned long long engine_get(base_t *base, const quid_t *quid, struct metadata *meta) {
	if (islocked(base))
		return 0;

	bool nodata = 0;
	memset(meta, 0, sizeof(struct metadata));
	unsigned long long offset = lookup_key(base, base->engine->top, quid, &nodata, FALSE, meta);
	if (iserror())
		return 0;

	if (nodata)
		return 0;

	return offset;
}

unsigned long long engine_get_force(base_t *base, const quid_t *quid, struct metadata *meta) {
	if (islocked(base))
		return 0;

	bool nodata = 0;
	memset(meta, 0, sizeof(struct metadata));
	unsigned long long offset = lookup_key(base, base->engine->top, quid, &nodata, TRUE, meta);
	if (iserror())
		return 0;

	if (nodata)
		return 0;

	return offset;
}

int engine_purge(base_t *base, quid_t *quid) {
	if (islocked(base))
		return -1;

	unsigned long long offset = delete_table(base, base->engine->top, quid);
	if (iserror())
		return -1;

	base->engine->top = table_join(base, base->engine->top);
	base->stats.zero_size--;

	free_dbchunk(base, offset);
	flush_super(base);
	return 0;
}

static int set_meta(base_t *base, unsigned long long table_offset, const quid_t *quid, const struct metadata *md) {
	while (table_offset) {
		struct _engine_table *table = get_table(base, table_offset);
		size_t left = 0, right = from_be16(table->size);
		while (left < right) {
			size_t i = (right - left) / 2 + left;
			int cmp = quidcmp(quid, &table->items[i].quid);
			if (cmp == 0) {
				if (table->items[i].meta.syslock) {
					error_throw("4987a3310049", "Record locked");
					put_table(base->engine, table, table_offset);
					return -1;
				}
				memcpy(&table->items[i].meta, md, sizeof(struct metadata));
				flush_table(base, table, table_offset);
				return 0;
			}
			if (cmp < 0) {
				right = i;
			} else {
				left = i + 1;
			}
		}
		unsigned long long child = from_be64(table->items[left].child);
		put_table(base->engine, table, table_offset);
		table_offset = child;
	}
	error_throw("6ef42da7901f", "Record not found");
	return -1;
}

int engine_setmeta(base_t *base, const quid_t *quid, const struct metadata *data) {
	if (islocked(base))
		return -1;

	set_meta(base, base->engine->top, quid, data);
	if (iserror())
		return -1;
	return 0;
}

int engine_delete(base_t *base, const quid_t *quid) {
	if (islocked(base))
		return -1;

	bool nodata = 0;
	struct metadata meta;
	lookup_key(base, base->engine->top, quid, &nodata, FALSE, &meta);
	if (iserror())
		return -1;

	meta.lifecycle = MD_LIFECYCLE_RECYCLE;
	set_meta(base, base->engine->top, quid, &meta);
	if (iserror())
		return -1;

	flush_super(base);
	return 0;
}

#ifdef DEBUG
void engine_traverse(const base_t *base, unsigned long long table_offset) {
	struct _engine_table *table = get_table(base, table_offset);
	size_t sz = from_be16(table->size);
	for (int i = 0; i < (int)sz; ++i) {
		unsigned long long child = from_be64(table->items[i].child);
		unsigned long long right = from_be64(table->items[i + 1].child);
		unsigned long long dboffset = from_be64(table->items[i].offset);

		printf("Location %d data offset: %llu\n", i, dboffset);

		if (child)
			engine_traverse(base, child);
		if (right)
			engine_traverse(base, right);
	}
	put_table(base->engine, table, table_offset);
}
#endif

static void engine_copy(base_t *base, base_t *new_base, unsigned long long table_offset) {
	struct _engine_table *table = get_table(base, table_offset);
	size_t sz = from_be16(table->size);
	for (int i = 0; i < (int)sz; ++i) {
		unsigned long long child = from_be64(table->items[i].child);
		unsigned long long right = from_be64(table->items[i + 1].child);
		unsigned long long dboffset = from_be64(table->items[i].offset);

		size_t len;
		void *data = get_data(base, dboffset, &len);

		/* Only copy active keys */
		if (table->items[i].meta.lifecycle == MD_LIFECYCLE_FINITE) {
			insert_toplevel(new_base, &new_base->engine->top, &table->items[i].quid, &table->items[i].meta, data, len);
			new_base->stats.zero_size++;
			flush_super(new_base);
			error_clear();
		}

		zfree(data);
		if (child)
			engine_copy(base, new_base, child);
		if (right)
			engine_copy(base, new_base, right);
	}
	put_table(base->engine, table, table_offset);
}

int engine_rebuild(base_t *base, base_t *new_base) {
	if (!base->offset.zero) {
		error_throw_fatal("5e6f0673908d", "Core not initialized");
		return -1;
	}

	if (new_base->offset.zero) {
		error_throw_fatal("5e6f0673908d", "Core not initialized");
		return -1;
	}

	engine_create(new_base);
	engine_copy(base, new_base, base->engine->top);

	return 0;
}

int engine_update_data(base_t *base, const quid_t *quid, const void *data, size_t len) {
	if (islocked(base))
		return -1;

	unsigned long long offset = 0;
	unsigned long long table_offset = base->engine->top;
	while (table_offset) {
		struct _engine_table *table = get_table(base, table_offset);
		size_t left = 0, right = from_be16(table->size);
		while (left < right) {
			size_t i = (right - left) / 2 + left;
			int cmp = quidcmp(quid, &table->items[i].quid);
			if (cmp == 0) {
				if (table->items[i].meta.syslock) {
					error_throw("4987a3310049", "Record locked");
					put_table(base->engine, table, table_offset);
					return -1;
				}
				offset = from_be64(table->items[i].offset);
				free_dbchunk(base, offset);
				offset = insert_data(base, data, len);
				table->items[i].offset = to_be64(offset);
				flush_table(base, table, table_offset);
				flush_super(base);
				return 0;
			}
			if (cmp < 0) {
				right = i;
			} else {
				left = i + 1;
			}
		}
		unsigned long long child = from_be64(table->items[left].child);
		put_table(base->engine, table, table_offset);
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
