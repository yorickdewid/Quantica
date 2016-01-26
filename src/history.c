#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <config.h>
#include <common.h>
#include <error.h>
#include "zmalloc.h"
#include "quid.h"
#include "pager.h"

#define HISTORY_LIST_SIZE	32

struct _history_list {
	struct {
		quid_t quid;
		__be16 version;
		__be64 offset;
	} items[HISTORY_LIST_SIZE];
	__be16 size;
	__be64 link;
} __attribute__((packed));

/* Read list structure from offset */
static struct _history_list *get_history_list(base_t *base, uint64_t offset) {
	int fd = pager_get_fd(base, &offset);

	struct _history_list *list = (struct _history_list *)zcalloc(1, sizeof(struct _history_list));
	if (!list) {
		zfree(list);
		error_throw_fatal("7b8a6ac440e2", "Failed to request memory");
		return NULL;
	}
	if (lseek(fd, offset, SEEK_SET) < 0) {
		zfree(list);
		error_throw_fatal("a7df40ba3075", "Failed to read disk");
		return NULL;
	}
	if (read(fd, list, sizeof(struct _history_list)) != sizeof(struct _history_list)) {
		zfree(list);
		error_throw_fatal("a7df40ba3075", "Failed to read disk");
		return NULL;
	}
	return list;
}

static void flush_history_list(base_t *base, struct _history_list *list, uint64_t offset) {
	int fd = pager_get_fd(base, &offset);

	if (lseek(fd, offset, SEEK_SET) < 0) {
		error_throw_fatal("1fd531fa70c1", "Failed to write disk");
		return;
	}
	if (write(fd, list, sizeof(struct _history_list)) != sizeof(struct _history_list)) {
		error_throw_fatal("1fd531fa70c1", "Failed to write disk");
		return;
	}
	zfree(list);
}

#ifdef DEBUG
void history_dump(base_t *base) {
	unsigned long long offset = base->offset.history;
	while (offset) {
		struct _history_list *list = get_history_list(base, offset);
		zassert(from_be16(list->size) <= HISTORY_LIST_SIZE);

		for (int i = 0; i < from_be16(list->size); ++i) {
			char squid[QUID_LENGTH + 1];
			quidtostr(squid, &list->items[i].quid);

			printf("Location %d key: %s, version: %d, offset: %llu\n", i, squid, from_be16(list->items[i].version), (unsigned long long)from_be64(list->items[i].offset));
		}
		offset = list->link ? from_be64(list->link) : 0;
		zfree(list);
	}
}
#endif

static unsigned short history_get_next_version(base_t *base, const quid_t *c_quid) {
	short version = -1;

	unsigned long long offset = base->offset.history;
	while (offset) {
		struct _history_list *list = get_history_list(base, offset);
		zassert(from_be16(list->size) <= HISTORY_LIST_SIZE);

		for (int i = 0; i < from_be16(list->size); ++i) {
			int cmp = quidcmp(c_quid, &list->items[i].quid);
			if (cmp == 0) {
				version = from_be16(list->items[i].version);
			}
		}
		offset = list->link ? from_be64(list->link) : 0;
		zfree(list);
	}

	if (version != -1)
		return ++version;

	return 0;
}

unsigned long long history_get_version_offset(base_t *base, const quid_t *c_quid, unsigned short version) {
	unsigned long long offset = base->offset.history;
	while (offset) {
		struct _history_list *list = get_history_list(base, offset);
		zassert(from_be16(list->size) <= HISTORY_LIST_SIZE);

		for (int i = 0; i < from_be16(list->size); ++i) {
			if (!list->items[i].offset)
				continue;

			int cmp = quidcmp(c_quid, &list->items[i].quid);
			if (cmp == 0) {
				if (from_be16(list->items[i].version) == version) {
					unsigned long long version_offset = from_be64(list->items[i].offset);
					zfree(list);
					return version_offset;
				}
			}
		}
		offset = list->link ? from_be64(list->link) : 0;
		zfree(list);
	}

	error_throw("595a8ca9706d", "Key has no history");
	return 0;
}

int history_count(base_t *base, const quid_t *c_quid) {
	int counter = 0;

	unsigned long long offset = base->offset.history;
	while (offset) {
		struct _history_list *list = get_history_list(base, offset);
		zassert(from_be16(list->size) <= HISTORY_LIST_SIZE);

		for (int i = 0; i < from_be16(list->size); ++i) {
			if (!list->items[i].offset)
				continue;

			int cmp = quidcmp(c_quid, &list->items[i].quid);
			if (cmp == 0) {
				counter++;
			}
		}
		offset = list->link ? from_be64(list->link) : 0;
		zfree(list);
	}

	return counter;
}

