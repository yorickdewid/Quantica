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
#include "md5.h"
#include "sha1.h"
#include "sha256.h"
#include "core.h"
#include "core.h"
#include "sql.h"

#define STACK_SZ	15
#define STACK_EMPTY_POP()				\
	if (stack->size<=0) {				\
		ERROR(ESQL_PARSE_END, EL_WARN);	\
		return &rs;						\
	}									\
	tok = stack_rpop(stack);

static int charcnt = 0;
static int cnt = 0;
extern unsigned int run;

enum token {
	/* actions */
	T_SELECT,
	T_INSERT,
	T_DELETE,
	T_UPDATE,
	T_ASTERISK,
	T_FROM,
	T_WHERE,
	T_ORDER,
	T_HAVING,
	/* logical */
	T_AND,
	T_ANY,
	T_ALL,
	T_BETWEEN,
	T_EXISTS,
	T_IN,
	T_LIKE,
	T_UNIQUE,
	T_NOT,
	T_IS,
	T_OR,
	T_BY,
	/* group */
	T_BRACK_OPEN,
	T_BRACK_CLOSE,
	T_SEPARATE,
	T_COMMIT,
	T_TARGET,
	/* arithmetic */
	T_ADD,
	T_SUBTRACT,
	T_DEVIDE,
	T_MODULO,
	/* comparison */
	T_GREATER,
	T_SMALLER,
	T_EQUAL,
	T_GREATER_EQUAL,
	T_SMALLER_EQUAL,
	/* functional */
	T_FUNC_QUID,
	T_FUNC_NOW,
	T_FUNC_MD5,
	T_FUNC_SHA1,
	T_FUNC_SHA256,
	T_FUNC_VACUUM,
	T_FUNC_SYNC,
	T_FUNC_SHUTDOWN,
	T_FUNC_INSTANCE_NAME,
	T_FUNC_INSTANCE_KEY,
	T_FUNC_SESSION_KEY,
	T_FUNC_LICENSE,
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
	if (tok->token == T_SELECT) {
		tok = stack_rpop(stack);
		if (tok->token == T_FUNC_QUID) {
			STACK_EMPTY_POP();
			if (tok->token != T_BRACK_OPEN) {
				ERROR(ESQL_PARSE_TOK, EL_WARN);
				return &rs;
			}
			STACK_EMPTY_POP();
			if (tok->token != T_BRACK_CLOSE) {
				ERROR(ESQL_PARSE_TOK, EL_WARN);
				return &rs;
			}
			quid_generate(rs.quid);
			return &rs;
		} else if (tok->token == T_FUNC_NOW) {
			STACK_EMPTY_POP();
			if (tok->token != T_BRACK_OPEN) {
				ERROR(ESQL_PARSE_TOK, EL_WARN);
				return &rs;
			}
			STACK_EMPTY_POP();
			if (tok->token != T_BRACK_CLOSE) {
				ERROR(ESQL_PARSE_TOK, EL_WARN);
				return &rs;
			}
			rs.name = zstrdup("datetime");
			rs.data = tstostrf(zmalloc(20), 20, get_timestamp(), ISO_8601_FORMAT);
			return &rs;
		} else if (tok->token == T_FUNC_MD5) {
			char *strmd5 = zmalloc(MD5_SIZE+1);
			strmd5[MD5_SIZE] = '\0';
			STACK_EMPTY_POP();
			if (tok->token != T_BRACK_OPEN) {
				ERROR(ESQL_PARSE_TOK, EL_WARN);
				return &rs;
			}
			STACK_EMPTY_POP();
			if (tok->token != T_STRING) {
				ERROR(ESQL_PARSE_TOK, EL_WARN);
				return &rs;
			}
			crypto_md5(strmd5, tok->string);
			STACK_EMPTY_POP();
			if (tok->token != T_BRACK_CLOSE) {
				ERROR(ESQL_PARSE_TOK, EL_WARN);
				return &rs;
			}
			rs.name = zstrdup("hash");
			rs.data = strmd5;
			return &rs;
		} else if (tok->token == T_FUNC_SHA1) {
			char *strsha = zmalloc(SHA1_LENGTH+1);
			strsha[SHA1_LENGTH] = '\0';
			STACK_EMPTY_POP();
			if (tok->token != T_BRACK_OPEN) {
				ERROR(ESQL_PARSE_TOK, EL_WARN);
				return &rs;
			}
			STACK_EMPTY_POP();
			if (tok->token != T_STRING) {
				ERROR(ESQL_PARSE_TOK, EL_WARN);
				return &rs;
			}
			crypto_sha1(strsha, tok->string);
			STACK_EMPTY_POP();
			if (tok->token != T_BRACK_CLOSE) {
				ERROR(ESQL_PARSE_TOK, EL_WARN);
				return &rs;
			}
			rs.name = zstrdup("hash");
			rs.data = strsha;
			return &rs;
		} else if (tok->token == T_FUNC_SHA256) {
			char *strsha256 = zmalloc(SHA256_SIZE+1);
			strsha256[SHA256_SIZE] = '\0';
			STACK_EMPTY_POP();
			if (tok->token != T_BRACK_OPEN) {
				ERROR(ESQL_PARSE_TOK, EL_WARN);
				return &rs;
			}
			STACK_EMPTY_POP();
			if (tok->token != T_STRING) {
				ERROR(ESQL_PARSE_TOK, EL_WARN);
				return &rs;
			}
			crypto_sha256(strsha256, tok->string);
			STACK_EMPTY_POP();
			if (tok->token != T_BRACK_CLOSE) {
				ERROR(ESQL_PARSE_TOK, EL_WARN);
				return &rs;
			}
			rs.name = zstrdup("hash");
			rs.data = strsha256;
			return &rs;
		} else if (tok->token == T_FUNC_INSTANCE_NAME) {
			STACK_EMPTY_POP();
			if (tok->token != T_BRACK_OPEN) {
				ERROR(ESQL_PARSE_TOK, EL_WARN);
				return &rs;
			}
			STACK_EMPTY_POP();
			if (tok->token != T_BRACK_CLOSE) {
				ERROR(ESQL_PARSE_TOK, EL_WARN);
				return &rs;
			}
			rs.name = zstrdup("name");
			rs.data = zstrdup(get_instance_name());
			return &rs;
		} else if (tok->token == T_FUNC_INSTANCE_KEY) {
			STACK_EMPTY_POP();
			if (tok->token != T_BRACK_OPEN) {
				ERROR(ESQL_PARSE_TOK, EL_WARN);
				return &rs;
			}
			STACK_EMPTY_POP();
			if (tok->token != T_BRACK_CLOSE) {
				ERROR(ESQL_PARSE_TOK, EL_WARN);
				return &rs;
			}
			strcpy(rs.quid, get_instance_key());
			return &rs;
		} else if (tok->token == T_FUNC_SESSION_KEY) {
			STACK_EMPTY_POP();
			if (tok->token != T_BRACK_OPEN) {
				ERROR(ESQL_PARSE_TOK, EL_WARN);
				return &rs;
			}
			STACK_EMPTY_POP();
			if (tok->token != T_BRACK_CLOSE) {
				ERROR(ESQL_PARSE_TOK, EL_WARN);
				return &rs;
			}
			strcpy(rs.quid, get_session_key());
			return &rs;
		} else if (tok->token == T_FUNC_LICENSE) {
			STACK_EMPTY_POP();
			if (tok->token != T_BRACK_OPEN) {
				ERROR(ESQL_PARSE_TOK, EL_WARN);
				return &rs;
			}
			STACK_EMPTY_POP();
			if (tok->token != T_BRACK_CLOSE) {
				ERROR(ESQL_PARSE_TOK, EL_WARN);
				return &rs;
			}
			rs.name = zstrdup("license");
			rs.data = zstrdup(LICENSE);
			return &rs;
		}
		if (tok->token != T_ASTERISK) {
			ERROR(ESQL_PARSE_TOK, EL_WARN);
			return &rs;
		}
		STACK_EMPTY_POP();
		if (tok->token != T_FROM) {
			ERROR(ESQL_PARSE_TOK, EL_WARN);
			return &rs;
		}
		STACK_EMPTY_POP();
		if (tok->token == T_STRING)
			goto selete_tablelist;
		if (tok->token != T_QUID) {
			ERROR(ESQL_PARSE_VAL, EL_WARN);
			return &rs;
		}
		rs.data = db_get(tok->string, len);
		if (rs.data)
			return &rs;
selete_tablelist:
		rs.data = db_table_get(tok->string, len);
		if (rs.data)
			return &rs;
	} else if (tok->token == T_INSERT) {
		int i = 0;
		struct objname {
			char *name;
			int length;
		} *name = NULL;
		schema_t schema = SCHEMA_ARRAY;
		STACK_EMPTY_POP();
		if (tok->token == T_TARGET) {
			STACK_EMPTY_POP();
			if (tok->token == T_QUID) {
			} else if (tok->token == T_NULL) {
			} else {
				ERROR(ESQL_PARSE_VAL, EL_WARN);
				return &rs;
			}
			cnt--;
			STACK_EMPTY_POP();
		}
		if (cnt == 1)
			schema = SCHEMA_FIELD;
		if (tok->token == T_SEPARATE)
			goto insert_arr;
		if (tok->token != T_BRACK_OPEN) {
			ERROR(ESQL_PARSE_TOK, EL_WARN);
			return &rs;
		}
		schema = SCHEMA_OBJECT;
		int name_cnt = cnt/2;
		name = zmalloc(sizeof(struct objname) * name_cnt);
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
		STACK_EMPTY_POP();
		if (tok->token != T_SEPARATE) {
			ERROR(ESQL_PARSE_TOK, EL_WARN);
			return &rs;
		}
insert_arr:
		STACK_EMPTY_POP();
		if (tok->token != T_BRACK_OPEN) {
			ERROR(ESQL_PARSE_TOK, EL_WARN);
			return &rs;
		}
		size_t slay_len = 0;
		int j = 0;
		if (name && cnt > i)
			cnt = i;
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
			} else if (tok->token == T_NULL) {
				if (name)
					next = slay_wrap(next, name[j].name, name[j].length, NULL, 0, DT_NULL);
				else
					next = slay_wrap(next, NULL, 0, NULL, 0, DT_NULL);
			}
			j++;
		}
		if (name)
			zfree(name);
		_db_put(rs.quid, slay, slay_len);
		rs.items = j;
		return &rs;
	} else if (tok->token == T_UPDATE) {
		int i = 0;
		STACK_EMPTY_POP();
		if (tok->token != T_QUID) {
			ERROR(ESQL_PARSE_VAL, EL_WARN);
			return &rs;
		}
		strcpy(rs.quid, tok->string);
		struct objname {
			char *name;
			int length;
		} *name = NULL;
		schema_t schema = SCHEMA_ARRAY;
		if (--cnt == 1)
			schema = SCHEMA_FIELD;
		STACK_EMPTY_POP();
		if (tok->token != T_SEPARATE) {
			ERROR(ESQL_PARSE_TOK, EL_WARN);
			return &rs;
		}
		STACK_EMPTY_POP();
		if (tok->token == T_SEPARATE)
			goto update_arr;
		if (tok->token != T_BRACK_OPEN) {
			ERROR(ESQL_PARSE_TOK, EL_WARN);
			return &rs;
		}
		schema = SCHEMA_OBJECT;
		int name_cnt = cnt/2;
		name = zmalloc(sizeof(struct objname) * name_cnt);
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
		STACK_EMPTY_POP();
		if (tok->token != T_SEPARATE) {
			ERROR(ESQL_PARSE_TOK, EL_WARN);
			return &rs;
		}
update_arr:
		STACK_EMPTY_POP();
		if (tok->token != T_BRACK_OPEN) {
			ERROR(ESQL_PARSE_TOK, EL_WARN);
			return &rs;
		}
		size_t slay_len = 0;
		int j = 0;
		if (name && cnt > i)
			cnt = i;
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
			} else if (tok->token == T_NULL) {
				if (name)
					next = slay_wrap(next, name[j].name, name[j].length, NULL, 0, DT_NULL);
				else
					next = slay_wrap(next, NULL, 0, NULL, 0, DT_NULL);
			}
			j++;
		}
		if (name)
			zfree(name);
		_db_update(rs.quid, slay, slay_len);
		rs.items = j;
		return &rs;
	} else if (tok->token == T_DELETE) {
		STACK_EMPTY_POP();
		if (tok->token != T_FROM) {
			ERROR(ESQL_PARSE_TOK, EL_WARN);
			return &rs;
		}
		STACK_EMPTY_POP();
		if (tok->token != T_QUID) {
			ERROR(ESQL_PARSE_VAL, EL_WARN);
			return &rs;
		}
		db_delete(tok->string);
	} else if (tok->token == T_FUNC_VACUUM) {
		STACK_EMPTY_POP();
		if (tok->token != T_BRACK_OPEN) {
			ERROR(ESQL_PARSE_TOK, EL_WARN);
			return &rs;
		}
		STACK_EMPTY_POP();
		if (tok->token != T_BRACK_CLOSE) {
			ERROR(ESQL_PARSE_TOK, EL_WARN);
			return &rs;
		}
		db_vacuum();
		return &rs;
	} else if (tok->token == T_FUNC_SYNC) {
		STACK_EMPTY_POP();
		if (tok->token != T_BRACK_OPEN) {
			ERROR(ESQL_PARSE_TOK, EL_WARN);
			return &rs;
		}
		STACK_EMPTY_POP();
		if (tok->token != T_BRACK_CLOSE) {
			ERROR(ESQL_PARSE_TOK, EL_WARN);
			return &rs;
		}
		filesync();
		return &rs;
	} else if (tok->token == T_FUNC_SHUTDOWN) {
		STACK_EMPTY_POP();
		if (tok->token != T_BRACK_OPEN) {
			ERROR(ESQL_PARSE_TOK, EL_WARN);
			return &rs;
		}
		STACK_EMPTY_POP();
		if (tok->token != T_BRACK_CLOSE) {
			ERROR(ESQL_PARSE_TOK, EL_WARN);
			return &rs;
		}
		run = 0;
		return &rs;
	} else {
		ERROR(ESQL_PARSE_TOK, EL_WARN);
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
	char *pch = strtoken(_ustr," ,");
	while(pch != NULL) {
		struct stoken *tok = (struct stoken *)tree_zmalloc(sizeof(struct stoken), NULL);
		if (!strcmp(pch, "SELECT") || !strcmp(pch, "select")) {
			tok->token = T_SELECT;
		} else if (!strcmp(pch, "INSERT") || !strcmp(pch, "insert")) {
			tok->token = T_INSERT;
		} else if (!strcmp(pch, "UPDATE") || !strcmp(pch, "update")) {
			tok->token = T_UPDATE;
		} else if (!strcmp(pch, "DELETE") || !strcmp(pch, "delete")) {
			tok->token = T_DELETE;
		} else if (!strcmp(pch, "SET") || !strcmp(pch, "set")) {
			tok->token = T_SEPARATE;
		} else if (!strcmp(pch, "INTO") || !strcmp(pch, "into")) {
			tok->token = T_TARGET;
		} else if (!strcmp(pch, "ORDER") || !strcmp(pch, "order")) {
			tok->token = T_ORDER;
		} else if (!strcmp(pch, "BY") || !strcmp(pch, "by")) {
			tok->token = T_BY;
		} else if (!strcmp(pch, "HAVING") || !strcmp(pch, "having")) {
			tok->token = T_HAVING;
		} else if (!strcmp(pch, "VALUES") || !strcmp(pch, "values")) {
			tok->token = T_SEPARATE;
		} else if (!strcmp(pch, "(")) {
			tok->token = T_BRACK_OPEN;
		} else if (!strcmp(pch, ")")) {
			tok->token = T_BRACK_CLOSE;
		} else if (!strcmp(pch, "*")) {
			tok->token = T_ASTERISK;
		} else if (!strcmp(pch, "FROM") || !strcmp(pch, "from")) {
			tok->token = T_FROM;
		} else if (!strcmp(pch, "WHERE") || !strcmp(pch, "where")) {
			tok->token = T_WHERE;
		} else if (!strcmp(pch, "AND") || !strcmp(pch, "and") || !strcmp(pch, "&&")) {
			tok->token = T_AND;
		} else if (!strcmp(pch, "OR") || !strcmp(pch, "or") || !strcmp(pch, "||")) {
			tok->token = T_OR;
		} else if (!strcmp(pch, "+")) {
			tok->token = T_AND;
		} else if (!strcmp(pch, "-")) {
			tok->token = T_SUBTRACT;
		} else if (!strcmp(pch, "/")) {
			tok->token = T_DEVIDE;
		} else if (!strcmp(pch, "%")) {
			tok->token = T_MODULO;
		} else if (!strcmp(pch, ">")) {
			tok->token = T_GREATER;
		} else if (!strcmp(pch, "<")) {
			tok->token = T_SMALLER;
		} else if (!strcmp(pch, "=")) {
			tok->token =  T_EQUAL;
		} else if (!strcmp(pch, "<=")) {
			tok->token =  T_SMALLER_EQUAL;
		} else if (!strcmp(pch, ">=")) {
			tok->token =  T_GREATER_EQUAL;
		} else if (!strcmp(pch, "!=") || !strcmp(pch, "NOT") || !strcmp(pch, "not") || !strcmp(pch, "<>")) {
			tok->token =  T_NOT;
		} else if (!strcmp(pch, "ANY") || !strcmp(pch, "any")) {
			tok->token =  T_ANY;
		} else if (!strcmp(pch, "ALL") || !strcmp(pch, "all")) {
			tok->token =  T_ALL;
		} else if (!strcmp(pch, "BETWEEN") || !strcmp(pch, "between")) {
			tok->token =  T_BETWEEN;
		} else if (!strcmp(pch, "EXISTS") || !strcmp(pch, "exists")) {
			tok->token =  T_EXISTS;
		} else if (!strcmp(pch, "IN") || !strcmp(pch, "in")) {
			tok->token =  T_IN;
		} else if (!strcmp(pch, "LIKE") || !strcmp(pch, "like")) {
			tok->token =  T_LIKE;
		} else if (!strcmp(pch, "UNIQUE") || !strcmp(pch, "unique")) {
			tok->token =  T_UNIQUE;
		} else if (!strcmp(pch, "IS") || !strcmp(pch, "is")) {
			tok->token =  T_IS;
		} else if (!strcmp(pch, "QUID") || !strcmp(pch, "quid")) {
			tok->token =  T_FUNC_QUID;
		} else if (!strcmp(pch, "NOW") || !strcmp(pch, "now")) {
			tok->token =  T_FUNC_NOW;
		} else if (!strcmp(pch, "MD5") || !strcmp(pch, "md5")) {
			tok->token =  T_FUNC_MD5;
		} else if (!strcmp(pch, "SHA1") || !strcmp(pch, "sha1")) {
			tok->token =  T_FUNC_SHA1;
		} else if (!strcmp(pch, "SHA256") || !strcmp(pch, "sha256")) {
			tok->token =  T_FUNC_SHA256;
		} else if (!strcmp(pch, "VACUUM") || !strcmp(pch, "vacuum")) {
			tok->token =  T_FUNC_VACUUM;
		} else if (!strcmp(pch, "SYNC") || !strcmp(pch, "sync")) {
			tok->token =  T_FUNC_SYNC;
		} else if (!strcmp(pch, "SHUTDOWN") || !strcmp(pch, "shutdown")) {
			tok->token =  T_FUNC_SHUTDOWN;
		} else if (!strcmp(pch, "INSTANCE_NAME") || !strcmp(pch, "instance_name")) {
			tok->token =  T_FUNC_INSTANCE_NAME;
		} else if (!strcmp(pch, "INSTANCE_KEY") || !strcmp(pch, "instance_key")) {
			tok->token =  T_FUNC_INSTANCE_KEY;
		} else if (!strcmp(pch, "SESSION_KEY") || !strcmp(pch, "session_key")) {
			tok->token =  T_FUNC_SESSION_KEY;
		} else if (!strcmp(pch, "LICENSE") || !strcmp(pch, "license")) {
			tok->token =  T_FUNC_LICENSE;
		} else if (!strcmp(pch, ";")) {
			tok->token =  T_COMMIT;
		} else if (!strcmp(pch, "TRUE") || !strcmp(pch, "true")) {
			cnt++;
			tok->token = T_TRUE;
		} else if (!strcmp(pch, "FALSE") || !strcmp(pch, "false")) {
			cnt++;
			tok->token = T_FALSE;
		} else if (!strcmp(pch, "NULL") || !strcmp(pch, "null")) {
			cnt++;
			tok->token = T_NULL;
		} else if (strisdigit(pch)) {
			charcnt += strlen(pch);
			cnt++;
			tok->token = T_INTEGER;
			char *_s = tree_zstrdup(pch, tok);
			tok->string = _s;
			tok->length = strlen(_s);
		} else if (strismatch(pch, "1234567890.")) {
			if (strccnt(pch, '.') != 1)
				goto tok_err;
			if (pch[0] == '.' || pch[strlen(pch)-1] == '.')
				goto tok_err;
			charcnt += strlen(pch);
			cnt++;
			tok->token = T_DOUBLE;
			char *_s = tree_zstrdup(pch, tok);
			tok->string = _s;
			tok->length = strlen(_s);
		} else if (strisualpha(pch)) {
			charcnt += strlen(pch);
			cnt++;
			tok->token = T_STRING;
			char *_s = tree_zstrdup(pch, tok);
			tok->string = _s;
			tok->length = strlen(_s);
		} else if ((pch[0] == '"' && pch[strlen(pch)-1] == '"') || (pch[0] == '\'' && pch[strlen(pch)-1] == '\'')) {
			charcnt += strlen(pch);
			cnt++;
			tok->token = T_STRING;
			char *_s = tree_zstrdup(pch, tok);
			_s[strlen(pch)-1] = '\0';
			_s++;
			if (strquid_format(_s)>0)
				tok->token = T_QUID;
			tok->string = _s;
			tok->length = strlen(_s);
		} else {
tok_err:
			ERROR(ESQL_TOKEN, EL_WARN);
			tree_zfree(tok);
			zfree(_ustr);
			return 0;
		}
		stack_push(stack, tok);
		pch = strtoken(NULL, " ,");
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
