#ifndef PAGE_H_INCLUDED
#define PAGE_H_INCLUDED

#include <config.h>
#include <common.h>
#include "base.h"
#include "quid.h"

#define MIN_PAGE_SIZE	4096 // 4 kb
#define PAGE_SIZE		10 // 4 Mb

struct _page {
	__be32 sequence;
} __attribute__((packed));

typedef struct page {
	unsigned int sequence;
	quid_short_t page_key;
	int fd;
} page_t;

typedef struct {
	unsigned int count;
	page_t **pages;
} pager_t;

void pager_init(base_t *base);
void pager_close(base_t *base);

#endif // PAGE_H_INCLUDED
