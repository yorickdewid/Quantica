#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <config.h>
#include <common.h>

#include "dict.h"
#include "zmalloc.h"

/* Allocates a fresh unused token from the token pull. */
static dict_token_t *dict_alloc_token(dict_parser *parser, dict_token_t *tokens, size_t num_tokens) {
	dict_token_t *tok;
	if (parser->toknext >= num_tokens) {
		return NULL;
	}
	tok = &tokens[parser->toknext++];
	tok->start = tok->end = -1;
	tok->size = 0;
#ifdef DICT_PARENT_LINKS
	tok->parent = -1;
#endif
	return tok;
}

/* Fills token type and boundaries. */
static void dict_fill_token(dict_token_t *token, dict_type_t type, int start, int end) {
	token->type = type;
	token->start = start;
	token->end = end;
	token->size = 0;
}

/* Fills next available token with dictionary primitive. */
static dict_err_t dict_parse_primitive(dict_parser *parser, const char *str, size_t len, dict_token_t *tokens, size_t num_tokens) {
	dict_token_t *token;
	int start;

	start = parser->pos;

	for (; parser->pos < len && str[parser->pos] != '\0'; parser->pos++) {
		switch (str[parser->pos]) {
			/* In strict mode primitive must be followed by "," or "}" or "]" */
			case ':':
			case '\t' : case '\r' : case '\n' : case ' ' :
			case ','  : case ']'  : case '}' :
				goto found;
			default:
				break;
		}
		if (str[parser->pos] < 32 || str[parser->pos] >= 127) {
			parser->pos = start;
			return DICT_ERROR_INVAL;
		}
	}
	/* In strict mode primitive must be followed by a comma/object/array */
	parser->pos = start;
	return DICT_ERROR_PART;

found:
	if (tokens == NULL) {
		parser->pos--;
		return 0;
	}
	token = dict_alloc_token(parser, tokens, num_tokens);
	if (token == NULL) {
		parser->pos = start;
		return DICT_ERROR_NOMEM;
	}
	dict_fill_token(token, DICT_PRIMITIVE, start, parser->pos);
#ifdef DICT_PARENT_LINKS
	token->parent = parser->toksuper;
#endif
	parser->pos--;
	return 0;
}

/* Fills next token with dictionary string. */
static dict_err_t dict_parse_string(dict_parser *parser, const char *str, size_t len, dict_token_t *tokens, size_t num_tokens) {
	dict_token_t *token;

	int start = parser->pos;

	parser->pos++;

	/* Skip starting quote */
	for (; parser->pos < len && str[parser->pos] != '\0'; parser->pos++) {
		char c = str[parser->pos];

		/* Quote: end of string */
		if (c == '\"') {
			if (tokens == NULL) {
				return 0;
			}
			token = dict_alloc_token(parser, tokens, num_tokens);
			if (token == NULL) {
				parser->pos = start;
				return DICT_ERROR_NOMEM;
			}
			dict_fill_token(token, DICT_STRING, start + 1, parser->pos);
#ifdef DICT_PARENT_LINKS
			token->parent = parser->toksuper;
#endif
			return 0;
		}

		/* Backslash: Quoted symbol expected */
		if (c == '\\' && parser->pos + 1 < len) {
			int i;
			parser->pos++;
			switch (str[parser->pos]) {
				/* Allowed escaped symbols */
				case '\"': case '/' : case '\\' : case 'b' :
				case 'f' : case 'r' : case 'n'  : case 't' :
					break;
				/* Allows escaped symbol \uXXXX */
				case 'u':
					parser->pos++;
					for (i = 0; i < 4 && parser->pos < len && str[parser->pos] != '\0'; i++) {
						/* If it isn't a hex character we have an error */
						if (!((str[parser->pos] >= 48 && str[parser->pos] <= 57) || /* 0-9 */
						        (str[parser->pos] >= 65 && str[parser->pos] <= 70) || /* A-F */
						        (str[parser->pos] >= 97 && str[parser->pos] <= 102))) { /* a-f */
							parser->pos = start;
							return DICT_ERROR_INVAL;
						}
						parser->pos++;
					}
					parser->pos--;
					break;
				/* Unexpected symbol */
				default:
					parser->pos = start;
					return DICT_ERROR_INVAL;
			}
		}
	}
	parser->pos = start;
	return DICT_ERROR_PART;
}

