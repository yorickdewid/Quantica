#ifndef PAGE_H_INCLUDED
#define PAGE_H_INCLUDED

#include <config.h>
#include <common.h>
#include "quid.h"
#include "base.h"

struct _page {
	__be32 version;						/* Version */
	__be32 sequence;					/* Sequence number */
	char next[SHORT_QUID_LENGTH];		/* Pointer to next page */
} __attribute__((packed));

typedef struct page {
	struct page *next;
} page_t;

void pager_init(pager_t *core, const char *name);

#endif // PAGE_H_INCLUDED