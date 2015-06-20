#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <config.h>
#include <common.h>
#include <error.h>
#include "zmalloc.h"
#include "stack.h"
#include "quid.h"
#include "core.h"
#include "sql.h"

#define STACK_SZ	15

enum token {
	/* actions */
	T_SELECT,
	T_INSERT,
	T_DELETE,
	T_UPDATE,
	T_ALL,
	T_FROM,
	T_WHERE,
	T_AND,
	T_OR,
	/* group */
	T_BRACK_OPEN,
	T_BRACK_CLOSE,
	T_SEPARATE,
	T_COMMIT,
	/* operations */
	T_GREATER,
	T_SMALLER,
	T_EQUAL,
	T_GREATER_EQUAL,
	T_SMALLER_EQUAL,
	/* data */
	T_INTEGER,
	T_DOUBLE,
	T_STRING,
	T_QUID,
	T_INVALID,
};

struct stoken {
	enum token token;
	union {
		struct {
			int length;
			char *ptr;
		} string;
		int integer;
		double dbl;
	} u;
};

void *parse(stack_t *stack, size_t *len) {
	struct stoken *tok = stack_rpop(stack);
	if (!tok) {
		ERROR(ESQL_PARSE, EL_WARN);
		puts("Empty stack 1");
		return NULL;
	}
	switch (tok->token) {
		case T_SELECT:
			puts("SELECT");
			tree_zfree(tok);
			while ((tok = stack_rpop(stack)) != NULL) {
				if (tok->token == T_FROM)
					break;
				if (tok->token == T_ALL)
					puts("ALL");
				if (tok->token == T_STRING)
					printf("<%s>\n", tok->u.string.ptr);
			}
			tok = stack_rpop(stack);
			if (!tok) {
				ERROR(ESQL_PARSE, EL_WARN);
				puts("Empty stack 2");
				return NULL;
			}
			if (tok->token != T_QUID) {
				ERROR(ESQL_PARSE, EL_WARN);
				puts("No key 3");
				return NULL;
			}
			printf("[%s]\n", tok->u.string.ptr);
			void *data = db_get(tok->u.string.ptr, len);
			if (data) {
				printf("%.*s\n", (int)*len, (char *)data);
				return data;
			}
			break;
		case T_INSERT:
			puts("INSERT");
			break;
		case T_UPDATE:
			puts("UPDATE");
			break;
		case T_DELETE:
			puts("DELETE");
			tok = stack_rpop(stack);
			if (!tok) {
				ERROR(ESQL_PARSE, EL_WARN);
				puts("Empty stack 3");
				return NULL;
			}
			if (tok->token != T_FROM) {
				ERROR(ESQL_PARSE, EL_WARN);
				puts("No FROM");
				return NULL;
			}
			tok = stack_rpop(stack);
			if (!tok) {
				ERROR(ESQL_PARSE, EL_WARN);
				puts("Empty stack 4");
				return NULL;
			}
			if (tok->token != T_QUID) {
				ERROR(ESQL_PARSE, EL_WARN);
				puts("No key 4");
				return NULL;
			}
			printf("[%s]\n", tok->u.string.ptr);
			db_delete(tok->u.string.ptr);
			break;
		case T_ALL:
		case T_FROM:
		case T_WHERE:
		case T_AND:
		case T_OR:
		case T_BRACK_OPEN:
		case T_BRACK_CLOSE:
		case T_SEPARATE:
		case T_COMMIT:
		case T_GREATER:
		case T_SMALLER:
		case T_EQUAL:
		case T_GREATER_EQUAL:
		case T_SMALLER_EQUAL:
		case T_INTEGER:
		case T_DOUBLE:
		case T_STRING:
		case T_QUID:
		case T_INVALID:
			ERROR(ESQL_PARSE, EL_WARN);
			puts("Invalid query 1");
			break;
	}
	return NULL;
}

