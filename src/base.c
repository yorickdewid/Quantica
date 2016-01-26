#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#include <config.h>
#include <common.h>
#include <log.h>
#include <error.h>
#include "arc4random.h"
#include "diagnose.h"
#include "zmalloc.h"
#include "pager.h"
#include "base.h"

#define BASECONTROL		"base"
#define BASECONTROLTMP	"~" BASECONTROL
#define INSTANCE_RANDOM	5
#define BASE_MAGIC		"$EOBCTRL$"

static enum exit_status exit_status;

static char *generate_instance_name() {
	static const char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	int len = INSTANCE_RANDOM;
	char strrand[INSTANCE_RANDOM];
	for (int i = 0; i < len; ++i)
		strrand[i] = alphanum[arc4random() % (sizeof(alphanum) - 1)];
	strrand[len - 1] = 0;

	static char buf[INSTANCE_LENGTH];
	strlcpy(buf, INSTANCE_PREFIX, INSTANCE_LENGTH);
	strlcat(buf, "_", INSTANCE_LENGTH);
	strlcat(buf, strrand, INSTANCE_LENGTH);

	return buf;
}

#ifdef DEBUG
void base_list(base_t *base) {
	struct _page_list list;
	nullify(&list, sizeof(struct _page_list));

	for (unsigned int i = 0; i <= base->page_list_count; ++i) {
		unsigned long offset = sizeof(struct _base) * (i + 1);
		if (lseek(base->fd, offset, SEEK_SET) < 0) {
			lprint("[erro] Failed to read " BASECONTROL "\n");
			return;
		}
		if (read(base->fd, &list, sizeof(struct _page_list)) != sizeof(struct _page_list)) {
			lprint("[erro] Failed to read " BASECONTROL "\n");
			return;
		}

		for (unsigned short x = 0; x < from_be16(list.size); ++x) {
			char name[SHORT_QUID_LENGTH + 1];
			quid_shorttostr(name, &list.item[x].page_key);

			printf("Location %d:%d key: %s, free: %d\n", i, x, name, list.item[x].free);
		}
	}
}
#endif

void base_list_delete(base_t *base, quid_short_t *key) {
	struct _page_list list;
	nullify(&list, sizeof(struct _page_list));

	for (unsigned int i = 0; i <= base->page_list_count; ++i) {
		unsigned long offset = sizeof(struct _base) * (i + 1);
		if (lseek(base->fd, offset, SEEK_SET) < 0) {
			lprint("[erro] Failed to read " BASECONTROL "\n");
			return;
		}
		if (read(base->fd, &list, sizeof(struct _page_list)) != sizeof(struct _page_list)) {
			lprint("[erro] Failed to read " BASECONTROL "\n");
			return;
		}

		for (unsigned short x = 0; x < from_be16(list.size); ++x) {
			if (!quid_shortcmp(&list.item[x].page_key, key)) {
				list.item[x].free = 1;

				if (lseek(base->fd, offset, SEEK_SET) < 0) {
					lprint("[erro] Failed to read " BASECONTROL "\n");
					return;
				}
				if (write(base->fd, &list, sizeof(struct _page_list)) != sizeof(struct _page_list)) {
					lprint("[erro] Failed to write " BASECONTROL "\n");
					return;
				}
				return;

			}
		}
	}
}

void base_list_set_crc_sum(base_t *base, quid_short_t *key, unsigned long long sum) {
	struct _page_list list;
	nullify(&list, sizeof(struct _page_list));

	for (unsigned int i = 0; i <= base->page_list_count; ++i) {
		unsigned long offset = sizeof(struct _base) * (i + 1);
		if (lseek(base->fd, offset, SEEK_SET) < 0) {
			lprint("[erro] Failed to read " BASECONTROL "\n");
			return;
		}
		if (read(base->fd, &list, sizeof(struct _page_list)) != sizeof(struct _page_list)) {
			lprint("[erro] Failed to read " BASECONTROL "\n");
			return;
		}

		for (unsigned short x = 0; x < from_be16(list.size); ++x) {
			if (!quid_shortcmp(&list.item[x].page_key, key)) {
				list.item[x].crc_sum = to_be64(sum);

				if (lseek(base->fd, offset, SEEK_SET) < 0) {
					lprint("[erro] Failed to read " BASECONTROL "\n");
					return;
				}
				if (write(base->fd, &list, sizeof(struct _page_list)) != sizeof(struct _page_list)) {
					lprint("[erro] Failed to write " BASECONTROL "\n");
					return;
				}
				return;

			}
		}
	}
}

