#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <math.h>

#include <config.h>
#include <common.h>
#include "log.h"
#include "zmalloc.h"
#include "base.h"
#include "pager.h"

#define DEFAULT_PAGE_ALLOC	10

struct _page {
	__be32 sequence;
} __attribute__((packed));

#if 0
static size_t page_align(size_t val) {
	size_t i = 1;
	while (i < val)
		i <<= 1;
	return i;
}
#endif

#ifdef DEBUG
void pager_list(base_t *base) {
	for (unsigned int i = 0; i < ((pager_t *)base->core)->count; ++i) {
		char name[SHORT_QUID_LENGTH + 1];
		quid_shorttostr(name, &((pager_t *)base->core)->pages[i]->page_key);

		printf("Location %d fd: %d, key: %s, sequence: %u\n", i, ((pager_t *)base->core)->pages[i]->fd, name, ((pager_t *)base->core)->pages[i]->sequence);
	}
}
#endif

static void create_page(base_t *base, pager_t *core) {
	struct _page super;
	char name[SHORT_QUID_LENGTH + 1];
	nullify(&super, sizeof(struct _page));

	page_t *page = (page_t *)tree_zcalloc(1, sizeof(page_t), core);
	quid_short_create(&page->page_key);
	quid_shorttostr(name, &page->page_key);
	page->sequence = base->pager.sequence++;
	page->fd = open(name, O_RDWR | O_TRUNC | O_CREAT | O_BINARY, 0644);
	if (page->fd < 0)
		return; //TODO err

	super.sequence = to_be32(page->sequence);
	if (write(page->fd, &super, sizeof(struct _page)) != sizeof(struct _page)) {
		lprint("[erro] Failed to write page item\n");
		return;
	}

	if (core->count >= core->allocated) {
		core->allocated += DEFAULT_PAGE_ALLOC;
		core->pages = (page_t **)tree_zrealloc(core->pages, core->allocated * sizeof(page_t *));
	}
	core->pages[core->count++] = page;
	base_list_add(base, &page->page_key);
}

static void open_page(quid_short_t *page_key, pager_t *core) {
	struct _page super;
	char name[SHORT_QUID_LENGTH + 1];
	nullify(&super, sizeof(struct _page));

	page_t *page = (page_t *)tree_zcalloc(1, sizeof(page_t), core);
	quid_shorttostr(name, page_key);
	page->fd = open(name, O_RDWR | O_BINARY);
	if (page->fd < 0)
		return; //TODO err

	if (read(page->fd, &super, sizeof(struct _page)) != sizeof(struct _page)) {
		lprint("[erro] Failed to read page item\n");
		return;
	}

	page->page_key = *page_key;
	page->sequence = from_be32(super.sequence);
	if (core->count >= core->allocated) {
		core->allocated += DEFAULT_PAGE_ALLOC;
		core->pages = (page_t **)tree_zrealloc(core->pages, core->allocated * sizeof(page_t *));
	}
	core->pages[core->count++] = page;
}

static void flush_page(page_t *page) {
	struct _page super;
	nullify(&super, sizeof(struct _page));

	super.sequence = to_be32(page->sequence);
	if (write(page->fd, &super, sizeof(struct _page)) != sizeof(struct _page)) {
		lprint("[erro] Failed to write page item\n");
		return;
	}
}

unsigned long long pager_alloc(base_t *base, size_t len) {
	if (!len)
		return 0; // thow err

	bool flush = FALSE;
	//len = page_align(len);
	unsigned long long page_size = MIN_PAGE_SIZE << base->pager.size;
	unsigned long long offset = base->pager.offset;

	/* Create new page */
	if ((offset % page_size) + len >= page_size) {
		create_page(base, (pager_t *)base->core);

		offset = ((((pager_t *)base->core)->count - 1) * page_size);
		offset += sizeof(struct _page);
		flush = TRUE;
	}

	base->pager.offset = offset + len;
	if (flush) {
		base_sync(base);
	}

	return offset;
}

int pager_get_fd(base_t *base, unsigned long long *offset) {
	unsigned long long page_size = MIN_PAGE_SIZE << base->pager.size;
	unsigned long long page = floor(*offset / page_size);

	if (page > (((pager_t *)base->core)->count - 1)) {
		return -1; // thow err
	}

	*offset %= page_size;
	return ((pager_t *)base->core)->pages[page]->fd;
}

unsigned int pager_get_sequence(base_t *base, unsigned long long offset) {
	unsigned long long page_size = MIN_PAGE_SIZE << base->pager.size;
	unsigned long long page = floor(offset / page_size);

	if (page > (((pager_t *)base->core)->count - 1)) {
		return -1; // thow err
	}

	return ((pager_t *)base->core)->pages[page]->sequence;
}

/*
 * Initialize all pages
 */
void pager_init(base_t *base) {
	struct _page_list list;
	nullify(&list, sizeof(struct _page_list));

	base->core = (pager_t *)tree_zcalloc(1, sizeof(pager_t), NULL);
	pager_t *page_core = (pager_t *)base->core;

	for (unsigned int i = 0; i <= base->page_list_count; ++i) {
		unsigned long offset = sizeof(struct _base) * (i + 1);
		if (lseek(base->fd, offset, SEEK_SET) < 0) {
			lprint("[erro] Failed to read \n");
			return;
		}
		if (read(base->fd, &list, sizeof(struct _page_list)) != sizeof(struct _page_list)) {
			lprint("[erro] Failed to read \n");
			return;
		}

		unsigned short list_size = from_be16(list.size);
		if (i == 0 && list_size == 0) {
			lprint("[info] Creating database heap\n");

			page_core->allocated = DEFAULT_PAGE_ALLOC;
			page_core->pages = (page_t **)tree_zcalloc(page_core->allocated, sizeof(page_t *), page_core);
			create_page(base, page_core);
			base->pager.offset = sizeof(struct _page);
			goto flush_base;
		} else {
			page_core->allocated = list_size < DEFAULT_PAGE_ALLOC ? DEFAULT_PAGE_ALLOC : list_size + DEFAULT_PAGE_ALLOC;
			page_core->pages = (page_t **)tree_zcalloc(page_core->allocated, sizeof(page_t *), page_core);
			for (unsigned short x = 0; x < list_size; ++x) {
				open_page(&list.item[x].page_key, page_core);
			}
		}
	}

flush_base:
	base_sync(base);
}

void pager_close(base_t *base) {
	for (unsigned int i = 0; i < ((pager_t *)base->core)->count; ++i) {
		flush_page(((pager_t *)base->core)->pages[i]);
		close(((pager_t *)base->core)->pages[i]->fd);
	}
	tree_zfree(base->core);
}
