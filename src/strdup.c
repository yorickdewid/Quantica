#include <stdlib.h>
#include <string.h>

#include "zmalloc.h"

char *strdup(const char *str) {
	size_t size;
	char *copy;

	size = strlen(str) + 1;
	if ((copy = (char *)zmalloc(size)) == NULL) {
		return NULL;
	}
	memcpy(copy, str, size);

	return copy;
}
