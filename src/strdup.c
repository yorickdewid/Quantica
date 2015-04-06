#include <stdlib.h>
#include <string.h>

char *strdup(const char *str) {
	size_t size;
	char *copy;

	size = strlen(str) + 1;
	if ((copy = malloc(size)) == NULL) {
		return NULL;
	}
	memcpy(copy, str, size);

	return copy;
}
