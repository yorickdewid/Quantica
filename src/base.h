#ifndef BASE_H_INCLUDED
#define BASE_H_INCLUDED

#include <config.h>
#include <common.h>

#include "endian.h"
#include "quid.h"
#include "engine.h"

#define INSTANCE_LENGTH	32
#define PAGE_LIST_SIZE	10
#define MAGIC_LENGTH	10

#define BASE_PAGE_SIZE		4096 // 4 kb
#define MIN_PAGE_SIZE		0
#define MAX_PAGE_SIZE		19

enum exit_status {
	EXSTAT_ERROR,
	EXSTAT_INVALID,
	EXSTAT_CHECKPOINT,
	EXSTAT_SUCCESS
};

struct _page_list_item {
	quid_short_t page_key;
	char free;
	__be64 crc_sum;
} __attribute__((packed));

struct _page_list {
	struct _page_list_item item[PAGE_LIST_SIZE];
	__be16 size;
} __attribute__((packed));

typedef struct engine engine_t;
typedef struct pager pager_t;

typedef struct base {
	char instance_name[INSTANCE_LENGTH];
	quid_t instance_key;
	pager_t *core;		/* Pager */
	engine_t *engine;	/* Core engine */
	bool lock;
	unsigned short version;
	int fd;
	struct {
		unsigned short sequence;
		unsigned long long offset;
		unsigned char size;
	} pager;
	struct {
		unsigned long long zero;
		unsigned long long heap;
		unsigned long long alias;
		unsigned long long history;
		unsigned long long index_list;
	} offset;
	struct {
		unsigned long long zero_size;
		unsigned long long zero_free_size;
		unsigned long long heap_free_size;
		unsigned long long alias_size;
		unsigned long long index_list_size;
	} stats;
	unsigned short page_list_count;
} base_t;

struct _base {
	__be8 instance_name[INSTANCE_LENGTH];
	quid_t instance_key;
	__be8 magic[MAGIC_LENGTH];
	__be8 lock;
	__be8 exitstatus;
	__be16 version;
	struct {
		__be32 sequence;
		__be64 offset;
		__be8 size;
	} pager;
	struct {
		__be64 zero;
		__be64 heap;
		__be64 alias;
		__be64 history;
		__be64 index_list;
	} offset;
	struct {
		__be64 zero_size;
		__be64 zero_free_size;
		__be64 heap_free_size;
		__be64 alias_size;
		__be64 index_list_size;
	} stats;
	__be16 page_list_count;
} __attribute__((packed));

char *generate_bindata_name(base_t *base);

void base_list_set_crc_sum(base_t *base, quid_short_t *key, unsigned long long sum);
void base_list_add(base_t *base, quid_short_t *key);
void base_list_delete(base_t *base, quid_short_t *key);

void base_sync(base_t *base);
void base_lock(base_t *base);
void base_init(base_t *base, engine_t *engine);
void base_copy(base_t *base, base_t *new_base, engine_t *new_engine, unsigned char page_size);
void base_swap();
void base_close(base_t *base);

#endif // BASE_H_INCLUDED
