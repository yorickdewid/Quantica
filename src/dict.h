#ifndef DICT_H_INCLUDED
#define DICT_H_INCLUDED

#include <stddef.h>

/**
 * Dictionary type identifier. Basic types are:
 * 	o Object
 * 	o Array
 * 	o String
 * 	o Other primitive: number, boolean (true/false) or null
 */
typedef enum {
	DICT_PRIMITIVE = 0,
	DICT_OBJECT = 1,
	DICT_ARRAY = 2,
	DICT_STRING = 3
} dict_type_t;

typedef enum {
	/* Not enough tokens were provided */
	DICT_ERROR_NOMEM = -1,
	/* Invalid character inside dictionary string */
	DICT_ERROR_INVAL = -2,
	/* The string is not a full dictionary packet, more bytes expected */
	DICT_ERROR_PART = -3
} dict_err_t;

/**
 * Dictionary token description.
 * @param		type	type (object, array, string etc.)
 * @param		start	start position in dictionary data string
 * @param		end		end position in dictionary data string
 */
typedef struct {
	dict_type_t type;
	int start;
	int end;
	int size;
#ifdef DICT_PARENT_LINKS
	int parent;
#endif
} dict_token_t;

/**
 * Dictionary parser. Contains an array of token blocks available. Also stores
 * the string being parsed now and current position in that string
 */
typedef struct {
	unsigned int pos; /* offset in the dictionary string */
	unsigned int toknext; /* next token to allocate */
	int toksuper; /* superior token node, e.g parent object or array */
} dict_parser;

/**
 * Create dictionary parser over an array of tokens
 */
void dict_init(dict_parser *parser);

/**
 * Run dictionary parser. It parses a dictionary data string into and array of tokens, each describing
 * a single dictionary object.
 */
dict_err_t dict_parse(dict_parser *parser, const char *str, size_t len, dict_token_t *tokens, unsigned int num_tokens);

#endif // DICT_H_INCLUDED
