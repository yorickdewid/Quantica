#include <fcntl.h>
#include <string.h>

#include <config.h>
#include <common.h>
#include "pager.h"

static int pager_create(pager_t *core, const char *name) {
	nullify(core, sizeof(pager_t));
	core->fd = open(name, O_RDWR | O_TRUNC | O_CREAT | O_BINARY, 0644);
	if (core->fd < 0)
		return -1;

	return 0;
}

void pager_init(pager_t *core, const char *name) {
	//if (file_exists(name)) {
	//	pager_open(core, name);
	//} else {
	pager_create(core, name);
	//}
}

void pager_close(pager_t *core) {
	// flush here
	close(core->fd);
}
