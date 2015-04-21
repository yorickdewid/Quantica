#include <ctype.h>
#include <config.h>
#include <common.h>

#include "zmalloc.h"
#include "dstype.h"

struct dsinfo {
	dstype_t ds;
	bool data;
};

static const struct dsinfo dsinfo[] = {
	{DT_NULL,	TRUE},
	{DT_BOOL_T,	TRUE},
	{DT_BOOL_F,	TRUE},
	{DT_INT,	FALSE},
	{DT_FLOAT,	FALSE},
	{DT_TEXT,	FALSE},
	{DT_CHAR,	FALSE},
	{DT_QUID,	FALSE},
	{DT_JSON,	FALSE}
};

bool isdata(dstype_t ds) {
	size_t nsz = RSIZE(dsinfo);
	while (nsz-->0) {
		if (dsinfo[nsz].ds == ds && dsinfo[nsz].data)
			return TRUE;
	}
	return FALSE;
}

dstype_t autotype(const void *data, size_t len) {
	if (!len)
		return DT_NULL;
	if (len == 1) {
		int fchar = (int)((char *)data)[0];
		switch (fchar) {
			case '0':
			case 'f':
			case 'F':
				return DT_BOOL_F;
			case '1':
			case 't':
			case 'T':
				return DT_BOOL_T;
		}
		if(isalpha(fchar))
			return DT_CHAR;
	}
    if(strisdigit((char *)data))
		return DT_INT;
    return DT_TEXT;
}

char *datatotype(dstype_t dt) {
	switch(dt) {
		case DT_NULL:
			return zstrdup(str_null());
		case DT_BOOL_T:
			return zstrdup(str_bool(TRUE));
		case DT_BOOL_F:
			return zstrdup(str_bool(FALSE));
		default:
			return NULL;
	}
	return NULL;
}

char *str_bool(bool b) {
	return b ? "TRUE" : "FALSE";
}

char *str_null() {
	return "NULL";
}
