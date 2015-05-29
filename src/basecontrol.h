#ifndef BASECONTROL_H_INCLUDED
#define BASECONTROL_H_INCLUDED

#include <config.h>
#include <common.h>
#include "quid.h"

#define DBNAME_SIZE	64
#define INSTANCE_LENGTH 32

struct base {
	quid_t instance_key;
	char instance_name[INSTANCE_LENGTH];
	quid_t base_key;
	int fd;
	int base_fd;
};

struct base_super {
	quid_t instance_key;
	char instance_name[INSTANCE_LENGTH];
	quid_t base_key;
};

void base_sync(struct base *base);
void base_init(struct base *base);
void base_close(struct base *base);

#endif // BASECONTROL_H_INCLUDED
