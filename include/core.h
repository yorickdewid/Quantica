#ifndef CORE_H_INCLUDED
#define CORE_H_INCLUDED

#include "quid.h"
#include "engine.h"

#define INSTANCE_LENGTH 32

struct stats {
	unsigned int commit;
};

void start_core();
int store(char *quid, const void *data, size_t len);
void *request_quid(char *quid, size_t *len);
unsigned long int stat_getkeys();
unsigned long int stat_getfreekeys();
int test(char *param[]);
void debugstats();
void set_instance_name(char name[]);
char *get_instance_name();
void generate_quid(char *quid);
int debugkey(char *quid);
int sha1(char *s, const char *data);
int update(char *quid, struct microdata *nmd);
int delete(char *quid);
int vacuum();
void detach_core();

#endif // CORE_H_INCLUDED
