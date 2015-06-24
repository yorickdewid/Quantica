#ifndef SQL_H_INCLUDED
#define SQL_H_INCLUDED

#include "quid.h"

typedef struct {
	quid_t quid;
	char *str;
} sqlresult_t;

void *sql_exec(const char *sql, size_t *len);

#endif // SQL_H_INCLUDED
