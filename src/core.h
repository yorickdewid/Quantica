#ifndef CORE_H_INCLUDED
#define CORE_H_INCLUDED

#include "quid.h"
#include "time.h"
#include "engine.h"

struct record_status {
	char lifecycle[24];
	int importance;
	uint8_t syslock;
	uint8_t exec;
	uint8_t freeze;
	uint8_t error;
	char type[20];
};

/*
 * Core controll
 */
void start_core();
void detach_core();

void set_instance_name(char name[]);
char *get_instance_name();
char *get_uptime();
int crypto_sha1(char *s, const char *data);
unsigned long int stat_getkeys();
unsigned long int stat_getfreekeys();
void quid_generate(char *quid);

/*
 * Database operations
 */
int db_put(char *quid, const void *data, size_t len);
void *db_get(char *quid, size_t *len);
int db_update(char *quid, const void *data, size_t len);
int db_delete(char *quid);
int db_vacuum();

int db_record_get_meta(char *quid, struct record_status *status);

#endif // CORE_H_INCLUDED
