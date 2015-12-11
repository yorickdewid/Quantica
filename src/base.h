#ifndef BASE_H_INCLUDED
#define BASE_H_INCLUDED

#include <config.h>
#include <common.h>
#include "quid.h"

#define DBNAME_SIZE		64
#define INSTANCE_LENGTH	32
#define BINDATA_LENGTH	16
#define MAGIC_LENGTH	10

typedef struct {
	unsigned long long offset;
	unsigned int pagesz;
	unsigned int pagecnt;
	int fd;
} pager_t;

typedef struct base {
	quid_t instance_key;
	quid_t zero_key;
	quid_t page_key;
	char instance_name[INSTANCE_LENGTH];
	bool lock;
	unsigned short version;
	int fd;
	char bindata[BINDATA_LENGTH];
	int bincnt;
} base_t;

struct _base {
	char instance_name[INSTANCE_LENGTH];
	char bindata[BINDATA_LENGTH];	/* Will be obsolete when pager works*/
	char magic[MAGIC_LENGTH];
	quid_t instance_key;
	quid_t zero_key;
	quid_t page_key;
	__be16 version;
	__be32	bincnt;
	__be64 page_sz;
	__be32 page_cnt;
	__be64 page_offset;
	uint8_t lock;
	uint8_t exitstatus;
} __attribute__((packed));

char *generate_bindata_name(struct base *base);

void base_sync(base_t *base, pager_t *core);
void base_init(base_t *base, pager_t *core);
void base_close(base_t *base, pager_t *core);

#endif // BASE_H_INCLUDED