char *explode_sql(char *sql) {
	int pad = 0;
	char *osql = sql;
	for (; *sql; ++sql)
		switch (*sql) {
			case '>':
				if (*(sql+1) == '=') {
					if (!isspace(*(sql+2)))
						pad++;
					if (!isspace(*(sql-1)))
						pad++;
					++sql;
					break;
				}
			case '<':
				if (*(sql+1) == '>') {
					if (!isspace(*(sql+2)))
						pad++;
					if (!isspace(*(sql-1)))
						pad++;
					++sql;
					break;
				}
				if (*(sql+1) == '=') {
					if (!isspace(*(sql+2)))
						pad++;
					if (!isspace(*(sql-1)))
						pad++;
					++sql;
					break;
				}
			case '(':
			case ')':
			case '=':
			case ';':
				if (!isspace(*(sql+1)))
					pad++;
				if (!isspace(*(sql-1)))
					pad++;
				break;
		}
	sql = osql;
	char *_sql = zmalloc(strlen(sql)+pad+1);
	char *_osql = _sql;
	unsigned int i;
	for (i=0; i<strlen(sql); ++i) {
		switch (sql[i]) {
			case '(':
			case ')':
			case '<':
			case ';':
				if (!isspace(sql[i-1])) {
					_sql[i] = ' ';
					_sql++;
				}
				break;
			case '=':
				if (sql[i-1] != '>' && sql[i-1] != '<') {
					if (!isspace(sql[i-1])) {
						_sql[i] = ' ';
						_sql++;
					}
				}
				break;
			case '>':
				if (sql[i-1] != '<') {
					if (!isspace(sql[i-1])) {
						_sql[i] = ' ';
						_sql++;
					}
				}
				break;
		}
		_sql[i] = sql[i];
		switch (sql[i]) {
			case '(':
			case ')':
			case '=':
			case ';':
				if (!isspace(sql[i+1])) {
					_sql[i+1] = ' ';
					_sql++;
				}
				break;
			case '>':
				if (sql[i+1] != '=') {
					if (!isspace(sql[i+1])) {
						_sql[i+1] = ' ';
						_sql++;
					}
				}
				break;
			case '<':
				if (sql[i+1] != '>' && sql[i+1] != '=') {
					if (!isspace(sql[i+1])) {
						_sql[i+1] = ' ';
						_sql++;
					}
				}
				break;
		}
	}
	_sql[i] = '\0';
	return _osql;
}

