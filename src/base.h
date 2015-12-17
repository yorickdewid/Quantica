#ifndef BASE_H_INCLUDED
#define BASE_H_INCLUDED

#include <config.h>
#include <common.h>
#include "quid.h"

#define DBNAME_SIZE		64
#define INSTANCE_LENGTH	32
#define BINDATA_LENGTH	16
#define MAGIC_LENGTH	10
#define PAGE_LIST_SIZE	10

struct _page_list_item {
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
	void *core;
	char instance_name[INSTANCE_LENGTH];
	bool lock;
	unsigned short version;
	int fd;
	char bindata[BINDATA_LENGTH];
	int bincnt;
	unsigned short page_sequence;
	unsigned long long page_offset;
	unsigned long long page_offset_free;
	unsigned short page_list_count;
	unsigned char page_size;
} base_t;

struct _base {
	char instance_name[INSTANCE_LENGTH];
	char bindata[BINDATA_LENGTH];	/* Will be obsolete when pager works*/
	char magic[MAGIC_LENGTH];
	quid_t instance_key;
	quid_t zero_key;
	__be16 version;
	__be32	bincnt;
	__be32 page_sequence;
	__be64 page_offset;
	__be64 page_offset_free;
	__be16 page_list_count;
	uint8_t page_size;
	uint8_t lock;
	uint8_t exitstatus;
} __attribute__((packed));

char *generate_bindata_name(struct base *base);

void base_list_add(base_t *base, quid_short_t *key);
void base_list_delete(base_t *base, quid_short_t *key);

void base_sync(base_t *base);
void base_init(base_t *base);
void base_close(base_t *base);

#endif // BASE_H_INCLUDED
