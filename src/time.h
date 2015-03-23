#ifndef TIME_H_INCLUDED
#define TIME_H_INCLUDED

#include <time.h>

typedef long long int qtime_t;

qtime_t get_timestamp();
char *tstostrf(char *buf, size_t len, qtime_t ts, char *fmt);
qtime_t timetots(struct tm *t);

#endif // TIME_H_INCLUDED
