#ifndef PAGE_H_INCLUDED
#define PAGE_H_INCLUDED

#include <config.h>
#include <common.h>
#include "base.h"
#include "quid.h"

#define zpalloc(b,s) \
	pager_alloc(b, s);

typedef struct base base_t;

typedef struct {
	unsigned int sequence;
	quid_short_t page_key;
	enum exit_status exit_status;
	int fd;
} page_t;

typedef struct pager {
	unsigned int count;
	unsigned int allocated;
	page_t **pages;
} pager_t;

uint64_t pager_alloc(base_t *base, size_t len);
int pager_get_fd(const base_t *base, uint64_t *offset);
unsigned int pager_get_sequence(base_t *base, uint64_t offset);
void pager_init(base_t *base);
void pager_sync(base_t *base);
void pager_close(base_t *base);
void pager_unlink_all(base_t *base);
size_t pager_total_disk_size(base_t *base);
marshall_t *pager_all(base_t *base);

#endif // PAGE_H_INCLUDED
