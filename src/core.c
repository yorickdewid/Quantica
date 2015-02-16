#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "quid.h"
#include "engine.h"

static struct btree btx;
static uint8_t ready = 0;

void start_core() {
	btree_init(&btx, INITDB);
	ready = 1;
}

int store(char *quid, const void *data, size_t len) {
	if (!ready)
		return -1;
	struct quid key;
	quid_create(&key);
	if (btree_insert(&btx, &key, data, len)<0)
		return -1;
	quidtostr(quid, &key);
	return 0;
}

int test(char *param[]) {
	puts("Test stub");
	return 0;
}

void *request(char *quid, size_t *len) {
	if (!ready)
		return NULL;
	struct quid key;
	strtoquid(quid, &key);
	void *data = btree_get(&btx, &key, len);
	return data;
}

int delete(char *quid) {
	if (!ready)
		return -1;
	struct quid key;
	strtoquid(quid, &key);
	if (btree_delete(&btx, &key)<0)
		return -1;
	return 0;
}

void detach_core() {
	if (!ready)
		return;
	btree_close(&btx);
	ready = 0;
}