/* Parse dictionary string and fill tokens. */
dict_err_t dict_parse(dict_parser *parser, const char *str, size_t len, dict_token_t *tokens, unsigned int num_tokens) {
	dict_err_t r;
	int i;
	dict_token_t *token;
	int count = 0;

	for (; parser->pos < len && str[parser->pos] != '\0'; parser->pos++) {
		char c;
		dict_type_t type;

		c = str[parser->pos];
		switch (c) {
			case '{': case '[':
				count++;
				if (tokens == NULL) {
					break;
				}
				token = dict_alloc_token(parser, tokens, num_tokens);
				if (token == NULL)
					return DICT_ERROR_NOMEM;
				if (parser->toksuper != -1) {
					tokens[parser->toksuper].size++;
#ifdef DICT_PARENT_LINKS
					token->parent = parser->toksuper;
#endif
				}
				token->type = (c == '{' ? DICT_OBJECT : DICT_ARRAY);
				token->start = parser->pos;
				parser->toksuper = parser->toknext - 1;
				break;
			case '}': case ']':
				if (tokens == NULL)
					break;
				type = (c == '}' ? DICT_OBJECT : DICT_ARRAY);
#ifdef DICT_PARENT_LINKS
				if (parser->toknext < 1) {
					return DICT_ERROR_INVAL;
				}
				token = &tokens[parser->toknext - 1];
				for (;;) {
					if (token->start != -1 && token->end == -1) {
						if (token->type != type) {
							return DICT_ERROR_INVAL;
						}
						token->end = parser->pos + 1;
						parser->toksuper = token->parent;
						break;
					}
					if (token->parent == -1) {
						break;
					}
					token = &tokens[token->parent];
				}
#else
				for (i = parser->toknext - 1; i >= 0; i--) {
					token = &tokens[i];
					if (token->start != -1 && token->end == -1) {
						if (token->type != type) {
							return DICT_ERROR_INVAL;
						}
						parser->toksuper = -1;
						token->end = parser->pos + 1;
						break;
					}
				}
				/* Error if unmatched closing bracket */
				if (i == -1)
					return DICT_ERROR_INVAL;
				for (; i >= 0; i--) {
					token = &tokens[i];
					if (token->start != -1 && token->end == -1) {
						parser->toksuper = i;
						break;
					}
				}
#endif
				break;
			case '\"':
				r = dict_parse_string(parser, str, len, tokens, num_tokens);
				if (r < 0) return r;
				count++;
				if (parser->toksuper != -1 && tokens != NULL)
					tokens[parser->toksuper].size++;
				break;
			case '\t' : case '\r' : case '\n' : case ' ':
				break;
			case ':':
				parser->toksuper = parser->toknext - 1;
				break;
			case ',':
				if (tokens != NULL &&
				        tokens[parser->toksuper].type != DICT_ARRAY &&
				        tokens[parser->toksuper].type != DICT_OBJECT) {
#ifdef DICT_PARENT_LINKS
					parser->toksuper = tokens[parser->toksuper].parent;
#else
					for (i = parser->toknext - 1; i >= 0; i--) {
						if (tokens[i].type == DICT_ARRAY || tokens[i].type == DICT_OBJECT) {
							if (tokens[i].start != -1 && tokens[i].end == -1) {
								parser->toksuper = i;
								break;
							}
						}
					}
#endif
				}
				break;

			/* In strict mode primitives are: numbers and booleans */
			case '-': case '0': case '1' : case '2': case '3' : case '4':
			case '5': case '6': case '7' : case '8': case '9':
			case 't': case 'f': case 'n' :
				/* And they must not be keys of the object */
				if (tokens != NULL) {
					dict_token_t *t = &tokens[parser->toksuper];
					if (t->type == DICT_OBJECT ||
					        (t->type == DICT_STRING && t->size != 0)) {
						return DICT_ERROR_INVAL;
					}
				}
#if 0
			/* In non-strict mode every unquoted value is a primitive */
			default:
#endif
				r = dict_parse_primitive(parser, str, len, tokens, num_tokens);
				if (r < 0) return r;
				count++;
				if (parser->toksuper != -1 && tokens != NULL)
					tokens[parser->toksuper].size++;
				break;

			/* Unexpected char in strict mode */
			default:
				return DICT_ERROR_INVAL;
		}
	}

	for (i = parser->toknext - 1; i >= 0; i--) {
		/* Unmatched opened object or array */
		if (tokens[i].start != -1 && tokens[i].end == -1) {
			return DICT_ERROR_PART;
		}
	}

	return count;
}

