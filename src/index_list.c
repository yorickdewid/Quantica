#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <config.h>
#include <common.h>
#include <log.h>
#include <error.h>
#include "zmalloc.h"
#include "quid.h"
#include "pager.h"
#include "index_list.h"

#define INDEX_LIST_SIZE	64

struct _engine_index_list {
	struct {
		quid_t index;
		quid_t group;
		__be32 element_len;
		__be64 offset;
		char element[64]; //TODO this could be any sz
	} items[INDEX_LIST_SIZE];
	uint16_t size;
	__be64 link;
} __attribute__((packed));

static struct _engine_index_list *get_index_list(base_t *base, unsigned long long offset) {
	int fd = pager_get_fd(base, &offset);

	struct _engine_index_list *list = (struct _engine_index_list *)zmalloc(sizeof(struct _engine_index_list));
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
	if (read(fd, list, sizeof(struct _engine_index_list)) != sizeof(struct _engine_index_list)) {
		zfree(list);
		error_throw_fatal("a7df40ba3075", "Failed to read disk");
		return NULL;
	}
	return list;
}

static void flush_index_list(base_t *base, struct _engine_index_list *list, unsigned long long offset) {
	int fd = pager_get_fd(base, &offset);

	if (lseek(fd, offset, SEEK_SET) < 0) {
		error_throw_fatal("1fd531fa70c1", "Failed to write disk");
		return;
	}
	if (write(fd, list, sizeof(struct _engine_index_list)) != sizeof(struct _engine_index_list)) {
		error_throw_fatal("1fd531fa70c1", "Failed to write disk");
		return;
	}
	zfree(list);
}

int index_list_add(base_t *base, const quid_t *index, const quid_t *group, char *element, unsigned long long offset) {
	/* Does list exist */
	if (base->offset.index_list != 0) {
		struct _engine_index_list *list = get_index_list(base, base->offset.index_list);
		zassert(list->size <= INDEX_LIST_SIZE - 1);

		size_t psz = strlen(element);
		memcpy(&list->items[list->size].index, index, sizeof(quid_t));
		memcpy(&list->items[list->size].group, group, sizeof(quid_t));
		memcpy(&list->items[list->size].element, element, psz);
		list->items[list->size].element_len = to_be32(psz);
		list->items[list->size].offset = to_be64(offset);
		list->size++;

		base->stats.index_list_size++;

		/* Check if we need to add a new table*/
		if (list->size >= INDEX_LIST_SIZE) {
			flush_index_list(base, list, base->offset.index_list);

			struct _engine_index_list *new_list = (struct _engine_index_list *)zcalloc(1, sizeof(struct _engine_index_list));
			if (!new_list) {
				zfree(new_list);
				error_throw_fatal("7b8a6ac440e2", "Failed to request memory");
				return -1;
			}

			new_list->link = to_be64(base->offset.index_list);
			unsigned long long new_list_offset = pager_alloc(base, sizeof(struct _engine_index_list));
			flush_index_list(base, new_list, new_list_offset);

			base->offset.index_list = new_list_offset;
		} else {
			flush_index_list(base, list, base->offset.index_list);
		}
	} else {
		struct _engine_index_list *new_list = (struct _engine_index_list *)zcalloc(1, sizeof(struct _engine_index_list));
		if (!new_list) {
			zfree(new_list);
			error_throw_fatal("7b8a6ac440e2", "Failed to request memory");
			return -1;
		}

		size_t psz = strlen(element);
		new_list->size = 1;
		memcpy(&new_list->items[0].index, index, sizeof(quid_t));
		memcpy(&new_list->items[0].group, group, sizeof(quid_t));
		memcpy(&new_list->items[0].element, element, psz);
		new_list->items[0].element_len = to_be32(psz);
		new_list->items[0].offset = to_be64(offset);

		unsigned long long new_list_offset = pager_alloc(base, sizeof(struct _engine_index_list));
		flush_index_list(base, new_list, new_list_offset);

		base->offset.index_list = new_list_offset;
		base->stats.index_list_size = 1;
	}

	/* Flush every so many times */
	if (!(base->stats.index_list_size % 2)) {
		base_sync(base);
	}

	return 0;
}

quid_t *index_list_get_index(base_t *base, const quid_t *c_quid) {
	unsigned long long offset = base->offset.index_list;
	while (offset) {
		struct _engine_index_list *list = get_index_list(base, offset);
		zassert(list->size <= INDEX_LIST_SIZE);

		for (int i = 0; i < list->size; ++i) {
			if (!list->items[i].element_len)
				continue;

			int cmp = quidcmp(c_quid, &list->items[i].group);
			if (cmp == 0) {
				quid_t *index = (quid_t *)zmalloc(sizeof(quid_t));
				memcpy(index, &list->items[i].index, sizeof(quid_t));
				zfree(list);
				return index;
			}
		}
		offset = list->link ? from_be64(list->link) : 0;
		zfree(list);
	}

	error_throw("e553d927706a", "Index not found");
	return NULL;
}

