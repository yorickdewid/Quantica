#ifndef LOG_H_INCLUDED
#define LOG_H_INCLUDED

void start_log();
void lprintf(const char *format, ...);
void lprint(const char *str);
void stop_log();

#endif // LOG_H_INCLUDED