/* Creates a new parser */
void dict_init(dict_parser *parser) {
	parser->pos = 0;
	parser->toknext = 0;
	parser->toksuper = -1;
}

int dict_cmp(const char *str, dict_token_t *tok, const char *s) {
	if ((tok->type == DICT_STRING || tok->type == DICT_PRIMITIVE) && (int)strlen(s) == tok->end - tok->start && !strncmp(str + tok->start, s, tok->end - tok->start))
		return 1;
	return 0;
}

int dict_levelcount(dict_token_t *t, int depth, int level, int *cnt) {
	int i, j;
	depth++;
	if (depth == level)
		(*cnt)++;
	if (t->type == DICT_PRIMITIVE) {
		return 1;
	} else if (t->type == DICT_STRING) {
		return 1;
	} else if (t->type == DICT_OBJECT) {
		j = 0;
		for (i = 0; i < t->size; i++) {
			j += dict_levelcount(t + 1 + j, depth, level, cnt);
			j += dict_levelcount(t + 1 + j, depth, level, cnt);
		}
		return j + 1;
	} else if (t->type == DICT_ARRAY) {
		j = 0;
		for (i = 0; i < t->size; i++) {
			j += dict_levelcount(t + 1 + j, depth, level, cnt);
		}
		return j + 1;
	}
	return 0;
}

char *dict_array(vector_t *v, char *buf) {
	unsigned int i;
	char *_buf = buf + 1;
	for (i = 0; i < v->size; ++i) {
		dict_t *item = (dict_t *)vector_at(v, i);
		if (i != (v->size - 1)) {
			if (item->cap)
				sprintf(_buf + strlen(_buf), "\"%s\",", item->str);
			else
				sprintf(_buf + strlen(_buf), "%s,", item->str);
		} else {
			if (item->cap)
				sprintf(_buf + strlen(_buf), "\"%s\"", item->str);
			else
				sprintf(_buf + strlen(_buf), "%s", item->str);
		}
	}
	buf[0] = '[';
	buf[strlen(buf)] = ']';
	buf[strlen(buf) + 1] = '\0';

	return buf;
}

char *dict_object(vector_t *v, char *buf) {
	unsigned int i;
	char *_buf = buf + 1;
	for (i = 0; i < v->size; ++i) {
		dict_t *item = (dict_t *)vector_at(v, i);
		if (i != (v->size - 1)) {
			if (item->cap)
				sprintf(_buf + strlen(_buf), "\"%s\":\"%s\",", item->name, item->str);
			else
				sprintf(_buf + strlen(_buf), "\"%s\":%s,", item->name, item->str);
		} else {
			if (item->cap)
				sprintf(_buf + strlen(_buf), "\"%s\":\"%s\"", item->name, item->str);
			else
				sprintf(_buf + strlen(_buf), "\"%s\":%s", item->name, item->str);
		}
	}
	buf[0] = '{';
	buf[strlen(buf)] = '}';
	buf[strlen(buf) + 1] = '\0';

	return buf;
}

dict_t *dict_element_cnew(vector_t *v, bool encapsulate, char *name, char *val) {
	dict_t *elm = (dict_t *)tree_zmalloc(sizeof(dict_t), v);
	elm->str = val;
	elm->cap = encapsulate;
	if (name)
		elm->name = tree_zstrdup(name, v);
	return elm;
}

dict_t *dict_element_new(vector_t *v, bool encapsulate, char *name, char *val) {
	dict_t *elm = (dict_t *)tree_zmalloc(sizeof(dict_t), v);
	elm->str = tree_zstrdup(val, v);
	elm->cap = encapsulate;
	if (name)
		elm->name = tree_zstrdup(name, v);
	return elm;
}
