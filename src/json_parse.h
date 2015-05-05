#ifndef JSON_PARSE_H_INCLUDED
#define JSON_PARSE_H_INCLUDED

#include <stdlib.h>
#include <inttypes.h>

#define json_enable_comments  0x01
#define json_error_max 128

extern const struct _json_value json_value_none;

typedef struct {
	unsigned long max_memory;
	int settings;

	/* Custom allocator support (leave null to use malloc/free)*/
	void *(* mem_alloc)(size_t, int zero, void *user_data);
	void (* mem_free)(void *, void *user_data);
	void *user_data;  /* will be passed to mem_alloc and mem_free */
	size_t value_extra;  /* how much extra space to allocate for values? */
} json_settings;

typedef enum {
	json_none,
	json_object,
	json_array,
	json_integer,
	json_double,
	json_string,
	json_boolean,
	json_null
} json_type;

typedef struct _json_object_entry {
	char *name;
	unsigned int name_length;
	struct _json_value *value;
} json_object_entry;

typedef struct _json_value {
	struct _json_value *parent;
	json_type type;

	union {
		int boolean;
		int64_t integer;
		double dbl;

		struct {
			unsigned int length;
			char *ptr; /* null terminated */
		} string;

		struct {
			unsigned int length;
			json_object_entry *values;
		} object;

		struct {
			unsigned int length;
			struct _json_value **values;
		} array;
	} u;

	union {
		struct _json_value *next_alloc;
		void *object_mem;
	} _reserved;
} json_value;

json_value *json_parse(const char *json, size_t length);

json_value *json_parse_ex(json_settings *settings, const char *json, size_t length, char *error);

void json_value_free(json_value *);


/* Not usually necessary, unless you used a custom mem_alloc and now want to
 * use a custom mem_free.
 */
void json_value_free_ex(json_settings *settings, json_value *);

#endif // JSON_PARSE_H_INCLUDED
