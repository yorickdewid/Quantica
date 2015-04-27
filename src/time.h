#ifndef TIME_H_INCLUDED
#define TIME_H_INCLUDED

#include <time.h>

#define TIMENAME_SIZE 12

typedef long long int qtime_t;

qtime_t get_timestamp();
char *tstostrf(char *buf, size_t len, qtime_t ts, char *fmt);
qtime_t timetots(struct tm *t);
char *timename_now(char *str);

#endif // TIME_H_INCLUDED
