#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <common.h>
#include "csv.h"

const char *csv_getfield(char *line, vector_t *vector) {
	char *oline = line;

	int count = 0;
	for (; *line; ++line) {
		if (*line == CSV_DEFAULT_DELIMITER) {
			*line = '\0';
			vector_append(vector, (void *)oline);

			oline = line + 1;
			count++;
		}
	}

	if (vector->size > 0)
		vector_append(vector, (void *)oline);

	return NULL;
}

size_t csv_getfieldcount(const char *line) {
	size_t count = 0;
	for (; *line; line++) {
		if (*line == CSV_DEFAULT_DELIMITER)
			count++;
	}

	return count;
}

bool csv_valid(const char *str) {
	int cnt = strccnt(str, '\n');
	if (!cnt)
		return FALSE;

	size_t fields = csv_getfieldcount(str);
	if (!fields)
		return FALSE;

	return TRUE;
}
