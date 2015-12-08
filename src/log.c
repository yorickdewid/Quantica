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
		if (fp)
			setvbuf(fp, NULL, _IOLBF, 1024);
		else {
			fputs("[warn] Failed to open log\n", stderr);
			fputs("[info] Continue output on stderr\n", stderr);
		}
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

void lprint(const char *str) {
	if (fp) {
		char buf[32];
		fprintf(fp, "[%s] ", tstostrf(buf, 32, get_timestamp(), "%d/%b/%Y %H:%M:%S %z"));
		fputs(str, fp);
	}

	fputs(str, stderr);
}

void lprinterr(const char *str) {
	if (fp) {
		char buf[32];
		fprintf(fp, "[%s] ", tstostrf(buf, 32, get_timestamp(), "%d/%b/%Y %H:%M:%S %z"));
		fputs(str, fp);
	}

	fputs(str, stderr);
}

void stop_log() {
	if (fp)
		fclose(fp);
}