int tokenize(stack_t *stack, char sql[]) {
	char *_ustr = explode_sql(sql);
	char *pch = strtok(_ustr," ,");
	while(pch != NULL) {
		if (!strcmp(pch, "SELECT")) {
			struct stoken *tok = (struct stoken *)tree_zmalloc(sizeof(struct stoken), NULL);
			tok->token = T_SELECT;
			stack_push(stack, tok);
		} else if (!strcmp(pch, "INSERT")) {
			struct stoken *tok = (struct stoken *)tree_zmalloc(sizeof(struct stoken), NULL);
			tok->token = T_INSERT;
			stack_push(stack, tok);
		} else if (!strcmp(pch, "UPDATE")) {
			struct stoken *tok = (struct stoken *)tree_zmalloc(sizeof(struct stoken), NULL);
			tok->token = T_UPDATE;
			stack_push(stack, tok);
		} else if (!strcmp(pch, "DELETE")) {
			struct stoken *tok = (struct stoken *)tree_zmalloc(sizeof(struct stoken), NULL);
			tok->token = T_DELETE;
			stack_push(stack, tok);
		} else if (!strcmp(pch, "SET")) {
			struct stoken *tok = (struct stoken *)tree_zmalloc(sizeof(struct stoken), NULL);
			tok->token = T_SEPARATE;
			stack_push(stack, tok);
		} else if (!strcmp(pch, "INTO")) {
		} else if (!strcmp(pch, "VALUES")) {
			struct stoken *tok = (struct stoken *)tree_zmalloc(sizeof(struct stoken), NULL);
			tok->token = T_SEPARATE;
			stack_push(stack, tok);
		} else if (!strcmp(pch, "(")) {
			struct stoken *tok = (struct stoken *)tree_zmalloc(sizeof(struct stoken), NULL);
			tok->token = T_BRACK_OPEN;
			stack_push(stack, tok);
		} else if (!strcmp(pch, ")")) {
			struct stoken *tok = (struct stoken *)tree_zmalloc(sizeof(struct stoken), NULL);
			tok->token = T_BRACK_CLOSE;
			stack_push(stack, tok);
		} else if (!strcmp(pch, "*")) {
			struct stoken *tok = (struct stoken *)tree_zmalloc(sizeof(struct stoken), NULL);
			tok->token = T_ALL;
			stack_push(stack, tok);
		} else if (!strcmp(pch, "FROM")) {
			struct stoken *tok = (struct stoken *)tree_zmalloc(sizeof(struct stoken), NULL);
			tok->token = T_FROM;
			stack_push(stack, tok);
		} else if (!strcmp(pch, "WHERE")) {
			struct stoken *tok = (struct stoken *)tree_zmalloc(sizeof(struct stoken), NULL);
			tok->token = T_WHERE;
			stack_push(stack, tok);
		} else if (!strcmp(pch, "AND")) {
			struct stoken *tok = (struct stoken *)tree_zmalloc(sizeof(struct stoken), NULL);
			tok->token = T_AND;
			stack_push(stack, tok);
		} else if (!strcmp(pch, "OR")) {
			struct stoken *tok = (struct stoken *)tree_zmalloc(sizeof(struct stoken), NULL);
			tok->token = T_OR;
			stack_push(stack, tok);
		} else if (!strcmp(pch, ">")) {
			struct stoken *tok = (struct stoken *)tree_zmalloc(sizeof(struct stoken), NULL);
			tok->token = T_GREATER;
			stack_push(stack, tok);
		} else if (!strcmp(pch, "<")) {
			struct stoken *tok = (struct stoken *)tree_zmalloc(sizeof(struct stoken), NULL);
			tok->token = T_SMALLER;
			stack_push(stack, tok);
		} else if (!strcmp(pch, "=")) {
			struct stoken *tok = (struct stoken *)tree_zmalloc(sizeof(struct stoken), NULL);
			tok->token =  T_EQUAL;
			stack_push(stack, tok);
		} else if (!strcmp(pch, "<=")) {
			struct stoken *tok = (struct stoken *)tree_zmalloc(sizeof(struct stoken), NULL);
			tok->token =  T_SMALLER_EQUAL;
			stack_push(stack, tok);
		} else if (!strcmp(pch, ">=")) {
			struct stoken *tok = (struct stoken *)tree_zmalloc(sizeof(struct stoken), NULL);
			tok->token =  T_GREATER_EQUAL;
			stack_push(stack, tok);
		} else if (!strcmp(pch, ";")) {
			struct stoken *tok = (struct stoken *)tree_zmalloc(sizeof(struct stoken), NULL);
			tok->token =  T_COMMIT;
			stack_push(stack, tok);
		} else if (strisdigit(pch)) {
			struct stoken *tok = (struct stoken *)tree_zmalloc(sizeof(struct stoken), NULL);
			tok->token =  T_INTEGER;
			tok->u.integer = atoi(pch);
			stack_push(stack, tok);
		} else if (strismatch(pch, "1234567890.")) {
			if (strccnt(pch, '.') != 1)
				goto tok_next;
			if (pch[0] == '.' || pch[strlen(pch)-1] == '.')
				goto tok_next;
			struct stoken *tok = (struct stoken *)tree_zmalloc(sizeof(struct stoken), NULL);
			tok->token =  T_DOUBLE;
			tok->u.dbl = atof(pch);
			stack_push(stack, tok);
		} else if (strisualpha(pch)) {
			struct stoken *tok = (struct stoken *)tree_zmalloc(sizeof(struct stoken), NULL);
			tok->token =  T_STRING;
			char *_s = tree_zstrdup(pch, tok);
			tok->u.string.ptr = _s;
			tok->u.string.length = strlen(_s);
			stack_push(stack, tok);
		} else if ((pch[0] == '"' && pch[strlen(pch)-1] == '"') || (pch[0] == '\'' && pch[strlen(pch)-1] == '\'')) {
			struct stoken *tok = (struct stoken *)tree_zmalloc(sizeof(struct stoken), NULL);
			tok->token = T_STRING;
			char *_s = tree_zstrdup(pch, tok);
			_s[strlen(pch)-1] = '\0';
			_s++;
			if (strquid_format(_s)>0)
				tok->token = T_QUID;
			tok->u.string.ptr = _s;
			tok->u.string.length = strlen(_s);
			stack_push(stack, tok);
		} else {
			ERROR(ESQL_TOKEN, EL_WARN);
			printf("Unexpected token '%s'\n", pch);
			return 0;
		}
tok_next:
		pch = strtok(NULL, " ,");
	}
	zfree(_ustr);
	return 1;
}

void *sql_exec(const char *sql, size_t *len) {
	ERRORZEOR();
	void *rs = NULL;
	stack_t tokenstream;
	stack_init(&tokenstream, STACK_SZ);
	if (tokenize(&tokenstream, (char *)sql))
		rs = parse(&tokenstream, len);
	stack_destroy(&tokenstream);

	return rs;
}
