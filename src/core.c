#include <stdlib.h>
#include <string.h>

#include <config.h>
#include <common.h>
#include <log.h>
#include "quid.h"
#include "sha1.h"
#include "aes.h"
#include "crc32.h"
#include "base64.h"
#include "time.h"
#include "engine.h"
#include "bootstrap.h"
#include "core.h"

static struct engine btx;
static uint8_t ready = FALSE;
char ins_name[INSTANCE_LENGTH];
static qtime_t uptime;

void start_core() {
	start_log();
    set_instance_name(INSTANCE);
	engine_init(&btx, INITDB);
	bootstrap(&btx);
	uptime = get_timestamp();
	ready = TRUE;
}

void detach_core() {
	if (!ready)
		return;
	engine_close(&btx);
	stop_log();
	ready = FALSE;
}

void set_instance_name(char name[]) {
    if (strlen(name) > INSTANCE_LENGTH)
        return;

    strlcpy(ins_name, name, INSTANCE_LENGTH);
    ins_name[INSTANCE_LENGTH-1] = '\0';
}

char *get_instance_name() {
    return ins_name;
}

char *get_uptime(char *buf, size_t len) {
    qtime_t passed = get_timestamp()-uptime;
    unsigned int days = passed/86400;
    passed = passed % 86400;
    unsigned int hours = passed/3600;
    passed = passed % 3600;
    unsigned int mins = passed/60;
    passed = passed % 60;
    unsigned int secs = passed;
    snprintf(buf, len, "%u days, %.2u:%.2u:%.2u", days, hours, mins, secs);
    return buf;
}

int crypto_sha1(char *s, const char *data) {
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

void quid_generate(char *quid) {
	quid_t key;
	quid_create(&key);
	quidtostr(quid, &key);
}

int db_put(char *quid, const void *data, size_t len) {
	if (!ready)
		return -1;
	quid_t key;
	quid_create(&key);
	if (engine_insert(&btx, &key, data, len)<0)
		return -1;
	quidtostr(quid, &key);
	return 0;
}

void *db_get(char *quid, size_t *len) {
	if (!ready)
		return NULL;
	quid_t key;
	strtoquid(quid, &key);
	void *data = engine_get(&btx, &key, len);
	return data;
}

int db_update(char *quid, const void *data, size_t len) {
	if (!ready)
		return -1;
	quid_t key;
    strtoquid(quid, &key);
	if (engine_update(&btx, &key, data, len)<0) {
		return -1;
	}
	return 0;
}

#if 0
int db_update_(char *quid, struct microdata *nmd) {
	if (!ready)
		return -1;
	quid_t key;
	strtoquid(quid, &key);
	if (engine_set_meta(&btx, &key, nmd)<0)
		return -1;
	return 0;
}
#endif // 0

int db_delete(char *quid) {
	if (!ready)
		return -1;
	quid_t key;
	strtoquid(quid, &key);
	if (engine_delete(&btx, &key)<0)
		return -1;
	return 0;
}

int db_vacuum() {
	if (!ready)
		return -1;
	return engine_vacuum(&btx, INITDB);
}
