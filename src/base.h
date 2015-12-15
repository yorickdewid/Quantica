#ifndef BASE_H_INCLUDED
#define BASE_H_INCLUDED

#include <config.h>
#include <common.h>
#include "quid.h"
#include "pager.h"

#define DBNAME_SIZE		64
#define INSTANCE_LENGTH	32
#define BINDATA_LENGTH	16
#define MAGIC_LENGTH	10
#define PAGE_LIST_SIZE	4 //10

struct _page_list_item {
	__be32 sequence;
	quid_short_t page_key;
	char free;
} __attribute__((packed));

struct page_list {
	struct _page_list_item item[PAGE_LIST_SIZE];
	__be16 size;
};

typedef struct base {
	quid_t instance_key;
	quid_t zero_key;
	// pager_t core;
	char instance_name[INSTANCE_LENGTH];
	bool lock;
	unsigned short version;
	int fd;
	char bindata[BINDATA_LENGTH];
	unsigned int page_offset;
	int bincnt;
	unsigned int page_list_count;
} base_t;

struct _base {
	char instance_name[INSTANCE_LENGTH];
	char bindata[BINDATA_LENGTH];	/* Will be obsolete when pager works*/
	char magic[MAGIC_LENGTH];
	quid_t instance_key;
	quid_t zero_key;
	__be16 version;
	__be32	bincnt;
	__be16 page_list_count;
	// {
	__be64 page_sz;
	__be32 page_cnt;
	__be32 page_offset;
	// }
	uint8_t lock;
	uint8_t exitstatus;
} __attribute__((packed));

char *generate_bindata_name(struct base *base);

void base_sync(base_t *base);
void base_init(base_t *base);
void base_close(base_t *base);

#endif // BASE_H_INCLUDED
