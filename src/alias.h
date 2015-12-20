#include <string.h>
#include <unistd.h>

#define ALIAS_NAME_LENGTH 48

int alias_add(base_t *base, const quid_t *c_quid, const char *c_name, size_t len);
