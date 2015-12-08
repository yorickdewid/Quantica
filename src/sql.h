#ifndef SQL_H_INCLUDED
#define SQL_H_INCLUDED

#include "quid.h"

typedef struct {
	char quid[QUID_LENGTH + 1];
	void *data;
	int items;
	char *name;
} sqlresult_t;

sqlresult_t *sql_exec(const char *sql, size_t *len);

#endif // SQL_H_INCLUDED
