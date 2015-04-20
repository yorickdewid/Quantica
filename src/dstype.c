#include <config.h>
#include <common.h>

#include "zmalloc.h"
#include "dstype.h"

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
