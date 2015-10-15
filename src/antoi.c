#include <stdlib.h>

/* Convert string to integer with length */
int antoi(const char *str, size_t num) {
	unsigned int i, res = 0;

	for (i = 0; i < num; ++i)
		res = res * 10 + str[i] - '0';

	return res;
}