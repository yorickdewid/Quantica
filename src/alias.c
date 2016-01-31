#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <config.h>
#include <common.h>
#include <log.h>
#include <error.h>
#include "zmalloc.h"
#include "jenhash.h"
#include "quid.h"
#include "pager.h"
#include "alias.h"

#define ALIAS_LIST_SIZE	128

struct _alias_list {
	struct {
		quid_t quid;
		__be32 len;
		__be32 hash;
		char name[ALIAS_NAME_LENGTH];
	} items[ALIAS_LIST_SIZE];
	__be16 size;
	__be64 link;
} __attribute__((packed));

/* Read list structure from offset */
static struct _alias_list *get_alias_list(base_t *base, uint64_t offset) {
	int fd = pager_get_fd(base, &offset);

	struct _alias_list *list = (struct _alias_list *)zcalloc(1, sizeof(struct _alias_list));
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
	if (read(fd, list, sizeof(struct _alias_list)) != sizeof(struct _alias_list)) {
		zfree(list);
		error_throw_fatal("a7df40ba3075", "Failed to read disk");
		return NULL;
	}
	return list;
}

static void flush_alias_list(base_t *base, struct _alias_list *list, uint64_t offset) {
	int fd = pager_get_fd(base, &offset);

	if (lseek(fd, offset, SEEK_SET) < 0) {
		error_throw_fatal("1fd531fa70c1", "Failed to write disk");
		return;
	}
	if (write(fd, list, sizeof(struct _alias_list)) != sizeof(struct _alias_list)) {
		error_throw_fatal("1fd531fa70c1", "Failed to write disk");
		return;
	}
	zfree(list);
}

int alias_add(base_t *base, const quid_t *c_quid, const char *c_name, size_t len) {
	/* Name is max 32 */
	if (len > ALIAS_NAME_LENGTH) {
		len = ALIAS_NAME_LENGTH;
	}

	unsigned int hash = jen_hash((unsigned char *)c_name, len);

	/* does list exist */
	if (base->offset.alias != 0) {
		struct _alias_list *list = get_alias_list(base, base->offset.alias);
		zassert(from_be16(list->size) <= ALIAS_LIST_SIZE);

		memcpy(&list->items[from_be16(list->size)].quid, c_quid, sizeof(quid_t));
		memcpy(&list->items[from_be16(list->size)].name, c_name, len);
		list->items[from_be16(list->size)].len = to_be32(len);
		list->items[from_be16(list->size)].hash = to_be32(hash);
		list->size = incr_be16(list->size);

		base->stats.alias_size++;

		/* Check if we need to add a new table*/
		if (from_be16(list->size) >= ALIAS_LIST_SIZE) {
			flush_alias_list(base, list, base->offset.alias);

			struct _alias_list *new_list = (struct _alias_list *)zcalloc(1, sizeof(struct _alias_list));
			if (!new_list) {
				zfree(new_list);
				error_throw_fatal("7b8a6ac440e2", "Failed to request memory");
				return -1;
			}

			new_list->link = to_be64(base->offset.alias);
			uint64_t new_list_offset = zpalloc(base, sizeof(struct _alias_list));
			flush_alias_list(base, new_list, new_list_offset);
			base->offset.alias = new_list_offset;
		} else {
			flush_alias_list(base, list, base->offset.alias);
		}
	} else {
		struct _alias_list *new_list = (struct _alias_list *)zcalloc(1, sizeof(struct _alias_list));
		if (!new_list) {
			zfree(new_list);
			error_throw_fatal("7b8a6ac440e2", "Failed to request memory");
			return -1;
		}

		new_list->size = to_be16(1);
		memcpy(&new_list->items[0].quid, c_quid, sizeof(quid_t));
		memcpy(&new_list->items[0].name, c_name, len);
		new_list->items[0].len = to_be32(len);
		new_list->items[0].hash = to_be32(hash);

		uint64_t new_list_offset = zpalloc(base, sizeof(struct _alias_list));
		flush_alias_list(base, new_list, new_list_offset);

		base->offset.alias = new_list_offset;
		base->stats.alias_size = 1;
	}

	/* Flush every so many times */
	if (!(base->stats.alias_size % 4)) {
		base_sync(base);
	}

	return 0;
}

char *alias_get_val(base_t *base, const quid_t *c_quid) {
	uint64_t offset = base->offset.alias;
	while (offset) {
		struct _alias_list *list = get_alias_list(base, offset);
		zassert(from_be16(list->size) <= ALIAS_LIST_SIZE);

		for (int i = 0; i < from_be16(list->size); ++i) {
			if (!list->items[i].len)
				continue;

			int cmp = quidcmp(c_quid, &list->items[i].quid);
			if (cmp == 0) {
				size_t len = from_be32(list->items[i].len);
				char *name = (char *)zmalloc(len + 1);
				name[len] = '\0';
				memcpy(name, list->items[i].name, len);
				zfree(list);
				return name;
			}
		}
		offset = list->link ? from_be64(list->link) : 0;
		zfree(list);
	}

	error_throw("2836444cd009", "Alias not found");
	return NULL;
}

