#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <math.h>

#include <config.h>
#include "time.h"

#define DAYS45_70 (25 * 365 + 6)
#define DAYS1601_1970 134774
#define NTIMENAME (31 * 36 * 36 * 36)
#define EPOCH_DIFF 1262304000

long long get_timestamp() {
	long long ts;
	struct timeval tv;
	gettimeofday(&tv, NULL);
	ts = tv.tv_sec - EPOCH_DIFF;
	return ts;
}

long long get_unixtimestamp() {
	long long ts;
	struct timeval tv;
	gettimeofday(&tv, NULL);
	ts = tv.tv_sec;
	return ts;
}

char *tstostrf(char *buf, size_t len, long long ts, char *fmt) {
	time_t now = ts + EPOCH_DIFF;
	struct tm uts = *localtime(&now);
	strftime(buf, len, fmt, &uts);
	return buf;
}

char *unixtostrf(char *buf, size_t len, long long ts, char *fmt) {
	struct tm tm_ts = *localtime((time_t *)&ts);
	strftime(buf, len, fmt, &tm_ts);
	return buf;
}

long long timetots(struct tm *t) {
	return (long long)mktime(t) - EPOCH_DIFF;
}

static void current_time(int *days, int *secs, int *nanosecs) {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	*days = tv.tv_sec / 86400;
	*secs = tv.tv_sec % 86400;
	*nanosecs = tv.tv_usec * 1000;
}

static void timename(int days, int secs, int nanosecs, char *str) {
	double ntime;
	ldiv_t d;
	int i;

	ntime = (double)secs * 1E9 + (double)nanosecs;
	ntime = (ntime * (36.0 * 36.0 * 36.0 * 36.0 * 36.0 * 36.0 * 36.0 * 36.0)) / 86400E9;
	ntime = floor(ntime + 0.5);
	for (i = 11; i >= 4; i--) {
		double _d = floor(ntime / 36.0);
		int rem = (int)(floor(ntime - (_d * 36.0)));
		str[i] = rem < 10 ? '0' + rem : 'a' + rem - 10;
		ntime = _d;
	}

	d.quot = days + DAYS45_70 + NTIMENAME;
	for (i = 4; i--;) {
		d = ldiv(d.quot, 36);
		str[i] = d.rem < 10 ? '0' + (char)d.rem : 'a' + (char)d.rem - 10;
	}
}

char *timename_now(char *str) {
	int days, secs, nanosecs;

	current_time(&days, &secs, &nanosecs);
	timename(days, secs, nanosecs, str);

	return str;
}
