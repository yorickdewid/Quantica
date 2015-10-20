#ifndef MARSHALL_H_INCLUDED
#define MARSHALL_H_INCLUDED

typedef enum {
	DICT_NULL,
	DICT_TRUE,
	DICT_FALSE,
	DICT_INT,
	DICT_STR,
	DICT_ARR,
	DICT_OBJ
} slay_type_t;

typedef struct slay_serialize {
	char *name;
	void *data;
	struct slay_serialize **child;
	unsigned int sz;
	slay_type_t type;
} slay_serialize_t;

slay_serialize_t *marshall_decode(char *data, size_t data_len, char *name, void *parent);

#endif // MARSHALL_H_INCLUDED
