#ifndef BASECONTROL_H_INCLUDED
#define BASECONTROL_H_INCLUDED

#include <config.h>
#include <common.h>
#include "quid.h"

#define DBNAME_SIZE		64
#define INSTANCE_LENGTH	32
#define BINDATA_LENGTH	16

struct base {
	quid_t instance_key;
	char instance_name[INSTANCE_LENGTH];
	quid_t zero_key;
	bool lock;
	uint16_t version;
	int fd;
	char bindata[BINDATA_LENGTH];
	int bincnt;
};

struct base_super {
	quid_t instance_key;
	char instance_name[INSTANCE_LENGTH];
	quid_t zero_key;
	bool lock;
	uint16_t version;
	char bindata[BINDATA_LENGTH];
	int bincnt;
};

char *generate_bindata_name(struct base *base);

void base_sync(struct base *base);
void base_init(struct base *base);
void base_close(struct base *base);

#endif // BASECONTROL_H_INCLUDED
