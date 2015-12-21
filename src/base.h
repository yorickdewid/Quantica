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

#define MIN_PAGE_SIZE		4096 // 4 kb
#define DEFAULT_PAGE_SIZE	10 // 4 Mb

struct _page_list_item {
	quid_short_t page_key;
	char free;
} __attribute__((packed));

struct _page_list {
	struct _page_list_item item[PAGE_LIST_SIZE];
	__be16 size;
} __attribute__((packed));

typedef struct {
	quid_t instance_key;
	quid_t zero_key;
	void *core;
	char instance_name[INSTANCE_LENGTH];
	bool lock;
	unsigned short version;
	int fd;
	char bindata[BINDATA_LENGTH];
	int bincnt;
	struct {
		unsigned short sequence;
		unsigned long long offset;
		// unsigned long long offset_free;
		unsigned char size;
	} pager;
	struct {
		unsigned long index;
		unsigned long heap;
		unsigned long alias;
		unsigned long index_list;
	} offset;
	struct {
		unsigned long alias_size;
		unsigned long index_list_size;
	} stats;
	unsigned short page_list_count;
} base_t;

struct _base {
	char instance_name[INSTANCE_LENGTH];
	char bindata[BINDATA_LENGTH];	/* Will be obsolete when pager works*/
	char magic[MAGIC_LENGTH];
	quid_t instance_key;
	quid_t zero_key;
	__be16 version;
	__be32	bincnt;
	struct {
		__be32 sequence;
		__be64 offset;
		// __be64 offset_free;
		uint8_t size;
	} pager;
	struct {
		__be64 index;
		__be64 heap;
		__be64 alias;
		__be64 index_list;
	} offset;
	struct {
		__be64 alias_size;
		__be64 index_list_size;
	} stats;
	__be16 page_list_count;
	uint8_t lock;
	uint8_t exitstatus;
} __attribute__((packed));

char *generate_bindata_name(base_t *base);

void base_list_add(base_t *base, quid_short_t *key);
void base_list_delete(base_t *base, quid_short_t *key);

void base_sync(base_t *base);
void base_init(base_t *base);
void base_close(base_t *base);

#endif // BASE_H_INCLUDED
