#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "zmalloc.h"

int zprintf(const char *fmt, ...) {
	va_list arg;
	int done;

	va_start(arg, fmt);
	done = vfprintf(stdout, fmt, arg);
	va_end(arg);

	return done;
}
