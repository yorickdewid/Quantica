#ifndef CORE_H_INCLUDED
#define CORE_H_INCLUDED

#include "quid.h"
#include "time.h"
#include "dstype.h"
#include "engine.h"

#define STATUS_LIFECYCLE_SIZE 10
#define STATUS_TYPE_SIZE 12

struct record_status {
	char lifecycle[STATUS_LIFECYCLE_SIZE];
	int importance;
	uint8_t syslock;
	uint8_t exec;
	uint8_t freeze;
	uint8_t error;
	char type[STATUS_TYPE_SIZE];
};

/*
 * Core control
 */
void start_core();
void detach_core();

void set_instance_name(char name[]);
char *get_instance_name();
char *get_instance_id();
char *get_uptime();
int crypto_sha1(char *s, const char *data);
int crypto_md5(char *s, const char *data);
int crypto_sha256(char *s, const char *data);
char *crypto_base64_enc(const char *data);
char *crypto_base64_dec(const char *data);
unsigned long int stat_getkeys();
unsigned long int stat_getfreekeys();
void quid_generate(char *quid);

/*
 * Database operations
 */
int db_put(char *quid, int *items, const void *data, size_t len);
void *_db_get(char *quid, dstype_t *dt);
void *db_get(char *quid);
char *db_get_type(char *quid);
int db_update(char *quid, const void *data, size_t len);
int db_delete(char *quid);
int db_purge(char *quid);
int db_vacuum();

int db_record_get_meta(char *quid, struct record_status *status);
int db_record_set_meta(char *quid, struct record_status *status);

#endif // CORE_H_INCLUDED
