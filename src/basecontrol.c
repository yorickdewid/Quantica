#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#include <config.h>
#include <common.h>
#include <log.h>
#include <error.h>
#include "arc4random.h"
#include "diagnose.h"
#include "basecontrol.h"

#define BASECONTROL		"base"
#define INSTANCE_RANDOM	5
#define BASE_MAGIC		"$EOBCTRL$"

static enum {
	EXSTAT_ERROR,
	EXSTAT_INVALID,
	EXSTAT_CHECKPOINT,
	EXSTAT_SUCCESS
} exit_status;

static char *generate_instance_name() {
	static const char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	int i, len = INSTANCE_RANDOM;
	char strrand[INSTANCE_RANDOM];
	for (i = 0; i < len; ++i)
		strrand[i] = alphanum[arc4random() % (sizeof(alphanum) - 1)];
	strrand[len - 1] = 0;

	static char buf[INSTANCE_LENGTH];
	strlcpy(buf, INSTANCE_PREFIX, INSTANCE_LENGTH);
	strlcat(buf, "_", INSTANCE_LENGTH);
	strlcat(buf, strrand, INSTANCE_LENGTH);

	return buf;
}

char *generate_bindata_name(struct base *base) {
	static char buf[BINDATA_LENGTH];
	char *pdot = strchr(buf, '.');
	if (!pdot) {
		sprintf(buf, "%s.%d", base->bindata, ++base->bincnt);
	} else {
		char *k = buf;
		char *token = strsep(&k , ".");
		sprintf(buf, "%s.%d", token, ++base->bincnt);

	}
	return buf;
}

void base_sync(struct base *base) {
	struct base_super super;
	memset(&super, 0, sizeof(struct base_super));
	super.zero_key = base->zero_key;
	super.instance_key = base->instance_key;
	super.lock = base->lock;
	super.version = VERSION_RELESE;
	super.bincnt = base->bincnt;
	super.exitstatus = exit_status;
	strlcpy(super.instance_name, base->instance_name, INSTANCE_LENGTH);
	strlcpy(super.bindata, base->bindata, BINDATA_LENGTH);
	strlcpy(super.magic, BASE_MAGIC, MAGIC_LENGTH);

	if (lseek(base->fd, 0, SEEK_SET) < 0) {
		lprint("[erro] Failed to read " BASECONTROL "\n");
		return;
	}
	if (write(base->fd, &super, sizeof(struct base_super)) != sizeof(struct base_super)) {
		lprint("[erro] Failed to read " BASECONTROL "\n");
		return;
	}
}

void base_init(struct base *base) {
	memset(base, 0, sizeof(struct base));
	if (file_exists(BASECONTROL)) {

		/* Open existing database */
		base->fd = open(BASECONTROL, O_RDWR | O_BINARY);
		if (base->fd < 0)
			return;

		struct base_super super;
		memset(&super, 0, sizeof(struct base_super));
		if (read(base->fd, &super, sizeof(struct base_super)) != sizeof(struct base_super)) {
			lprint("[erro] Failed to read " BASECONTROL "\n");
			return;
		}
		base->zero_key = super.zero_key;
		base->instance_key = super.instance_key;
		base->lock = super.lock;
		base->bincnt = super.bincnt;
		super.instance_name[INSTANCE_LENGTH - 1] = '\0';
		super.bindata[BINDATA_LENGTH - 1] = '\0';
		strlcpy(base->instance_name, super.instance_name, INSTANCE_LENGTH);
		strlcpy(base->bindata, super.bindata, BINDATA_LENGTH);

		zassert(super.version == VERSION_RELESE);
		zassert(!strcmp(super.magic, BASE_MAGIC));
		if (super.exitstatus != EXSTAT_SUCCESS) {
			if (diag_exerr(base)) {
				exit_status = EXSTAT_CHECKPOINT;
			} else {
				exit(1);
			}
		}
		exit_status = EXSTAT_CHECKPOINT;
	} else {

		/* Create new database */
		quid_create(&base->instance_key);
		quid_create(&base->zero_key);
		base->bincnt = 0;
		exit_status = EXSTAT_INVALID;

		strlcpy(base->instance_name, generate_instance_name(), INSTANCE_LENGTH);
		strlcpy(base->bindata, BINDATA, BINDATA_LENGTH);
		base->fd = open(BASECONTROL, O_RDWR | O_TRUNC | O_CREAT | O_BINARY, 0644);
		base_sync(base);
		exit_status = EXSTAT_CHECKPOINT;
	}
}

void base_close(struct base *base) {
	exit_status = EXSTAT_SUCCESS;
	base_sync(base);
	close(base->fd);
	lprint("[info] Exist with EXSTAT_SUCCESS\n");
}
