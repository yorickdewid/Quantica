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
#include "pager.h"
#include "base.h"

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
	int len = INSTANCE_RANDOM;
	char strrand[INSTANCE_RANDOM];
	for (int i = 0; i < len; ++i)
		strrand[i] = alphanum[arc4random() % (sizeof(alphanum) - 1)];
	strrand[len - 1] = 0;

	static char buf[INSTANCE_LENGTH];
	strlcpy(buf, INSTANCE_PREFIX, INSTANCE_LENGTH);
	strlcat(buf, "_", INSTANCE_LENGTH);
	strlcat(buf, strrand, INSTANCE_LENGTH);

	return buf;
}

char *generate_bindata_name(base_t *base) {
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

void add_page_item(base_t *base, pager_t *core) {
	struct _page_list_item item;
	nullify(&item, sizeof(struct _page_list_item));

	item.sequence = to_be32(++core->pagecnt);
	item.next = 0;
	quid_short_create(&item.page_key);

	if (lseek(base->fd, base->page_offset, SEEK_SET) < 0) {
		lprint("[erro] Failed to read " BASECONTROL "\n");
		return;
	}
	if (write(base->fd, &item, sizeof(struct _page_list_item)) != sizeof(struct _page_list_item)) {
		lprint("[erro] Failed to write page item\n");
		return;
	}
	base->page_offset += sizeof(struct _page_list_item);
}

void base_sync(base_t *base, pager_t *core) {
	struct _base super;
	nullify(&super, sizeof(struct _base));
	super.zero_key = base->zero_key;
	super.instance_key = base->instance_key;
	super.lock = base->lock;
	super.version = to_be16(VERSION_MAJOR);
	super.bincnt = to_be32(base->bincnt);
	super.exitstatus = exit_status;
	super.page_sz = to_be64(core->pagesz);
	super.page_cnt = to_be32(core->pagecnt);
	super.page_offset = to_be32(base->page_offset);

	strlcpy(super.instance_name, base->instance_name, INSTANCE_LENGTH);
	strlcpy(super.bindata, base->bindata, BINDATA_LENGTH);
	strlcpy(super.magic, BASE_MAGIC, MAGIC_LENGTH);

	if (lseek(base->fd, 0, SEEK_SET) < 0) {
		lprint("[erro] Failed to read " BASECONTROL "\n");
		return;
	}
	if (write(base->fd, &super, sizeof(struct _base)) != sizeof(struct _base)) {
		lprint("[erro] Failed to write " BASECONTROL "\n");
		return;
	}
}

void base_init(base_t *base, pager_t *core) {
	nullify(base, sizeof(base_t));
	nullify(core, sizeof(pager_t));
	if (file_exists(BASECONTROL)) {

		/* Open existing database */
		base->fd = open(BASECONTROL, O_RDWR | O_BINARY);
		if (base->fd < 0)
			return;

		struct _base super;
		nullify(&super, sizeof(struct _base));
		if (read(base->fd, &super, sizeof(struct _base)) != sizeof(struct _base)) {
			lprint("[erro] Failed to read " BASECONTROL "\n");
			return;
		}

		base->zero_key = super.zero_key;
		base->instance_key = super.instance_key;
		base->lock = super.lock;
		base->bincnt = from_be32(super.bincnt);
		core->pagesz = from_be64(super.page_sz);
		core->pagecnt = from_be32(super.page_cnt);
		base->page_offset = from_be32(super.page_offset);
		super.instance_name[INSTANCE_LENGTH - 1] = '\0';
		super.bindata[BINDATA_LENGTH - 1] = '\0';
		strlcpy(base->instance_name, super.instance_name, INSTANCE_LENGTH);
		strlcpy(base->bindata, super.bindata, BINDATA_LENGTH);

		zassert(from_be16(super.version) == VERSION_MAJOR);
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
		base->page_offset = sizeof(struct _base);
		exit_status = EXSTAT_INVALID;

		strlcpy(base->instance_name, generate_instance_name(), INSTANCE_LENGTH);
		strlcpy(base->bindata, BINDATA, BINDATA_LENGTH);
		base->fd = open(BASECONTROL, O_RDWR | O_TRUNC | O_CREAT | O_BINARY, 0644);
		if (base->fd < 0) {
			lprint("[erro] Failed to read " BASECONTROL "\n");
			return;
		}

		base_sync(base, core);

		add_page_item(base, core);

		exit_status = EXSTAT_CHECKPOINT;
	}
}

void base_close(base_t *base, pager_t *core) {
	exit_status = EXSTAT_SUCCESS;
	base_sync(base, core);
	close(base->fd);
	lprint("[info] Exist with EXSTAT_SUCCESS\n");
}
