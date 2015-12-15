#ifndef PAGE_H_INCLUDED
#define PAGE_H_INCLUDED

#include <config.h>
#include <common.h>
#include "base.h"
#include "quid.h"

#define PAGE_SIZE	(1024 * 1024 * 1024 * (unsigned long long)4) // 4 Mb

struct _page {
	__be32 sequence;
	quid_short_t page_key;
} __attribute__((packed));

typedef struct page {
	unsigned int sequence;
	quid_short_t page_key;
	int fd;
} page_t;

typedef struct {
	unsigned long long size;
	unsigned int count;
	unsigned int sequence;
	page_t **pages;
} pager_t;

void pager_init(base_t *base);
void pager_close(base_t *base);

#endif // PAGE_H_INCLUDED
