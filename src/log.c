#include <stdio.h>
#include <stdarg.h>

#include <config.h>
#include <common.h>
#include <log.h>

#include "time.h"

static FILE *fp = NULL;

void start_log() {
	if (!fp) {
		fp = fopen(LOGFILE, "a");
		setvbuf(fp, NULL, _IOLBF, 1024);
	}
}

void lprintf(const char *format, ...) {
	va_list arglist;

	if (fp) {
		char buf[32];
		fprintf(fp, "[%s] ", tstostrf(buf, 32, get_timestamp(), "%d/%b/%Y %H:%M:%S %z"));
		va_start(arglist, format);
		vfprintf(fp, format, arglist);
		va_end(arglist);
	}

	va_start(arglist, format);
	vfprintf(stderr, format, arglist);
	va_end(arglist);
}

void stop_log() {
	if (fp)
		fclose(fp);
}
