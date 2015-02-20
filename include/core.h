#ifndef CORE_H_INCLUDED
#define CORE_H_INCLUDED

#include "quid.h"
#include "engine.h"

struct stats {
	unsigned int commit;
};

void start_core();
int store(char *quid, const void *data, size_t len);
void *request(char *quid, size_t *len);
int test(char *param[]);
void debugstats();
int debugkey(char *quid);
int update(char *quid, struct microdata *nmd);
int delete(char *quid);
void detach_core();

#endif // CORE_H_INCLUDED
