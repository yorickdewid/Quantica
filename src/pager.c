#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <math.h>

#include <config.h>
#include <common.h>
#include <log.h>
#include <error.h>
#include "zmalloc.h"
#include "crc64.h"
#include "base.h"
#include "pager.h"

#define DEFAULT_PAGE_ALLOC	10
#define PAGE_MAGIC			"$OYPTTRL$"
#define MAGIC_LENGTH		10

struct _page {
	__be32 sequence;
	char exit_status;
	char magic[MAGIC_LENGTH];
	__be16 version;
} __attribute__((packed));

#ifdef DEBUG
void pager_list(base_t *base) {
	for (unsigned int i = 0; i < base->core->count; ++i) {
		char name[SHORT_QUID_LENGTH + 1];
		quid_shorttostr(name, &base->core->pages[i]->page_key);

		printf("Location %d fd: %d, key: %s, sequence: %u\n", i, base->core->pages[i]->fd, name, base->core->pages[i]->sequence);
	}
}
#endif

static void flush_page(page_t *page) {
	struct _page super;
	nullify(&super, sizeof(struct _page));

	super.sequence = to_be32(page->sequence);
	super.version = to_be16(VERSION_MAJOR);
	super.exit_status = page->exit_status;
	strlcpy(super.magic, PAGE_MAGIC, MAGIC_LENGTH);
	if (lseek(page->fd, 0, SEEK_SET) < 0) {
		error_throw_fatal("1fd531fa70c1", "Failed to write disk");
		return;
	}
	if (write(page->fd, &super, sizeof(struct _page)) != sizeof(struct _page)) {
		error_throw_fatal("1fd531fa70c1", "Failed to write disk");
		return;
	}
}

static uint64_t page_crc_sum(page_t *page) {
	uint64_t crc64sum;
	if (!crc_file(page->fd, &crc64sum)) {
		lprint("[erro] Failed to calculate CRC\n");
		return 0;
	}
	return crc64sum;
}

static void create_page(base_t *base, pager_t *core) {
	struct _page super;
	char name[SHORT_QUID_LENGTH + 1];
	nullify(&super, sizeof(struct _page));

	page_t *page = (page_t *)tree_zcalloc(1, sizeof(page_t), core);
	quid_short_create(&page->page_key);
	quid_shorttostr(name, &page->page_key);
	page->sequence = base->pager.sequence++;
	page->fd = open(name, O_RDWR | O_TRUNC | O_CREAT | O_BINARY, 0644);
	if (page->fd < 0) {
		error_throw_fatal("65ccc95b60a6", "Failed to acquire descriptor");
		return;
	}

	super.sequence = to_be32(page->sequence);
	super.version = to_be16(VERSION_MAJOR);
	super.exit_status = EXSTAT_INVALID;
	strlcpy(super.magic, PAGE_MAGIC, MAGIC_LENGTH);
	if (write(page->fd, &super, sizeof(struct _page)) != sizeof(struct _page)) {
		error_throw_fatal("1fd531fa70c1", "Failed to write disk");
		return;
	}

	if (core->count >= core->allocated) {
		core->allocated += DEFAULT_PAGE_ALLOC;
		core->pages = (page_t **)tree_zrealloc(core->pages, core->allocated * sizeof(page_t *));
	}

	page->exit_status = EXSTAT_CHECKPOINT;
	core->pages[core->count++] = page;
	base_list_add(base, &page->page_key);
	flush_page(page);
}

static void open_page(quid_short_t *page_key, pager_t *core, unsigned long long sum) {
	struct _page super;
	char name[SHORT_QUID_LENGTH + 1];
	nullify(&super, sizeof(struct _page));

	page_t *page = (page_t *)tree_zcalloc(1, sizeof(page_t), core);
	quid_shorttostr(name, page_key);
	page->fd = open(name, O_RDWR | O_BINARY);
	if (page->fd < 0) {
		error_throw_fatal("65ccc95b60a6", "Failed to acquire descriptor");
		return;
	}

	uint64_t crc64sum;
	if (!crc_file(page->fd, &crc64sum)) {
		lprint("[erro] Failed to calculate CRC\n");
		return;
	}

	if (crc64sum != sum) {
		lprintf("[erro] Page %d corrupt\n", page->sequence);
		//TODO we should do something
		return;
	}

	if (read(page->fd, &super, sizeof(struct _page)) != sizeof(struct _page)) {
		error_throw_fatal("a7df40ba3075", "Failed to read disk");
		return;
	}

	page->page_key = *page_key;
	page->sequence = from_be32(super.sequence);
	page->exit_status = EXSTAT_CHECKPOINT;
	zassert(from_be16(super.version) == VERSION_MAJOR);
	zassert(!strcmp(super.magic, PAGE_MAGIC));
	if (super.exit_status != EXSTAT_SUCCESS) {
		lprintf("[warn] Page %d was not flushed on exit\n", page->sequence);
		//TODO we should do something
		return;
	}

	if (core->count >= core->allocated) {
		core->allocated += DEFAULT_PAGE_ALLOC;
		core->pages = (page_t **)tree_zrealloc(core->pages, core->allocated * sizeof(page_t *));
	}
	core->pages[core->count++] = page;
}

