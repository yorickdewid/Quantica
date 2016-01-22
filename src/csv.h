#ifndef CSV_H_INCLUDED
#define CSV_H_INCLUDED

#include "vector.h"

#define CSV_DEFAULT_DELIMITER	';'

typedef struct {
	char delimiter;
	bool header;
} csv_t;

const char *csv_getfield(csv_t *csv, char *line, vector_t *vector);
size_t csv_getfieldcount(csv_t *csv, const char *line);
bool csv_valid(csv_t *csv, const char *str);

#endif // CSV_H_INCLUDED
