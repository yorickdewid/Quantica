#include <stdio.h>
#include <string.h>

#include <config.h>
#include <common.h>
#include <log.h>

#include "quid.h"
#include "zmalloc.h"
#include "error.h"

static struct error stack;

void error_clear() {
	memset(&stack.error_squid, 0, 12 + 1);
	if (stack.description) {
		zfree(stack.description);
		stack.description = NULL;
	}
}

bool iserror() {
	return (stack.description ? TRUE : FALSE);
}

void error_throw(char *error_code, char *error_message) {
	error_clear();

	strncpy(stack.error_squid, error_code, ERROR_CODE);
	stack.description = (char *)zstrdup(error_message);
}

void error_throw_fatal(char *error_code, char *error_message) {
	error_clear();

	strncpy(stack.error_squid, error_code, ERROR_CODE);
	stack.description = (char *)zstrdup(error_message);
	lprintf("[erro] %s\n", error_message);
}

char *get_error_code() {
	return stack.error_squid;
}

char *get_error_description() {
	return stack.description;
}
