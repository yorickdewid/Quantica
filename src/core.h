#ifndef CORE_H_INCLUDED
#define CORE_H_INCLUDED

#include "quid.h"
#include "time.h"
#include "engine.h"

#define INSTANCE_LENGTH 32

struct stats {
	unsigned int commit;
};

/*
 * Core controll
 */
void start_core();
void detach_core();

void set_instance_name(char name[]);
char *get_instance_name();
char *get_uptime(char *buf, size_t len);
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
#if 0
//int db_update_(char *quid, struct microdata *nmd);
#endif // 0
int db_delete(char *quid);
int db_vacuum();

#endif // CORE_H_INCLUDED
