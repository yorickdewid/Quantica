#include <stdlib.h>
#include <string.h>

#include <config.h>
#include <common.h>
#include "quid.h"
#include "sha1.h"
#include "aes.h"
#include "crc32.h"
#include "base64.h"
#include "engine.h"
#include "bootstrap.h"
#include "core.h"

static struct btree btx;
static uint8_t ready = FALSE;
char ins_name[INSTANCE_LENGTH];

void start_core() {
    memset(ins_name, 0, INSTANCE_LENGTH);
    set_instance_name("DEVSRV1");
	btree_init(&btx, INITDB);
	bootstrap(&btx);
	ready = TRUE;
}

void set_instance_name(char name[]) {
    if (strlen(name) > INSTANCE_LENGTH)
        return;

    strcpy(ins_name, name);
    ins_name[INSTANCE_LENGTH-1] = '\0';
}

char *get_instance_name() {
    return ins_name;
}

int store(char *quid, const void *data, size_t len) {
	if (!ready)
		return -1;
	quid_t key;
	quid_create(&key);
	if (btree_insert(&btx, &key, data, len)<0)
		return -1;
	quidtostr(quid, &key);
	return 0;
}

int test(void *param[]) {
	puts("Test stub");
	(void)(param);
	return 0;
}

int update_key(char *quid, const void *data, size_t len) {
	if (!ready)
		return -1;
	quid_t key;
    strtoquid(quid, &key);
	if (btree_update(&btx, &key, data, len)<0) {
		return -1;
	}
	return 0;
}

int sha1(char *s, const char *data) {
    struct sha sha;
    sha1_reset(&sha);
    sha1_input(&sha, (const unsigned char *)data, strlen(data));

    if (!sha1_result(&sha)) {
        return 0;
    }
    sha1_strsum(s, &sha);
    return 1;
}

unsigned long int stat_getkeys() {
    return btx.stats.keys;
}

unsigned long int stat_getfreekeys() {
    return btx.stats.free_tables;
}

void generate_quid(char *quid) {
	quid_t key;
	quid_create(&key);
	quidtostr(quid, &key);
}

void *request_quid(char *quid, size_t *len) {
	if (!ready)
		return NULL;
	quid_t key;
	strtoquid(quid, &key);
	void *data = btree_get(&btx, &key, len);
	return data;
}

int update(char *quid, struct microdata *nmd) {
	if (!ready)
		return -1;
	quid_t key;
	strtoquid(quid, &key);
	if (btree_meta(&btx, &key, nmd)<0)
		return -1;
	return 0;
}

int delete(char *quid) {
	if (!ready)
		return -1;
	quid_t key;
	strtoquid(quid, &key);
	if (btree_delete(&btx, &key)<0)
		return -1;
	return 0;
}

int vacuum() {
	if (!ready)
		return -1;
	return btree_vacuum(&btx, INITDB);
}

void detach_core() {
	if (!ready)
		return;
	btree_close(&btx);
	ready = FALSE;
}
