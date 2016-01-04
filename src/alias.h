#ifndef ALIAS_H_INCLUDED
#define ALIAS_H_INCLUDED

#include <string.h>
#include <unistd.h>

#include "marshall.h"

#define ALIAS_NAME_LENGTH 48

int alias_add(base_t *base, const quid_t *c_quid, const char *c_name, size_t len);
char *alias_get_val(base_t *base, const quid_t *c_quid);
int alias_get_key(base_t *base, quid_t *key, const char *name, size_t len);
int alias_update(base_t *base, const quid_t *c_quid, const char *name, size_t len);
int alias_delete(base_t *base, const quid_t *c_quid);
marshall_t *alias_all(base_t *base);
void alias_rebuild(base_t *base, base_t *new_base);

#endif // ALIAS_H_INCLUDED
