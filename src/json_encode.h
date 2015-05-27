#ifndef JSON_ENCODE_H_INCLUDED
#define JSON_ENCODE_H_INCLUDED

#include <config.h>
#include <common.h>

#include "json_parse.h"

#define json_serialize_mode_multiline		0
#define json_serialize_mode_single_line		1
#define json_serialize_mode_packed			2

#define json_serialize_opt_CRLF						(1 << 1)
#define json_serialize_opt_pack_brackets			(1 << 2)
#define json_serialize_opt_no_space_after_comma		(1 << 3)
#define json_serialize_opt_no_space_after_colon		(1 << 4)
#define json_serialize_opt_use_tabs					(1 << 5)

extern const size_t json_builder_extra;

json_value *json_array_new(size_t length);

json_value *json_array_push(json_value *array, json_value *);

json_value *json_object_new(size_t length);

json_value *json_object_push(json_value *object, const char *name, json_value *);

json_value *json_object_push_length(json_value *object, unsigned int name_length, const char *name, json_value *);

json_value *json_object_push_nocopy(json_value *object, unsigned int name_length, char *name, json_value *);

json_value *json_object_merge(json_value *object_a, json_value *object_b);

void json_object_sort(json_value *object, json_value *proto);

json_value *json_string_new(const char *);
json_value *json_string_new_length(unsigned int length, const char *);
json_value *json_string_new_nocopy(unsigned int length, char *);

json_value *json_integer_new(int64_t);
json_value *json_double_new(double);
json_value *json_boolean_new(bool);
json_value *json_null_new();

typedef struct json_serialize_opts {
	int mode;
	int opts;
	int indent_size;
} json_serialize_opts;

size_t json_measure(json_value *);
size_t json_measure_ex(json_value *, json_serialize_opts);

void json_serialize(char *buf, json_value *);
void json_serialize_ex(char *buf, json_value *, json_serialize_opts);

void json_builder_free(json_value *);

#endif // JSON_ENCODE_H_INCLUDED
