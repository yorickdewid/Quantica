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

char *strndup(const char *str, size_t n) {
	char *copy;

	if ((copy = (char *)zmalloc(n + 1)) == NULL) {
		return NULL;
	}
	memcpy(copy, str, n);
	copy[n] = '\0';

	return copy;
}