uint64_t pager_alloc(base_t *base, size_t len) {
	zassert(len > 0);

	bool flush = FALSE;
	unsigned long long page_size = BASE_PAGE_SIZE << base->pager.size;
	uint64_t offset = base->pager.offset;

	/* Create new page */
	if ((offset % page_size) + len >= page_size) {
		create_page(base, base->core);

		offset = ((base->core->count - 1) * page_size);
		offset += sizeof(struct _page);
		flush = TRUE;
	}

	base->pager.offset = offset + len;
	if (flush) {
		base_sync(base);
	}

	return offset;
}

int pager_get_fd(const base_t *base, uint64_t *offset) {
	unsigned long long page_size = BASE_PAGE_SIZE << base->pager.size;
	unsigned long long page = floor(*offset / page_size);

	zassert(page <= (base->core->count - 1));
	*offset %= page_size;
	return base->core->pages[page]->fd;
}

unsigned int pager_get_sequence(base_t *base, uint64_t offset) {
	unsigned long long page_size = BASE_PAGE_SIZE << base->pager.size;
	unsigned long long page = floor(offset / page_size);

	zassert(page <= (base->core->count - 1));
	return base->core->pages[page]->sequence;
}

/*
 * Initialize all pages
 */
void pager_init(base_t *base) {
	struct _page_list list;
	nullify(&list, sizeof(struct _page_list));

	base->core = (pager_t *)tree_zcalloc(1, sizeof(pager_t), NULL);
	for (unsigned int i = 0; i <= base->page_list_count; ++i) {
		unsigned long offset = sizeof(struct _base) * (i + 1);
		if (lseek(base->fd, offset, SEEK_SET) < 0) {
			error_throw_fatal("a7df40ba3075", "Failed to read disk");
			return;
		}
		if (read(base->fd, &list, sizeof(struct _page_list)) != sizeof(struct _page_list)) {
			error_throw_fatal("a7df40ba3075", "Failed to read disk");
			return;
		}

		unsigned short list_size = from_be16(list.size);
		if (i == 0 && list_size == 0) {
			lprint("[info] Creating dataheap\n");

			base->core->allocated = DEFAULT_PAGE_ALLOC;
			base->core->pages = (page_t **)tree_zcalloc(base->core->allocated, sizeof(page_t *), base->core);
			create_page(base, base->core);
			base->pager.offset = sizeof(struct _page);
			goto flush_base;
		} else {
			base->core->allocated = list_size < DEFAULT_PAGE_ALLOC ? DEFAULT_PAGE_ALLOC : list_size + DEFAULT_PAGE_ALLOC;
			base->core->pages = (page_t **)tree_zcalloc(base->core->allocated, sizeof(page_t *), base->core);
			for (unsigned short x = 0; x < list_size; ++x) {
				open_page(&list.item[x].page_key, base->core, from_be64(list.item[x].crc_sum));
			}
		}
	}

flush_base:
	base_sync(base);
}

void pager_sync(base_t *base) {
	for (unsigned int i = 0; i < base->core->count; ++i) {
		base->core->pages[i]->exit_status = EXSTAT_SUCCESS;
		flush_page(base->core->pages[i]);
		base_list_set_crc_sum(base, &base->core->pages[i]->page_key, page_crc_sum(base->core->pages[i]));
	}
}

void pager_close(base_t *base) {
	for (unsigned int i = 0; i < base->core->count; ++i) {
		base->core->pages[i]->exit_status = EXSTAT_SUCCESS;
		flush_page(base->core->pages[i]);
		base_list_set_crc_sum(base, &base->core->pages[i]->page_key, page_crc_sum(base->core->pages[i]));
		close(base->core->pages[i]->fd);
	}
	tree_zfree(base->core);
}

void pager_unlink_all(base_t *base) {
	for (unsigned int i = 0; i < base->core->count; ++i) {
		char name[SHORT_QUID_LENGTH + 1];
		quid_shorttostr(name, &base->core->pages[i]->page_key);
		unlink(name);
	}
}

size_t pager_total_disk_size(base_t *base) {
	size_t total = 0;
	for (unsigned int i = 0; i < base->core->count; ++i) {
		total += file_size(base->core->pages[i]->fd);
	}
	return total;
}

marshall_t *pager_all(base_t *base) {
	if (!base->core->count)
		return NULL;

	marshall_t *marshall = (marshall_t *)tree_zcalloc(1, sizeof(marshall_t), NULL);
	marshall->child = (marshall_t **)tree_zcalloc(base->core->count, sizeof(marshall_t *), marshall);
	marshall->type = MTYPE_OBJECT;

	for (unsigned int i = 0; i < base->core->count; ++i) {
		char name[SHORT_QUID_LENGTH + 1];
		quid_shorttostr(name, &base->core->pages[i]->page_key);
		char *seq = itoa(base->core->pages[i]->sequence);
		size_t len = strlen(seq);

		marshall->child[marshall->size] = tree_zcalloc(1, sizeof(marshall_t), marshall);
		marshall->child[marshall->size]->type = MTYPE_INT;
		marshall->child[marshall->size]->name = tree_zstrdup(name, marshall);
		marshall->child[marshall->size]->name_len = SHORT_QUID_LENGTH;
		marshall->child[marshall->size]->data = tree_zstrdup(seq, marshall);
		marshall->child[marshall->size]->data_len = len;
		marshall->size++;
	}

	return marshall;
}
