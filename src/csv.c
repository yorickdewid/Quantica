#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <common.h>
#include "csv.h"

const char *csv_getfield(csv_t *csv, char *line, vector_t *vector) {
	char *oline = line;

	int count = 0;
	for (; *line; ++line) {
		if (*line == csv->delimiter) {
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

size_t csv_getfieldcount(csv_t *csv, const char *line) {
	size_t count = 0;
	for (; *line; line++) {
		if (*line == csv->delimiter)
			count++;
	}

	return count;
}

bool csv_valid(csv_t *csv, const char *str) {
	int cnt = strccnt(str, '\n');
	if (!cnt)
		return FALSE;

	size_t fields = csv_getfieldcount(csv, str);
	if (!fields)
		return FALSE;

	return TRUE;
}
