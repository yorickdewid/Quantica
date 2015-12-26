#ifndef PAGE_H_INCLUDED
#define PAGE_H_INCLUDED

#include <config.h>
#include <common.h>
#include "base.h"
#include "quid.h"

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

unsigned long long pager_alloc(base_t *base, size_t len);
int pager_get_fd(const base_t *base, unsigned long long *offset);
unsigned int pager_get_sequence(base_t *base, unsigned long long offset);
void pager_init(base_t *base);
void pager_close(base_t *base);
marshall_t *pager_all(base_t *base);

#endif // PAGE_H_INCLUDED