void base_list_add(base_t *base, quid_short_t *key) {
	struct _page_list list;
	nullify(&list, sizeof(struct _page_list));
	bool try_next = TRUE;
	bool fill_gap = FALSE;

	for (unsigned int i = 0; i <= base->page_list_count; ++i) {
		unsigned long offset = sizeof(struct _base) * (i + 1);
		if (lseek(base->fd, offset, SEEK_SET) < 0) {
			lprint("[erro] Failed to read " BASECONTROL "\n");
			return;
		}
		if (read(base->fd, &list, sizeof(struct _page_list)) != sizeof(struct _page_list)) {
			lprint("[erro] Failed to read " BASECONTROL "\n");
			return;
		}

		unsigned short idx = from_be16(list.size);
		for (unsigned short x = 0; x < idx; ++x) {
			if (list.item[x].free) {
				idx = x;
				try_next = FALSE;
				fill_gap = TRUE;
				goto write_page;
			}
		}

		if (idx == PAGE_LIST_SIZE) {
			if (i != base->page_list_count) {
				continue;
			} else {
				nullify(&list, sizeof(struct _page_list));
				offset = sizeof(struct _base) * (++base->page_list_count + 1);
				idx = 0;
				try_next = FALSE;
			}
		}

write_page:
		list.item[idx].free = 0;
		memcpy(&list.item[idx].page_key, key, sizeof(quid_short_t));
		if (!fill_gap)
			list.size = to_be16(++idx);

		if (lseek(base->fd, offset, SEEK_SET) < 0) {
			lprint("[erro] Failed to read " BASECONTROL "\n");
			return;
		}
		if (write(base->fd, &list, sizeof(struct _page_list)) != sizeof(struct _page_list)) {
			lprint("[erro] Failed to write " BASECONTROL "\n");
			return;
		}
		if (!try_next) {
			base_sync(base);
			return;
		}
	}
}

void base_sync(base_t *base) {
	struct _base super;
	nullify(&super, sizeof(struct _base));

	super.instance_key = base->instance_key;
	super.lock = base->lock;
	super.version = to_be16(VERSION_MAJOR);
	super.exitstatus = exit_status;
	super.page_list_count = to_be16(base->page_list_count);
	super.pager.size = base->pager.size;
	super.pager.sequence = to_be32(base->pager.sequence);
	super.pager.offset = to_be64(base->pager.offset);
	super.offset.alias = to_be64(base->offset.alias);
	super.offset.history = to_be64(base->offset.history);
	super.offset.zero = to_be64(base->offset.zero);
	super.offset.heap = to_be64(base->offset.heap);
	super.offset.index_list = to_be64(base->offset.index_list);
	super.stats.zero_size = to_be64(base->stats.zero_size);
	super.stats.zero_free_size = to_be64(base->stats.zero_free_size);
	super.stats.heap_free_size = to_be64(base->stats.heap_free_size);
	super.stats.alias_size = to_be64(base->stats.alias_size);
	super.stats.index_list_size = to_be64(base->stats.index_list_size);
	strlcpy((char *)super.instance_name, base->instance_name, INSTANCE_LENGTH);
	strlcpy((char *)super.magic, BASE_MAGIC, MAGIC_LENGTH);

	if (lseek(base->fd, 0, SEEK_SET) < 0) {
		lprint("[erro] Failed to read " BASECONTROL "\n");
		return;
	}
	if (write(base->fd, &super, sizeof(struct _base)) != sizeof(struct _base)) {
		lprint("[erro] Failed to write " BASECONTROL "\n");
		return;
	}
}

void base_lock(base_t *base) {
	base->lock = TRUE;
}

