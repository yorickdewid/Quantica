#ifndef DICT_H_INCLUDED
#define DICT_H_INCLUDED

#include <stddef.h>

#include <config.h>
#include <common.h>

#include "vector.h"

/* Dictionary type identifier. */
typedef enum {
	DICT_PRIMITIVE = 0,
	DICT_OBJECT = 1,
	DICT_ARRAY = 2,
	DICT_STRING = 3
} dict_type_t;

typedef enum {
	DICT_ERROR_NOMEM = -1,	/* Not enough tokens were provided */
	DICT_ERROR_INVAL = -2,	/* Invalid character inside dictionary string */
	DICT_ERROR_PART = -3	/* The string is not a full dictionary packet, more bytes expected */
} dict_err_t;

/* Dictionary token description. */
typedef struct {
	dict_type_t type;
	int start;
	int end;
	int size;
#ifdef DICT_PARENT_LINKS
	int parent;
#endif
} dict_token_t;

/* Dictionary parser. */
typedef struct {
	unsigned int pos;		/* offset in the dictionary string */
	unsigned int toknext;	/* next token to allocate */
	int toksuper;			/* superior token node, e.g parent object or array */
} dict_parser;

typedef struct {
	char *name;
	char *str;
	bool cap;
} dict_t;

/* Create dictionary parser over an array of tokens */
void dict_init(dict_parser *parser);

/* Run dictionary parser. */
dict_err_t dict_parse(dict_parser *parser, const char *str, size_t len, dict_token_t *tokens, unsigned int num_tokens);

int dict_cmp(const char *str, dict_token_t *tok, const char *s);

int dict_levelcount(dict_token_t *t, int depth, int level, int *cnt);

char *dict_array(vector_t *v, char *buf);

char *dict_object(vector_t *v, char *buf);

dict_t *dict_element_cnew(vector_t *v, bool encapsulate, char *name, char *val);

dict_t *dict_element_new(vector_t *v, bool encapsulate, char *name, char *val);

#endif // DICT_H_INCLUDED
