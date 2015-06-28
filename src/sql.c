#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <config.h>
#include <common.h>
#include <error.h>
#include "zmalloc.h"
#include "stack.h"
#include "slay.h"
#include "dict.h"
#include "quid.h"
#include "core.h"
#include "core.h"
#include "sql.h"

#define STACK_SZ	15
static int charcnt = 0;
static int cnt = 0;

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
	T_TRUE,
	T_FALSE,
	T_NULL,
	T_INVALID,
};

struct stoken {
	enum token token;
	char *string;
	int length;
};

sqlresult_t *parse(stack_t *stack, size_t *len) {
	static sqlresult_t rs;
	memset(&rs, '\0', sizeof(sqlresult_t));
	if (stack->size<=0) {
		ERROR(ESQL_PARSE_END, EL_WARN);
		return &rs;
	}
	struct stoken *tok = stack_rpop(stack);
	switch (tok->token) {
		case T_SELECT:
			while (stack->size>0) {
				tok = stack_rpop(stack);
				if (tok->token == T_FROM)
					break;
			}
			if (stack->size<=0) {
				ERROR(ESQL_PARSE_END, EL_WARN);
				return &rs;
			}
			tok = stack_rpop(stack);
			if (tok->token != T_QUID) {
				ERROR(ESQL_PARSE_VAL, EL_WARN);
				return &rs;
			}
			rs.data = db_get(tok->string, len);
			if (rs.data)
				return &rs;
			break;
		case T_INSERT: {
			struct objname {
				char *name;
				int length;
			} *name = NULL;
			schema_t schema = SCHEMA_ARRAY;
			if (cnt == 1)
				schema = SCHEMA_FIELD;
			if (stack->size<=0) {
				ERROR(ESQL_PARSE_END, EL_WARN);
				return &rs;
			}
			tok = stack_rpop(stack);
			if (tok->token == T_SEPARATE)
				goto insert_arr;
			if (tok->token != T_BRACK_OPEN) {
				ERROR(ESQL_PARSE_TOK, EL_WARN);
				return &rs;
			}
			schema = SCHEMA_OBJECT;
			int name_cnt = cnt/2;
			name = zmalloc(sizeof(struct objname) * name_cnt);
			int i = 0;
			while (stack->size>0) {
				tok = stack_rpop(stack);
				if (tok->token == T_BRACK_CLOSE)
					break;
				if (i+1 > name_cnt) {
					name = zrealloc(name, sizeof(struct objname) * ++name_cnt);
				}
				if (tok->token == T_STRING) {
					name[i].name = tok->string;
					name[i].length = tok->length;
					i++;
					cnt--;
				}
			}
			if (stack->size<=0) {
				ERROR(ESQL_PARSE_END, EL_WARN);
				return &rs;
			}
			tok = stack_rpop(stack);
			if (tok->token != T_SEPARATE) {
				ERROR(ESQL_PARSE_TOK, EL_WARN);
				return &rs;
			}
insert_arr:
			if (stack->size<=0) {
				ERROR(ESQL_PARSE_END, EL_WARN);
				return &rs;
			}
			tok = stack_rpop(stack);
			if (tok->token != T_BRACK_OPEN) {
				ERROR(ESQL_PARSE_TOK, EL_WARN);
				return &rs;
			}
			size_t slay_len = 0;
			int j = 0;
			void *slay = create_row(schema, cnt, charcnt, &slay_len);
			void *next = movetodata_row(slay);
			while (stack->size>0) {
				tok = stack_rpop(stack);
				if (tok->token == T_BRACK_CLOSE)
					break;
				if (name && j>=i)
					break;
				if (tok->token == T_STRING) {
					if (name)
						next = slay_wrap(next, name[j].name, name[j].length, tok->string, tok->length, DT_TEXT);
					else
						next = slay_wrap(next, NULL, 0, tok->string, tok->length, DT_TEXT);
				} else if (tok->token == T_INTEGER || tok->token == T_DOUBLE) {
					if (name)
						next = slay_wrap(next, name[j].name, name[j].length, tok->string, tok->length, DT_INT);
					else
						next = slay_wrap(next, NULL, 0, tok->string, tok->length, DT_INT);
				} else if (tok->token == T_QUID) {
					if (name)
						next = slay_wrap(next, name[j].name, name[j].length, tok->string, tok->length, DT_QUID);
					else
						next = slay_wrap(next, NULL, 0, tok->string, tok->length, DT_QUID);
				} else if (tok->token == T_FALSE) {
					if (name)
						next = slay_wrap(next, name[j].name, name[j].length, NULL, 0, DT_BOOL_F);
					else
						next = slay_wrap(next, NULL, 0, NULL, 0, DT_BOOL_F);
				} else if (tok->token == T_TRUE) {
					if (name)
						next = slay_wrap(next, name[j].name, name[j].length, NULL, 0, DT_BOOL_T);
					else
						next = slay_wrap(next, NULL, 0, NULL, 0, DT_BOOL_T);
				}
				j++;
			}
			if (name)
				zfree(name);
			_db_put(rs.quid, slay, slay_len);
			rs.items = j;
			return &rs;
		}
		case T_UPDATE:
			break;
		case T_DELETE:
			if (stack->size<=0) {
				ERROR(ESQL_PARSE_END, EL_WARN);
				return &rs;
			}
			tok = stack_rpop(stack);
			if (tok->token != T_FROM) {
				ERROR(ESQL_PARSE_TOK, EL_WARN);
				return &rs;
			}
			if (stack->size<=0) {
				ERROR(ESQL_PARSE_END, EL_WARN);
				return &rs;
			}
			tok = stack_rpop(stack);
			if (tok->token != T_QUID) {
				ERROR(ESQL_PARSE_VAL, EL_WARN);
				return &rs;
			}
			db_delete(tok->string);
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
		case T_TRUE:
		case T_FALSE:
		case T_NULL:
		case T_INVALID:
			ERROR(ESQL_PARSE_TOK, EL_WARN);
			break;
	}
	return &rs;
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
		if (!strcmp(pch, "SELECT") || !strcmp(pch, "select")) {
			struct stoken *tok = (struct stoken *)tree_zmalloc(sizeof(struct stoken), NULL);
			tok->token = T_SELECT;
			stack_push(stack, tok);
		} else if (!strcmp(pch, "INSERT") || !strcmp(pch, "insert")) {
			struct stoken *tok = (struct stoken *)tree_zmalloc(sizeof(struct stoken), NULL);
			tok->token = T_INSERT;
			stack_push(stack, tok);
		} else if (!strcmp(pch, "UPDATE") || !strcmp(pch, "update")) {
			struct stoken *tok = (struct stoken *)tree_zmalloc(sizeof(struct stoken), NULL);
			tok->token = T_UPDATE;
			stack_push(stack, tok);
		} else if (!strcmp(pch, "DELETE") || !strcmp(pch, "delete")) {
			struct stoken *tok = (struct stoken *)tree_zmalloc(sizeof(struct stoken), NULL);
			tok->token = T_DELETE;
			stack_push(stack, tok);
		} else if (!strcmp(pch, "SET") || !strcmp(pch, "set")) {
			struct stoken *tok = (struct stoken *)tree_zmalloc(sizeof(struct stoken), NULL);
			tok->token = T_SEPARATE;
			stack_push(stack, tok);
		} else if (!strcmp(pch, "INTO") || !strcmp(pch, "into")) {
		} else if (!strcmp(pch, "VALUES") || !strcmp(pch, "values")) {
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
		} else if (!strcmp(pch, "FROM") || !strcmp(pch, "from")) {
			struct stoken *tok = (struct stoken *)tree_zmalloc(sizeof(struct stoken), NULL);
			tok->token = T_FROM;
			stack_push(stack, tok);
		} else if (!strcmp(pch, "WHERE") || !strcmp(pch, "where")) {
			struct stoken *tok = (struct stoken *)tree_zmalloc(sizeof(struct stoken), NULL);
			tok->token = T_WHERE;
			stack_push(stack, tok);
		} else if (!strcmp(pch, "AND") || !strcmp(pch, "and") || !strcmp(pch, "&&")) {
			struct stoken *tok = (struct stoken *)tree_zmalloc(sizeof(struct stoken), NULL);
			tok->token = T_AND;
			stack_push(stack, tok);
		} else if (!strcmp(pch, "OR") || !strcmp(pch, "or") || !strcmp(pch, "||")) {
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
		} else if (!strcmp(pch, "TRUE") || !strcmp(pch, "true")) {
			cnt++;
			struct stoken *tok = (struct stoken *)tree_zmalloc(sizeof(struct stoken), NULL);
			tok->token = T_TRUE;
			stack_push(stack, tok);
		} else if (!strcmp(pch, "FALSE") || !strcmp(pch, "false")) {
			cnt++;
			struct stoken *tok = (struct stoken *)tree_zmalloc(sizeof(struct stoken), NULL);
			tok->token = T_FALSE;
			stack_push(stack, tok);
		} else if (!strcmp(pch, "NULL") || !strcmp(pch, "null")) {
			cnt++;
			struct stoken *tok = (struct stoken *)tree_zmalloc(sizeof(struct stoken), NULL);
			tok->token = T_NULL;
			stack_push(stack, tok);
		} else if (strisdigit(pch)) {
			charcnt += strlen(pch);
			cnt++;
			struct stoken *tok = (struct stoken *)tree_zmalloc(sizeof(struct stoken), NULL);
			tok->token = T_INTEGER;
			char *_s = tree_zstrdup(pch, tok);
			tok->string = _s;
			tok->length = strlen(_s);
			stack_push(stack, tok);
		} else if (strismatch(pch, "1234567890.")) {
			if (strccnt(pch, '.') != 1)
				goto tok_next;
			if (pch[0] == '.' || pch[strlen(pch)-1] == '.')
				goto tok_next;
			charcnt += strlen(pch);
			cnt++;
			struct stoken *tok = (struct stoken *)tree_zmalloc(sizeof(struct stoken), NULL);
			tok->token = T_DOUBLE;
			char *_s = tree_zstrdup(pch, tok);
			tok->string = _s;
			tok->length = strlen(_s);
			stack_push(stack, tok);
		} else if (strisualpha(pch)) {
			charcnt += strlen(pch);
			cnt++;
			struct stoken *tok = (struct stoken *)tree_zmalloc(sizeof(struct stoken), NULL);
			tok->token = T_STRING;
			char *_s = tree_zstrdup(pch, tok);
			tok->string = _s;
			tok->length = strlen(_s);
			stack_push(stack, tok);
		} else if ((pch[0] == '"' && pch[strlen(pch)-1] == '"') || (pch[0] == '\'' && pch[strlen(pch)-1] == '\'')) {
			charcnt += strlen(pch);
			cnt++;
			struct stoken *tok = (struct stoken *)tree_zmalloc(sizeof(struct stoken), NULL);
			tok->token = T_STRING;
			char *_s = tree_zstrdup(pch, tok);
			_s[strlen(pch)-1] = '\0';
			_s++;
			if (strquid_format(_s)>0)
				tok->token = T_QUID;
			tok->string = _s;
			tok->length = strlen(_s);
			stack_push(stack, tok);
		} else {
			ERROR(ESQL_TOKEN, EL_WARN);
			zfree(_ustr);
			return 0;
		}
tok_next:
		pch = strtok(NULL, " ,");
	}
	zfree(_ustr);
	return 1;
}

sqlresult_t *sql_exec(const char *sql, size_t *len) {
	sqlresult_t *rs = NULL;
	ERRORZEOR();
	charcnt = 0;
	cnt = 0;
	stack_t tokenstream;
	stack_init(&tokenstream, STACK_SZ);
	if (tokenize(&tokenstream, (char *)sql))
		rs = parse(&tokenstream, len);
	stack_destroy(&tokenstream);

	return rs;
}