int alias_get_key(base_t *base, quid_t *key, const char *name, size_t len) {
	unsigned int hash = jen_hash((unsigned char *)name, len);
	uint64_t offset = base->offset.alias;
	while (offset) {
		struct _alias_list *list = get_alias_list(base, offset);
		zassert(from_be16(list->size) <= ALIAS_LIST_SIZE);

		for (int i = 0; i < from_be16(list->size); ++i) {
			if (!list->items[i].len)
				continue;

			if (from_be32(list->items[i].hash) == hash) {
				memcpy(key, &list->items[i].quid, sizeof(quid_t));
				zfree(list);
				return 0;
			}
		}
		offset = list->link ? from_be64(list->link) : 0;
		zfree(list);
	}

	error_throw("2836444cd009", "Alias not found");
	return -1;
}

int alias_update(base_t *base, const quid_t *c_quid, const char *name, size_t len) {
	unsigned int hash = jen_hash((unsigned char *)name, len);
	uint64_t offset = base->offset.alias;
	while (offset) {
		struct _alias_list *list = get_alias_list(base, offset);
		zassert(from_be16(list->size) <= ALIAS_LIST_SIZE);

		for (int i = 0; i < from_be16(list->size); ++i) {
			if (!list->items[i].len)
				continue;

			int cmp = quidcmp(c_quid, &list->items[i].quid);
			if (cmp == 0) {
				memcpy(&list->items[i].name, name, len);
				list->items[i].len = to_be32(len);
				list->items[i].hash = to_be32(hash);
				flush_alias_list(base, list, offset);
				return 0;
			}
		}
		offset = list->link ? from_be64(list->link) : 0;
		zfree(list);
	}

	error_throw("2836444cd009", "Alias not found");
	return -1;
}

int alias_delete(base_t *base, const quid_t *c_quid) {
	uint64_t offset = base->offset.alias;
	while (offset) {
		struct _alias_list *list = get_alias_list(base, offset);
		zassert(from_be16(list->size) <= ALIAS_LIST_SIZE);

		for (int i = 0; i < from_be16(list->size); ++i) {
			if (!list->items[i].len)
				continue;

			int cmp = quidcmp(c_quid, &list->items[i].quid);
			if (cmp == 0) {
				memset(&list->items[i].quid, 0, sizeof(quid_t));
				list->items[i].len = 0;
				flush_alias_list(base, list, offset);
				base->stats.alias_size--;
				return 0;
			}
		}
		offset = list->link ? from_be64(list->link) : 0;
		zfree(list);
	}

	error_throw("2836444cd009", "Alias not found");
	return -1;
}

marshall_t *alias_all(base_t *base) {
	if (!base->stats.alias_size)
		return NULL;

	marshall_t *marshall = (marshall_t *)tree_zcalloc(1, sizeof(marshall_t), NULL);
	marshall->child = (marshall_t **)tree_zcalloc(base->stats.alias_size, sizeof(marshall_t *), marshall);
	marshall->type = MTYPE_OBJECT;

	uint64_t offset = base->offset.alias;
	while (offset) {
		struct _alias_list *list = get_alias_list(base, offset);
		zassert(from_be16(list->size) <= ALIAS_LIST_SIZE);

		for (int i = 0; i < from_be16(list->size); ++i) {
			char squid[QUID_LENGTH + 1];
			quidtostr(squid, &list->items[i].quid);

			size_t len = from_be32(list->items[i].len);
			if (!len)
				continue;

			if (list->items[i].name[0] == '_')
				continue;

			list->items[i].name[len] = '\0';

			marshall->child[marshall->size] = tree_zcalloc(1, sizeof(marshall_t), marshall);
			marshall->child[marshall->size]->type = MTYPE_QUID;
			marshall->child[marshall->size]->name = tree_zstrdup(squid, marshall);
			marshall->child[marshall->size]->name_len = QUID_LENGTH;
			marshall->child[marshall->size]->data = tree_zstrdup(list->items[i].name, marshall);
			marshall->child[marshall->size]->data_len = len;
			marshall->size++;
		}
		offset = list->link ? from_be64(list->link) : 0;
		zfree(list);
	}

	return marshall;
}

void alias_rebuild(base_t *base, base_t *new_base) {
	uint64_t offset = base->offset.alias;
	while (offset) {
		struct _alias_list *list = get_alias_list(base, base->offset.alias);
		zassert(from_be16(list->size) <= ALIAS_LIST_SIZE);

		for (int i = 0; i < from_be16(list->size); ++i) {
			size_t len = from_be32(list->items[i].len);
			if (!len)
				continue;

			if (list->items[i].name[0] == '_')
				continue;

			list->items[i].name[len] = '\0';

			alias_add(new_base, &list->items[i].quid, list->items[i].name, len);
		}
		offset = list->link ? from_be64(list->link) : 0;
		zfree(list);
	}
}
