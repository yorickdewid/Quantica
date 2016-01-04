#ifndef ERROR_H_INCLUDED
#define ERROR_H_INCLUDED

#include <config.h>
#include <common.h>

#define ERROR_CODE	14

/*
 * Trace error through global structure
 */
struct error {
	char error_squid[ERROR_CODE];
	char *description;
};

void error_clear();
bool iserror();
void error_throw(char *error_code, char *error_message);
void error_throw_fatal(char *error_code, char *error_message);
char *get_error_code();
char *get_error_description();

#endif // ERROR_H_INCLUDED
