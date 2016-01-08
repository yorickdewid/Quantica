#ifndef CSV_H_INCLUDED
#define CSV_H_INCLUDED

#include "vector.h"

#define CSV_DEFAULT_DELIMITER	';'

const char *csv_getfield(char *line, vector_t *vector);
size_t csv_getfieldcount(const char *line);
bool csv_valid(const char *str);

#endif // CSV_H_INCLUDED
