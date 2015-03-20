#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>

#include <config.h>
#include "time.h"

#define EPOCH_DIFF 1262304000

qtime_t get_timestamp() {
    qtime_t ts;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    ts = tv.tv_sec - EPOCH_DIFF;
    return ts;
}

char *tstostrf(char *buf, size_t len, qtime_t ts, char *fmt) {
    time_t now = ts + EPOCH_DIFF;
    struct tm uts = *localtime(&now);
    strftime(buf, len, fmt, &uts);
    return buf;
}

qtime_t timetots(struct tm *t) {
    return (qtime_t)mktime(t) - EPOCH_DIFF;
}