int history_delete(base_t *base, unsigned long long data_offset) {
	unsigned long long offset = base->offset.history;
	while (offset) {
		struct _history_list *list = get_history_list(base, offset);
		zassert(from_be16(list->size) <= HISTORY_LIST_SIZE);

		for (int i = 0; i < from_be16(list->size); ++i) {
			if (from_be64(list->items[i].offset) == data_offset) {
				memset(&list->items[i].quid, 0, sizeof(quid_t));
				list->items[i].offset = 0;
				list->items[i].version = 0;
				flush_history_list(base, list, offset);
				return 0;
			}
		}
		offset = list->link ? from_be64(list->link) : 0;
		zfree(list);
	}

	error_throw("595a8ca9706d", "Key has no history");
	return -1;
}

int history_add(base_t *base, const quid_t *c_quid, unsigned long long offset) {
	/* Does list exist */
	if (base->offset.history != 0) {
		struct _history_list *list = get_history_list(base, base->offset.history);
		zassert(from_be16(list->size) <= HISTORY_LIST_SIZE);

		memcpy(&list->items[from_be16(list->size)].quid, c_quid, sizeof(quid_t));
		list->items[from_be16(list->size)].offset = to_be64(offset);
		list->items[from_be16(list->size)].version = to_be16(history_get_next_version(base, c_quid));
		list->size = incr_be16(list->size);

		/* Check if we need to add a new table*/
		if (from_be16(list->size) >= HISTORY_LIST_SIZE) {
			flush_history_list(base, list, base->offset.history);

			struct _history_list *new_list = (struct _history_list *)zcalloc(1, sizeof(struct _history_list));
			if (!new_list) {
				zfree(new_list);
				error_throw_fatal("7b8a6ac440e2", "Failed to request memory");
				return -1;
			}

			new_list->link = to_be64(base->offset.history);
			unsigned long long new_list_offset = zpalloc(base, sizeof(struct _history_list));
			flush_history_list(base, new_list, new_list_offset);
			base->offset.history = new_list_offset;
		} else {
			flush_history_list(base, list, base->offset.history);
		}
	} else {
		struct _history_list *new_list = (struct _history_list *)zcalloc(1, sizeof(struct _history_list));
		if (!new_list) {
			zfree(new_list);
			error_throw_fatal("7b8a6ac440e2", "Failed to request memory");
			return -1;
		}

		new_list->size = to_be16(1);
		memcpy(&new_list->items[0].quid, c_quid, sizeof(quid_t));
		new_list->items[0].offset = to_be64(offset);
		new_list->items[0].version = 0;

		unsigned long long new_list_offset = zpalloc(base, sizeof(struct _history_list));
		flush_history_list(base, new_list, new_list_offset);

		base->offset.history = new_list_offset;
	}

	base_sync(base);
	return 0;
}

marshall_t *history_all(base_t *base, const quid_t *c_quid) {
	int count = history_count(base, c_quid);

	if (!count)
		return NULL;

	marshall_t *marshall = (marshall_t *)tree_zcalloc(1, sizeof(marshall_t), NULL);
	marshall->child = (marshall_t **)tree_zcalloc(count, sizeof(marshall_t *), marshall);
	marshall->type = MTYPE_ARRAY;

	unsigned long long offset = base->offset.history;
	while (offset) {
		struct _history_list *list = get_history_list(base, offset);
		zassert(from_be16(list->size) <= HISTORY_LIST_SIZE);

		for (int i = 0; i < from_be16(list->size); ++i) {
			if (!list->items[i].offset)
				continue;

			int cmp = quidcmp(c_quid, &list->items[i].quid);
			if (cmp == 0) {
				char *version_index = itoa(from_be16(list->items[i].version));
				marshall->child[marshall->size] = tree_zcalloc(1, sizeof(marshall_t), marshall);
				marshall->child[marshall->size]->type = MTYPE_INT;
				marshall->child[marshall->size]->data = tree_zstrdup(version_index, marshall);
				marshall->child[marshall->size]->data_len = strlen(version_index);
				marshall->size++;
			}
		}
		offset = list->link ? from_be64(list->link) : 0;
		zfree(list);
	}

	return marshall;
}
