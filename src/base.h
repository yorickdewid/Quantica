#ifndef BASECONTROL_H_INCLUDED
#define BASECONTROL_H_INCLUDED

#include <config.h>
#include <common.h>
#include "quid.h"

#define DBNAME_SIZE		64
#define INSTANCE_LENGTH	32
#define BINDATA_LENGTH	16
#define MAGIC_LENGTH	10

struct base {
	quid_t instance_key;
	char instance_name[INSTANCE_LENGTH];
	quid_t zero_key;
	bool lock;
	unsigned short version;
	int fd;
	char bindata[BINDATA_LENGTH];
	int bincnt;
};

struct base_super {
	char instance_name[INSTANCE_LENGTH];
	char bindata[BINDATA_LENGTH];
	char magic[MAGIC_LENGTH];
	quid_t instance_key;
	quid_t zero_key;
	__be16 version;
	__be32	bincnt;
	uint8_t lock;
	uint8_t exitstatus;
} __attribute__((packed));

char *generate_bindata_name(struct base *base);

void base_sync(struct base *base);
void base_init(struct base *base);
void base_close(struct base *base);

#endif // BASECONTROL_H_INCLUDED
