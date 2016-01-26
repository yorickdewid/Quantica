#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <config.h>
#include <common.h>
#include <log.h>
#include <error.h>
#include "zmalloc.h"
#include "quid.h"
#include "slay_marshall.h"
#include "alias.h"
#include "index.h"
#include "pager.h"
#include "index_list.h"

#define INDEX_LIST_SIZE	64

struct _engine_index_list {
	struct {
		quid_t index;
		quid_t group;
		__be64 offset;
		__be64 element;
		__be32 element_len;
		uint8_t type;
	} items[INDEX_LIST_SIZE];
	__be16 size;
	__be64 link;
} __attribute__((packed));

static struct _engine_index_list *get_index_list(base_t *base, uint64_t offset) {
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

static void flush_index_list(base_t *base, struct _engine_index_list *list, uint64_t offset) {
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

static char *get_element_name(base_t *base, size_t element_len, uint64_t offset) {
	int fd = pager_get_fd(base, &offset);

	char *element = (char *)zcalloc(element_len + 1, sizeof(char));
	if (!element) {
		zfree(element);
		error_throw_fatal("7b8a6ac440e2", "Failed to request memory");
		return NULL;
	}
	if (lseek(fd, offset, SEEK_SET) < 0) {
		zfree(element);
		error_throw_fatal("a7df40ba3075", "Failed to read disk");
		return NULL;
	}
	if (read(fd, element, element_len) != (ssize_t)element_len) {
		zfree(element);
		error_throw_fatal("a7df40ba3075", "Failed to read disk");
		return NULL;
	}
	return element;
}

static void flush_element_name(base_t *base, char *element, size_t element_len, uint64_t offset) {
	int fd = pager_get_fd(base, &offset);

	if (lseek(fd, offset, SEEK_SET) < 0) {
		error_throw_fatal("1fd531fa70c1", "Failed to write disk");
		return;
	}
	if (write(fd, element, element_len) != (ssize_t)element_len) {
		error_throw_fatal("1fd531fa70c1", "Failed to write disk");
		return;
	}
}

int index_list_add(base_t *base, const quid_t *index, const quid_t *group, char *element, index_type_t type, uint64_t offset) {
	/* Does list exist */
	if (base->offset.index_list != 0) {
		struct _engine_index_list *list = get_index_list(base, base->offset.index_list);
		zassert(from_be16(list->size) <= INDEX_LIST_SIZE - 1);

		size_t psz = strlen(element);
		unsigned long long psz_offset = zpalloc(base, psz);
		flush_element_name(base, element, psz, psz_offset);

		memcpy(&list->items[from_be16(list->size)].index, index, sizeof(quid_t));
		memcpy(&list->items[from_be16(list->size)].group, group, sizeof(quid_t));
		list->items[from_be16(list->size)].element = to_be64(psz_offset);
		list->items[from_be16(list->size)].element_len = to_be32(psz);
		list->items[from_be16(list->size)].offset = to_be64(offset);
		list->items[from_be16(list->size)].type = type;
		list->size = incr_be16(list->size);

		base->stats.index_list_size++;

		/* Check if we need to add a new table*/
		if (from_be16(list->size) >= INDEX_LIST_SIZE) {
			flush_index_list(base, list, base->offset.index_list);

			struct _engine_index_list *new_list = (struct _engine_index_list *)zcalloc(1, sizeof(struct _engine_index_list));
			if (!new_list) {
				zfree(new_list);
				error_throw_fatal("7b8a6ac440e2", "Failed to request memory");
				return -1;
			}

			new_list->link = to_be64(base->offset.index_list);
			unsigned long long new_list_offset = zpalloc(base, sizeof(struct _engine_index_list));
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
		unsigned long long psz_offset = zpalloc(base, psz);
		flush_element_name(base, element, psz, psz_offset);

		memcpy(&new_list->items[0].index, index, sizeof(quid_t));
		memcpy(&new_list->items[0].group, group, sizeof(quid_t));
		new_list->items[0].element = to_be64(psz_offset);
		new_list->items[0].element_len = to_be32(psz);
		new_list->items[0].offset = to_be64(offset);
		new_list->items[0].type = type;
		new_list->size = to_be16(1);

		unsigned long long new_list_offset = zpalloc(base, sizeof(struct _engine_index_list));
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

/* Get index from group */
quid_t *index_list_get_index(base_t *base, const quid_t *c_quid) {
	unsigned long long offset = base->offset.index_list;
	while (offset) {
		struct _engine_index_list *list = get_index_list(base, offset);
		zassert(from_be16(list->size) <= INDEX_LIST_SIZE);

		for (int i = 0; i < from_be16(list->size); ++i) {
			if (!list->items[i].element_len && !list->items[i].offset)
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

size_t index_list_size(base_t *base, const quid_t *c_quid) {
	size_t count = 0;
	unsigned long long offset = base->offset.index_list;
	while (offset) {
		struct _engine_index_list *list = get_index_list(base, offset);
		zassert(from_be16(list->size) <= INDEX_LIST_SIZE);

		for (int i = 0; i < from_be16(list->size); ++i) {
			if (!list->items[i].element_len && !list->items[i].offset)
				continue;

			if (!quidcmp(c_quid, &list->items[i].group))
				count++;
		}
		offset = list->link ? from_be64(list->link) : 0;
		zfree(list);
	}

	return count;
}

/* Return all indexed elements on group */
marshall_t *index_list_get_element(base_t *base, const quid_t *c_quid) {
	size_t index_elements = index_list_size(base, c_quid);
	if (!index_elements)
		return NULL;

	marshall_t *marshall = (marshall_t *)tree_zcalloc(1, sizeof(marshall_t), NULL);
	marshall->child = (marshall_t **)tree_zcalloc(index_elements, sizeof(marshall_t *), marshall);
	marshall->type = MTYPE_ARRAY;

	unsigned long long offset = base->offset.index_list;
	while (offset) {
		struct _engine_index_list *list = get_index_list(base, offset);
		zassert(from_be16(list->size) <= INDEX_LIST_SIZE);

		for (int i = 0; i < from_be16(list->size); ++i) {
			if (!list->items[i].element_len && !list->items[i].offset)
				continue;

			if (!quidcmp(c_quid, &list->items[i].group)) {
				char *element = get_element_name(base, from_be32(list->items[i].element_len), from_be64(list->items[i].element));
				marshall->child[marshall->size] = tree_zcalloc(1, sizeof(marshall_t), marshall);
				marshall->child[marshall->size]->type = MTYPE_STRING;
				marshall->child[marshall->size]->data = tree_zstrdup(element, marshall);
				marshall->child[marshall->size]->data_len = from_be32(list->items[i].element_len);
				marshall->size++;
				zfree(element);
			}
		}
		offset = list->link ? from_be64(list->link) : 0;
		zfree(list);
	}

	error_throw("e553d927706a", "Index not found");
	return marshall;
}

/* Return all indexes on group */
marshall_t *index_list_on_group(base_t *base, const quid_t *c_quid) {
	size_t index_elements = index_list_size(base, c_quid);
	if (!index_elements)
		return NULL;

	marshall_t *marshall = (marshall_t *)tree_zcalloc(1, sizeof(marshall_t), NULL);
	marshall->child = (marshall_t **)tree_zcalloc(index_elements, sizeof(marshall_t *), marshall);
	marshall->type = MTYPE_ARRAY;

	unsigned long long offset = base->offset.index_list;
	while (offset) {
		struct _engine_index_list *list = get_index_list(base, offset);
		zassert(from_be16(list->size) <= INDEX_LIST_SIZE);

		for (int i = 0; i < from_be16(list->size); ++i) {
			char index_squid[QUID_LENGTH + 1];

			if (!list->items[i].element_len && !list->items[i].offset)
				continue;

			if (!quidcmp(c_quid, &list->items[i].group)) {
				quidtostr(index_squid, &list->items[i].index);

				marshall->child[marshall->size] = tree_zcalloc(1, sizeof(marshall_t), marshall);
				marshall->child[marshall->size]->child = (marshall_t **)tree_zcalloc(3, sizeof(marshall_t *), marshall);
				marshall->child[marshall->size]->type = MTYPE_OBJECT;
				marshall->child[marshall->size]->size = 3;

				/* Index */
				char *group_name = alias_get_val(base, &list->items[i].group);
				marshall->child[marshall->size]->child[0] = tree_zcalloc(1, sizeof(marshall_t), marshall);
				marshall->child[marshall->size]->child[0]->type = MTYPE_QUID;
				marshall->child[marshall->size]->child[0]->name = tree_zstrdup("index", marshall);
				marshall->child[marshall->size]->child[0]->name_len = 5;
				marshall->child[marshall->size]->child[0]->data = tree_zstrdup(index_squid, marshall);
				marshall->child[marshall->size]->child[0]->data_len = QUID_LENGTH;
				if (group_name)
					zfree(group_name);

				/* Indexed element */
				char *element = get_element_name(base, from_be32(list->items[i].element_len), from_be64(list->items[i].element));
				marshall->child[marshall->size]->child[1] = tree_zcalloc(1, sizeof(marshall_t), marshall);
				marshall->child[marshall->size]->child[1]->type = strisdigit(element) ? MTYPE_INT : MTYPE_STRING;
				marshall->child[marshall->size]->child[1]->name = tree_zstrdup("element", marshall);
				marshall->child[marshall->size]->child[1]->name_len = 7;
				marshall->child[marshall->size]->child[1]->data = tree_zstrdup(element, marshall);
				marshall->child[marshall->size]->child[1]->data_len = from_be32(list->items[i].element_len);
				zfree(element);

				char *type = index_type(list->items[i].type);
				marshall->child[marshall->size]->child[2] = tree_zcalloc(1, sizeof(marshall_t), marshall);
				marshall->child[marshall->size]->child[2]->type = MTYPE_STRING;
				marshall->child[marshall->size]->child[2]->name = tree_zstrdup("type", marshall);
				marshall->child[marshall->size]->child[2]->name_len = 4;
				marshall->child[marshall->size]->child[2]->data = tree_zstrdup(type, marshall);
				marshall->child[marshall->size]->child[2]->data_len = strlen(type);
				marshall->size++;
			}
		}
		offset = list->link ? from_be64(list->link) : 0;
		zfree(list);
	}

	return marshall;
}

/* Get offset from index */
uint64_t index_list_get_index_offset(base_t *base, const quid_t *c_quid) {
	unsigned long long offset = base->offset.index_list;
	while (offset) {
		struct _engine_index_list *list = get_index_list(base, offset);
		zassert(from_be16(list->size) <= INDEX_LIST_SIZE);

		for (int i = 0; i < from_be16(list->size); ++i) {
			if (!list->items[i].element_len && !list->items[i].offset)
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

/* Get element from index */
char *index_list_get_index_element(base_t *base, const quid_t *c_quid) {
	unsigned long long offset = base->offset.index_list;
	while (offset) {
		struct _engine_index_list *list = get_index_list(base, offset);
		zassert(from_be16(list->size) <= INDEX_LIST_SIZE);

		for (int i = 0; i < from_be16(list->size); ++i) {
			if (!list->items[i].element_len && !list->items[i].offset)
				continue;

			if (!quidcmp(c_quid, &list->items[i].index)) {
				char *element = get_element_name(base, from_be32(list->items[i].element_len), from_be64(list->items[i].element));
				zfree(list);
				return element;
			}
		}
		offset = list->link ? from_be64(list->link) : 0;
		zfree(list);
	}

	error_throw("e553d927706a", "Index not found");
	return 0;
}

/* Get group from index */
quid_t *index_list_get_index_group(base_t *base, const quid_t *c_quid) {
	unsigned long long offset = base->offset.index_list;
	while (offset) {
		struct _engine_index_list *list = get_index_list(base, offset);
		zassert(from_be16(list->size) <= INDEX_LIST_SIZE);

		for (int i = 0; i < from_be16(list->size); ++i) {
			if (!list->items[i].element_len && !list->items[i].offset)
				continue;

			if (!quidcmp(c_quid, &list->items[i].index)) {
				quid_t *group = (quid_t *)zmalloc(sizeof(quid_t));
				memcpy(group, &list->items[i].group, sizeof(quid_t));
				zfree(list);
				return group;
			}
		}
		offset = list->link ? from_be64(list->link) : 0;
		zfree(list);
	}

	error_throw("e553d927706a", "Index not found");
	return 0;
}

int index_list_update_offset(base_t *base, const quid_t *index, uint64_t index_offset) {
	unsigned long long offset = base->offset.index_list;
	while (offset) {
		struct _engine_index_list *list = get_index_list(base, offset);
		zassert(from_be16(list->size) <= INDEX_LIST_SIZE);

		for (int i = 0; i < from_be16(list->size); ++i) {
			if (!list->items[i].element_len && !list->items[i].offset)
				continue;

			if (!quidcmp(index, &list->items[i].index)) {
				list->items[i].offset = to_be64(index_offset);
				flush_index_list(base, list, offset);
				return 0;
			}
		}
		offset = list->link ? from_be64(list->link) : 0;
		zfree(list);
	}

	error_throw("e553d927706a", "Index not found");
	return -1;
}

int index_list_delete(base_t *base, const quid_t *index) {
	unsigned long long offset = base->offset.index_list;
	while (offset) {
		struct _engine_index_list *list = get_index_list(base, offset);
		zassert(from_be16(list->size) <= INDEX_LIST_SIZE);

		for (int i = 0; i < from_be16(list->size); ++i) {
			if (!list->items[i].element_len && !list->items[i].offset)
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
		zassert(from_be16(list->size) <= INDEX_LIST_SIZE);

		for (int i = 0; i < from_be16(list->size); ++i) {
			char index_squid[QUID_LENGTH + 1];
			char group_squid[QUID_LENGTH + 1];

			if (!list->items[i].element_len && !list->items[i].offset)
				continue;

			quidtostr(index_squid, &list->items[i].index);
			quidtostr(group_squid, &list->items[i].group);

			marshall->child[marshall->size] = tree_zcalloc(1, sizeof(marshall_t), marshall);
			marshall->child[marshall->size]->child = (marshall_t **)tree_zcalloc(3, sizeof(marshall_t *), marshall);
			marshall->child[marshall->size]->type = MTYPE_OBJECT;
			marshall->child[marshall->size]->name = tree_zstrdup(index_squid, marshall);
			marshall->child[marshall->size]->name_len = QUID_LENGTH;
			marshall->child[marshall->size]->size = 3;

			/* Indexed group */
			char *group_name = alias_get_val(base, &list->items[i].group);
			marshall->child[marshall->size]->child[0] = tree_zcalloc(1, sizeof(marshall_t), marshall);
			marshall->child[marshall->size]->child[0]->type = group_name ? MTYPE_STRING : MTYPE_QUID;
			marshall->child[marshall->size]->child[0]->name = tree_zstrdup("group", marshall);
			marshall->child[marshall->size]->child[0]->name_len = QUID_LENGTH;
			marshall->child[marshall->size]->child[0]->data = tree_zstrdup(group_name ? group_name : group_squid, marshall);
			marshall->child[marshall->size]->child[0]->data_len = group_name ? strlen(group_name) : 5;
			if (group_name)
				zfree(group_name);

			/* Indexed element */
			char *element = get_element_name(base, from_be32(list->items[i].element_len), from_be64(list->items[i].element));
			marshall->child[marshall->size]->child[1] = tree_zcalloc(1, sizeof(marshall_t), marshall);
			marshall->child[marshall->size]->child[1]->type = strisdigit(element) ? MTYPE_INT : MTYPE_STRING;
			marshall->child[marshall->size]->child[1]->name = tree_zstrdup("element", marshall);
			marshall->child[marshall->size]->child[1]->name_len = 7;
			marshall->child[marshall->size]->child[1]->data = tree_zstrdup(element, marshall);
			marshall->child[marshall->size]->child[1]->data_len = from_be32(list->items[i].element_len);
			zfree(element);

			/* Index type */
			char *type = index_type(list->items[i].type);
			marshall->child[marshall->size]->child[2] = tree_zcalloc(1, sizeof(marshall_t), marshall);
			marshall->child[marshall->size]->child[2]->type = MTYPE_STRING;
			marshall->child[marshall->size]->child[2]->name = tree_zstrdup("type", marshall);
			marshall->child[marshall->size]->child[2]->name_len = 4;
			marshall->child[marshall->size]->child[2]->data = tree_zstrdup(type, marshall);
			marshall->child[marshall->size]->child[2]->data_len = strlen(type);
			marshall->size++;
		}
		offset = list->link ? from_be64(list->link) : 0;
		zfree(list);
	}

	return marshall;
}

void index_list_rebuild(base_t *base, base_t *new_base) {
	unsigned long long offset = base->offset.index_list;
	while (offset) {
		struct _engine_index_list *list = get_index_list(base, offset);
		zassert(from_be16(list->size) <= INDEX_LIST_SIZE);

		for (int i = 0; i < from_be16(list->size); ++i) {
			if (!list->items[i].element_len && !list->items[i].offset)
				continue;

			size_t len;
			struct metadata meta;
			unsigned long long index_offset = engine_get(new_base, &list->items[i].group, &meta);
			if (meta.type != MD_TYPE_GROUP)
				continue;

			void *index_data = get_data_block(new_base, index_offset, &len);
			if (!index_data)
				continue;

			marshall_t *index_obj = slay_get(new_base, index_data, NULL, FALSE);
			if (!index_obj) {
				zfree(index_data);
				continue;
			}

			index_result_t nrs;
			nullify(&nrs, sizeof(index_result_t));
			char *element = get_element_name(base, from_be32(list->items[i].element_len), from_be64(list->items[i].element));

			schema_t group = slay_get_schema(index_data);
			switch (group) {
				case SCHEMA_TABLE:
					index_btree_create_table(new_base, element, index_obj, &nrs);
					break;
				case SCHEMA_SET:
					index_btree_create_set(new_base, element, index_obj, &nrs);
					break;
				default:
					continue;
			}

			marshall_free(index_obj);
			zfree(index_data);

			index_list_add(new_base, &list->items[i].index, &list->items[i].group, element, list->items[i].type, nrs.offset);
			zfree(element);
		}
		offset = list->link ? from_be64(list->link) : 0;
		zfree(list);
	}
}

char *index_type(index_type_t type) {
	switch (type) {
		case INDEX_BTREE:
			return "BTREE";
		case INDEX_HASH:
			return "HASH";
		default:
			return "NULL";
	}
}

