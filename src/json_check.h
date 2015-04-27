#ifndef JSON_CHECK_H_INCLUDED
#define JSON_CHECK_H_INCLUDED

#include <common.h>

#define JSON_CHECK_DEPTH	15

typedef struct {
	int state;
	int depth;
	int top;
	int *stack;
} json_check_t;

json_check_t *new_json_check(int depth);
int json_check_char(json_check_t *jc, int next_char);
int json_check_done(json_check_t *jc);
bool json_valid(const char *json);

#endif /* JSON_CHECK_H_INCLUDED */
