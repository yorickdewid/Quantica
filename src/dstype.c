#include <string.h>
#include <ctype.h>
#include <config.h>
#include <common.h>

#include "zmalloc.h"
#include "json_check.h"
#include "quid.h"
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
#include <stdio.h>
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
	int8_t b = is_bool((char *)data);
	if (b!=-1)
		return b ? DT_BOOL_T : DT_BOOL_F;
	if (strisdigit((char *)data))
		return DT_INT;
	if (strquid_format((char *)data)>0)
		return DT_QUID;
	if (json_valid((char *)data))
		return DT_JSON;
    return DT_TEXT;
}

int8_t is_bool(char *str) {
	strtolower(str);
	if (!strcmp(str, "true"))
		return TRUE;
	if (!strcmp(str, "false"))
		return FALSE;
	return -1;
}

char *str_bool(bool b) {
	return b ? "true" : "false";
}

char *str_null() {
	return "null";
}

char *str_type(dstype_t dt) {
	switch(dt) {
		case DT_NULL:
			return "NULL";
		case DT_BOOL_T:
		case DT_BOOL_F:
			return "BOOLEAN";
		case DT_INT:
			return "INT";
		case DT_FLOAT:
			return "FLOAT";
		case DT_TEXT:
			return "TEXT";
		case DT_CHAR:
			return "CHAR";
		case DT_QUID:
			return "QUID";
		case DT_JSON:
			return "JSON";
		default:
			return "NULL";
	}
}
