#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#include <config.h>
#include <common.h>
#include <log.h>
#include <error.h>
#include "basecontrol.h"

#define BASECONTROL		"base_control"

void base_sync(struct base *base) {
	struct base_super super;
	memset(&super, 0, sizeof(struct base_super));
	super.base_key = base->base_key;
	super.instance_key = base->instance_key;
	strlcpy(super.instance_name, base->instance_name, INSTANCE_LENGTH);

	lseek(base->fd, 0, SEEK_SET);
	if (write(base->fd, &super, sizeof(struct base_super)) != sizeof(struct base_super)) {
		lprintf("[erro] Failed to read " BASECONTROL "\n");
		ERROR(EIO_WRITE, EL_FATAL);
		return;
	}
}

void base_init(struct base *base) {
	memset(base, 0, sizeof(struct base));
	if(file_exists(BASECONTROL)) {
		base->fd = open(BASECONTROL, O_RDWR | O_BINARY);
		if (base->fd < 0)
			return;

		struct base_super super;
		if (read(base->fd, &super, sizeof(struct base_super)) != sizeof(struct base_super)) {
			lprintf("[erro] Failed to read " BASECONTROL "\n");
			ERROR(EIO_READ, EL_FATAL);
			return;
		}
		base->base_key = super.base_key;
		base->instance_key = super.instance_key;
		strlcpy(base->instance_name, super.instance_name, INSTANCE_LENGTH);
	} else {
		quid_create(&base->instance_key);
		quid_create(&base->base_key);
		strlcpy(base->instance_name, INSTANCE, INSTANCE_LENGTH);

		static char buf[QUID_LENGTH+1];
		quidtostr(buf, &base->base_key);

		base->fd = open(BASECONTROL, O_RDWR | O_TRUNC | O_CREAT | O_BINARY, 0644);
		base->base_fd = open(buf, O_RDWR | O_TRUNC | O_CREAT | O_BINARY, 0644);
		base_sync(base);
	}
}

void base_close(struct base *base) {
	base_sync(base);
	close(base->base_fd);
	close(base->fd);
}
