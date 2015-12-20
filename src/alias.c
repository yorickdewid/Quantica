#include <string.h>
#include <stdlib.h>
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

#define LIST_SIZE	128

struct _alias_list {
	struct {
		quid_t quid;
		__be32 len;
		__be32 hash;
		char name[ALIAS_NAME_LENGTH];
	} items[LIST_SIZE];
	uint16_t size;
	__be64 link;
} __attribute__((packed));

/* Read list structure from offset */
static struct _alias_list *get_tablelist(base_t *base, unsigned long long offset) {
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
#include <stdio.h>
static void flush_tablelist(base_t *base, struct _alias_list *list, unsigned long long offset) {
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
		struct _alias_list *list = get_tablelist(base, base->offset.alias);
		zassert(list->size <= LIST_SIZE - 1);

		memcpy(&list->items[list->size].quid, c_quid, sizeof(quid_t));
		memcpy(&list->items[list->size].name, c_name, len);
		list->items[list->size].len = to_be32(len);
		list->items[list->size].hash = to_be32(hash);
		list->size++;

		//engine->stats.list_size++;//TODO

		/* Check if we need to add a new table*/
		if (list->size >= LIST_SIZE) {
			flush_tablelist(base, list, base->offset.alias);

			struct _alias_list *new_list = (struct _alias_list *)zcalloc(1, sizeof(struct _alias_list));
			if (!new_list) {
				zfree(new_list);
				error_throw_fatal("7b8a6ac440e2", "Failed to request memory");
				return -1;
			}

			new_list->link = to_be64(base->offset.alias);

			unsigned long long new_list_offset = pager_alloc(base, sizeof(struct _alias_list));
			flush_tablelist(base, new_list, new_list_offset);
			base->offset.alias = new_list_offset;
		} else {
			flush_tablelist(base, list, base->offset.alias);
		}
	} else {
		struct _alias_list *new_list = (struct _alias_list *)zcalloc(1, sizeof(struct _alias_list));
		if (!new_list) {
			zfree(new_list);
			error_throw_fatal("7b8a6ac440e2", "Failed to request memory");
			return -1;
		}

		new_list->size = 1;
		memcpy(&new_list->items[0].quid, c_quid, sizeof(quid_t));
		memcpy(&new_list->items[0].name, c_name, len);
		new_list->items[0].len = to_be32(len);
		new_list->items[0].hash = to_be32(hash);

		unsigned long long new_list_offset = pager_alloc(base, sizeof(struct _alias_list));
		flush_tablelist(base, new_list, new_list_offset);

		base->offset.alias = new_list_offset;
		//engine->stats.list_size = 1;//TODO
	}
	//flush_super(engine, TRUE);

	return 0;
}