marshall_t *index_list_get_element(base_t *base, const quid_t *c_quid) {
	int alloc_children = 10;

	marshall_t *marshall = (marshall_t *)tree_zcalloc(1, sizeof(marshall_t), NULL);
	marshall->child = (marshall_t **)tree_zcalloc(alloc_children, sizeof(marshall_t *), marshall);
	marshall->type = MTYPE_ARRAY;

	unsigned long long offset = base->offset.index_list;
	while (offset) {
		struct _engine_index_list *list = get_index_list(base, offset);
		zassert(list->size <= INDEX_LIST_SIZE);

		for (int i = 0; i < list->size; ++i) {
			if (!list->items[i].element_len)
				continue;

			int cmp = quidcmp(c_quid, &list->items[i].group);
			if (cmp == 0) {
				marshall->child[marshall->size] = tree_zcalloc(1, sizeof(marshall_t), marshall);
				marshall->child[marshall->size]->type = MTYPE_STRING;
				marshall->child[marshall->size]->data = tree_zstrdup(list->items[i].element, marshall);
				marshall->child[marshall->size]->data_len = from_be32(list->items[i].element_len);
				marshall->size++;
			}
		}
		offset = list->link ? from_be64(list->link) : 0;
		zfree(list);
	}

	error_throw("e553d927706a", "Index not found");
	return marshall;
}

unsigned long long index_list_get_index_offset(base_t *base, const quid_t *c_quid) {
	unsigned long long offset = base->offset.index_list;
	while (offset) {
		struct _engine_index_list *list = get_index_list(base, offset);
		zassert(list->size <= INDEX_LIST_SIZE);

		for (int i = 0; i < list->size; ++i) {
			if (!list->items[i].element_len)
				continue;

			int cmp = quidcmp(c_quid, &list->items[i].index);
			if (cmp == 0) {
				unsigned long long index_offset = from_be64(list->items[i].offset);
				zfree(list);
				return index_offset;
			}
		}
		offset = list->link ? from_be64(list->link) : 0;
		zfree(list);
	}

	error_throw("e553d927706a", "Index not found");
	return 0;
}

int index_list_delete(base_t *base, const quid_t *index) {
	unsigned long long offset = base->offset.index_list;
	while (offset) {
		struct _engine_index_list *list = get_index_list(base, offset);
		zassert(list->size <= INDEX_LIST_SIZE);

		for (int i = 0; i < list->size; ++i) {
			if (!list->items[i].element_len)
				continue;

			int cmp = quidcmp(index, &list->items[i].index);
			if (cmp == 0) {
				memset(&list->items[i].index, 0, sizeof(quid_t));
				memset(&list->items[i].group, 0, sizeof(quid_t));
				memset(&list->items[i].element, 0, 64);
				list->items[i].element_len = 0;
				list->items[i].offset = 0;
				flush_index_list(base, list, offset);
				base->stats.index_list_size--;
				return 0;
			}
		}
		offset = list->link ? from_be64(list->link) : 0;
		zfree(list);
	}

	error_throw("e553d927706a", "Index not found");
	return -1;
}

marshall_t *index_list_all(base_t *base) {
	if (!base->stats.index_list_size)
		return NULL;

	marshall_t *marshall = (marshall_t *)tree_zcalloc(1, sizeof(marshall_t), NULL);
	marshall->child = (marshall_t **)tree_zcalloc(base->stats.index_list_size, sizeof(marshall_t *), marshall);
	marshall->type = MTYPE_OBJECT;

	unsigned long long offset = base->offset.index_list;
	while (offset) {
		struct _engine_index_list *list = get_index_list(base, offset);
		zassert(list->size <= INDEX_LIST_SIZE);

		for (int i = 0; i < list->size; ++i) {
			char index_squid[QUID_LENGTH + 1];
			char group_squid[QUID_LENGTH + 1];

			if (!list->items[i].element_len)
				continue;

			quidtostr(index_squid, &list->items[i].index);
			quidtostr(group_squid, &list->items[i].group);

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
			marshall->child[marshall->size]->child[1]->type = strisdigit(list->items[i].element) ? MTYPE_INT : MTYPE_STRING;
			marshall->child[marshall->size]->child[1]->name = tree_zstrdup("element", marshall);
			marshall->child[marshall->size]->child[1]->name_len = 7;
			marshall->child[marshall->size]->child[1]->data = tree_zstrdup(list->items[i].element, marshall);
			marshall->child[marshall->size]->child[1]->data_len = strlen(list->items[i].element);
			marshall->size++;
		}
		offset = list->link ? from_be64(list->link) : 0;
		zfree(list);
	}

	return marshall;
}
