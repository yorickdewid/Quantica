#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <fcntl.h>
#include <string.h>

#include <config.h>
#include <common.h>
#include "log.h"
#include "zmalloc.h"
#include "base.h"
#include "pager.h"

#define DEFAULT_PAGE_ALLOC	10

static void create_page(base_t *base, pager_t *core) {
	struct _page super;
	char name[SHORT_QUID_LENGTH + 1];
	nullify(&super, sizeof(struct _page));

	page_t *page = (page_t *)tree_zcalloc(1, sizeof(page_t), core);
	quid_short_create(&page->page_key);
	quid_shorttostr(name, &page->page_key);
	page->sequence = core->sequence++;
	page->fd = open(name, O_RDWR | O_TRUNC | O_CREAT | O_BINARY, 0644);
	if (page->fd < 0)
		return; //TODO err

	super.sequence = to_be32(page->sequence);
	super.page_key = page->page_key;

	if (write(page->fd, &super, sizeof(struct _page)) < 0) {
		lprint("[erro] Failed to write page item\n");
		return;
	}

	core->pages = tree_zmalloc(DEFAULT_PAGE_ALLOC, core);
	core->pages[core->count++] = page;
	base_list_add(base, page->sequence, &page->page_key);
}

/*
 * Initialize all pages
 */
void pager_init(base_t *base) {
	struct page_list list;
	nullify(&list, sizeof(struct page_list));

	base->core = (pager_t *)tree_zcalloc(1, sizeof(pager_t), NULL);
	pager_t *page_core = (pager_t *)base->core;

	for (unsigned int i = 0; i <= base->page_list_count; ++i) {
		unsigned long offset = sizeof(struct _base) * (i + 1);
		if (lseek(base->fd, offset, SEEK_SET) < 0) {
			lprint("[erro] Failed to read \n");
			return;
		}
		if (read(base->fd, &list, sizeof(struct page_list)) != sizeof(struct page_list)) {
			lprint("[erro] Failed to read \n");
			return;
		}

		unsigned short list_size = from_be16(list.size);
		if (i == 0 && list_size == 0) {
			lprint("[info] Creating database heap\n");

			page_core->sequence = 1;
			page_core->size = PAGE_SIZE;
			create_page(base, page_core);
		} else {
			// read pages
			/*for (unsigned short x = 0; x < from_be16(list.size); ++x) {
				char name[SHORT_QUID_LENGTH + 1];
				quid_shorttostr(name, &list.item[x].page_key);

			}*/
		}
	}
}

void pager_close(base_t *base) {
	// flush page by page
	for (unsigned int i = 0; i < ((pager_t *)base->core)->count; ++i) {
		close(((pager_t *)base->core)->pages[i]->fd);
	}
	tree_zfree(base->core);
}