void base_init(base_t *base, engine_t *engine) {
	nullify(base, sizeof(base_t));
	zassert(DEFAULT_PAGE_SIZE > MIN_PAGE_SIZE && DEFAULT_PAGE_SIZE < MAX_PAGE_SIZE);

	base->engine = engine;
	if (file_access_exists(BASECONTROL) < 0) {
		lprint("[erro] Cannot access " BASECONTROL "\n");
		return;
	} else if (file_access_exists(BASECONTROL) > 0) {

		/* Open existing database */
		base->fd = open(BASECONTROL, O_RDWR | O_BINARY);
		if (base->fd < 0)
			return;

		struct _base super;
		nullify(&super, sizeof(struct _base));
		if (read(base->fd, &super, sizeof(struct _base)) != sizeof(struct _base)) {
			lprint("[erro] Failed to read " BASECONTROL "\n");
			return;
		}

		base->instance_key = super.instance_key;
		base->lock = super.lock;
		base->page_list_count = from_be16(super.page_list_count);
		base->pager.size = super.pager.size;
		base->pager.sequence = from_be32(super.pager.sequence);
		base->pager.offset = from_be64(super.pager.offset);
		base->offset.alias = from_be64(super.offset.alias);
		base->offset.history = from_be64(super.offset.history);
		base->offset.zero = from_be64(super.offset.zero);
		base->offset.heap = from_be64(super.offset.heap);
		base->offset.index_list = from_be64(super.offset.index_list);
		base->stats.zero_size = from_be64(super.stats.zero_size);
		base->stats.zero_free_size = from_be64(super.stats.zero_free_size);
		base->stats.heap_free_size = from_be64(super.stats.heap_free_size);
		base->stats.alias_size = from_be64(super.stats.alias_size);
		base->stats.index_list_size = from_be64(super.stats.index_list_size);
		super.instance_name[INSTANCE_LENGTH - 1] = '\0';
		strlcpy(base->instance_name, (char *)super.instance_name, INSTANCE_LENGTH);

		zassert(from_be16(super.version) == VERSION_MAJOR);
		zassert(!strcmp((char *)super.magic, BASE_MAGIC));
		if (super.exitstatus != EXSTAT_SUCCESS) {
			if (diag_exerr(base)) {
				exit_status = EXSTAT_CHECKPOINT;
				return;
			} else {
				error_throw_fatal("ef4b4df470a1", "Storage damaged beyond autorecovery");
				exit_status = EXSTAT_ERROR;
				return;
			}
		}
		exit_status = EXSTAT_CHECKPOINT;
	} else {

		/* Create new database */
		quid_create(&base->instance_key);
		base->pager.sequence = 1;
		base->pager.size = DEFAULT_PAGE_SIZE;
		strlcpy(base->instance_name, generate_instance_name(), INSTANCE_LENGTH);
		exit_status = EXSTAT_INVALID;

		base->fd = open(BASECONTROL, O_RDWR | O_TRUNC | O_CREAT | O_BINARY, 0644);
		if (base->fd < 0) {
			lprint("[erro] Failed to read " BASECONTROL "\n");
			return;
		}

		base_sync(base);

		/* We'll need at least one page */
		struct _page_list list;
		nullify(&list, sizeof(struct _page_list));
		if (write(base->fd, &list, sizeof(struct _page_list)) != sizeof(struct _page_list)) {
			lprint("[erro] Failed to write " BASECONTROL "\n");
			return;
		}

		exit_status = EXSTAT_CHECKPOINT;
	}
}

void base_copy(base_t *base, base_t *new_base, engine_t *new_engine, unsigned char page_size) {
	nullify(new_base, sizeof(base_t));

	if (page_size <= MIN_PAGE_SIZE || DEFAULT_PAGE_SIZE >= MAX_PAGE_SIZE)
		return;

	/* Copy current config */
	new_base->pager.sequence = 1;
	new_base->pager.size = page_size;
	new_base->engine = new_engine;
	new_base->instance_key = base->instance_key;
	strlcpy(new_base->instance_name, base->instance_name, INSTANCE_LENGTH);

	new_base->fd = open(BASECONTROLTMP, O_RDWR | O_TRUNC | O_CREAT | O_BINARY, 0644);
	if (base->fd < 0) {
		lprint("[erro] Failed to read " BASECONTROLTMP "\n");
		return;
	}

	base_sync(new_base);

	/* We'll need at least one page */
	struct _page_list list;
	nullify(&list, sizeof(struct _page_list));
	if (write(new_base->fd, &list, sizeof(struct _page_list)) != sizeof(struct _page_list)) {
		lprint("[erro] Failed to write " BASECONTROLTMP "\n");
		return;
	}
}

void base_swap() {
	unlink(BASECONTROL);
	rename(BASECONTROLTMP, BASECONTROL);
}

void base_close(base_t *base) {
	exit_status = EXSTAT_SUCCESS;
	base_sync(base);
	close(base->fd);
	lprint("[info] Exit with EXSTAT_SUCCESS\n");
}
