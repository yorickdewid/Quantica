#ifndef DSTYPE_H_INCLUDED
#define DSTYPE_H_INCLUDED

#include <common.h>

typedef enum {
	DT_NULL,
	DT_BOOL_T,
	DT_BOOL_F,
	DT_INT,
	DT_FLOAT,
	DT_TEXT,
	DT_CHAR,
	DT_QUID,
	DT_JSON
} dstype_t;

bool isdata(dstype_t ds);
dstype_t autotype(const char *data, size_t len);
char *datatotype(dstype_t dt);
char *str_bool(bool b);
char *str_null();
char *str_type(dstype_t dt);

#endif // DSTYPE_H_INCLUDED
