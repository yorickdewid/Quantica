#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <common.h>

size_t csv_getfieldcount(const char *line) {
	size_t count = 0;
	for (; *line; line++) {
		if (*line == ';')
			count++;
	}

	return count;
}

bool csv_valid(char *str) {
	int cnt = strccnt(str, '\n');
	if (!cnt)
		return FALSE;

	size_t fields = csv_getfieldcount(str);
	if (!fields)
		return FALSE;

	return TRUE;
}
