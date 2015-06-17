#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <config.h>
#include <common.h>
#include "sql.h"

#define STACK_SZ	15

enum token {
	/* actions */
	T_SELECT,
	T_INSERT,
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
	/* data */
	T_INTEGER,
	T_DOUBLE,
	T_STRING,
	T_INVALID,
};

struct stoken {
	enum token token;
	union {
		char *string;
		int integer;
		double dbl;
	} u;
};

typedef struct {
	void **buf;
	unsigned int size;
} stack_t;

stack_t tokenstream;

void stack_push(struct stoken *token) {
	if (tokenstream.size == STACK_SZ)
		tokenstream.buf = realloc(tokenstream.buf, (2*tokenstream.size) * sizeof(void *));
	tokenstream.buf[tokenstream.size++] = token;
}

void parse() {
	unsigned int i;
	for (i=0; i<tokenstream.size; ++i) {
		struct stoken *tok = tokenstream.buf[i];
		if (tok->token == T_STRING)
			printf("[%d] %d (%s)\n", i, tok->token, tok->u.string);
		else if (tok->token == T_INTEGER)
			printf("[%d] %d (%d)\n", i, tok->token, tok->u.integer);
		else if (tok->token == T_DOUBLE)
			printf("[%d] %d (%f)\n", i, tok->token, tok->u.dbl);
		else
			printf("[%d] %d\n", i, tok->token);
	}
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
	char *_sql = malloc(strlen(sql)+pad+1);
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

int tokenize(char sql[]) {
	char *_ustr = explode_sql(sql);
	puts(_ustr);
	char *pch = strtok(_ustr," ,");
	while(pch != NULL) {
		if (!strcmp(pch, "SELECT")) {
			struct stoken *tok = (struct stoken *)malloc(sizeof(struct stoken));
			tok->token = T_SELECT;
			stack_push(tok);
		} else if (!strcmp(pch, "INSERT")) {
			struct stoken *tok = (struct stoken *)malloc(sizeof(struct stoken));
			tok->token = T_INSERT;
			stack_push(tok);
		} else if (!strcmp(pch, "INTO")) {
		} else if (!strcmp(pch, "VALUES")) {
			struct stoken *tok = (struct stoken *)malloc(sizeof(struct stoken));
			tok->token = T_SEPARATE;
			stack_push(tok);
		} else if (!strcmp(pch, "(")) {
			struct stoken *tok = (struct stoken *)malloc(sizeof(struct stoken));
			tok->token = T_BRACK_OPEN;
			stack_push(tok);
		} else if (!strcmp(pch, ")")) {
			struct stoken *tok = (struct stoken *)malloc(sizeof(struct stoken));
			tok->token = T_BRACK_CLOSE;
			stack_push(tok);
		} else if (!strcmp(pch, "*")) {
			struct stoken *tok = (struct stoken *)malloc(sizeof(struct stoken));
			tok->token = T_ALL;
			stack_push(tok);
		} else if (!strcmp(pch, "FROM")) {
			struct stoken *tok = (struct stoken *)malloc(sizeof(struct stoken));
			tok->token = T_FROM;
			stack_push(tok);
		} else if (!strcmp(pch, "WHERE")) {
			struct stoken *tok = (struct stoken *)malloc(sizeof(struct stoken));
			tok->token = T_WHERE;
			stack_push(tok);
		} else if (!strcmp(pch, "AND")) {
			struct stoken *tok = (struct stoken *)malloc(sizeof(struct stoken));
			tok->token = T_AND;
			stack_push(tok);
		} else if (!strcmp(pch, "OR")) {
			struct stoken *tok = (struct stoken *)malloc(sizeof(struct stoken));
			tok->token = T_OR;
			stack_push(tok);
		} else if (!strcmp(pch, ">")) {
			struct stoken *tok = (struct stoken *)malloc(sizeof(struct stoken));
			tok->token = T_GREATER;
			stack_push(tok);
		} else if (!strcmp(pch, "<")) {
			struct stoken *tok = (struct stoken *)malloc(sizeof(struct stoken));
			tok->token = T_SMALLER;
			stack_push(tok);
		} else if (!strcmp(pch, "=")) {
			struct stoken *tok = (struct stoken *)malloc(sizeof(struct stoken));
			tok->token =  T_EQUAL;
			stack_push(tok);
		} else if (!strcmp(pch, ";")) {
			struct stoken *tok = (struct stoken *)malloc(sizeof(struct stoken));
			tok->token =  T_COMMIT;
			stack_push(tok);
		} else if (strisdigit(pch)) {
			struct stoken *tok = (struct stoken *)malloc(sizeof(struct stoken));
			tok->token =  T_INTEGER;
			tok->u.integer = atoi(pch);
			stack_push(tok);
		} else if (strismatch(pch, "1234567890.")) {
			if (strccnt(pch, '.') == 1) {
				if (pch[0] != '.' && pch[strlen(pch)-1] != '.') {
					struct stoken *tok = (struct stoken *)malloc(sizeof(struct stoken));
					tok->token =  T_DOUBLE;
					tok->u.dbl = atof(pch);
					stack_push(tok);
				}
			}
		} else if (strisualpha(pch)) {
			struct stoken *tok = (struct stoken *)malloc(sizeof(struct stoken));
			tok->token =  T_STRING;
			tok->u.string = pch;
			stack_push(tok);
		} else if ((pch[0] == '"' && pch[strlen(pch)-1] == '"') ||	(pch[0] == '\'' && pch[strlen(pch)-1] == '\'')) {
			struct stoken *tok = (struct stoken *)malloc(sizeof(struct stoken));
			tok->token =  T_STRING;
			char *_s = strdup(pch);
			_s[strlen(pch)-1] = '\0';
			_s++;
			tok->u.string = _s;
			stack_push(tok);
		} else {
			printf("Unexpected token '%s'\n", pch);
			return 0;
		}

		pch = strtok(NULL, " ,");
	}
	return 1;
}

#if 0
int main(int argc, char *argv[]) {
	tokenstream.buf = malloc(STACK_SZ * sizeof(void *));
	tokenstream.size = 0;
	if (argc < 2) {
		char sql[] = "INSERT INTO kaas_e (id,kaas,worst,'blub',\"arie\") VALUES (12,'blub','wurst',ja,nee)";
		if (tokenize(sql))
			parse();
	} else
		if (tokenize(argv[1]))
			parse();

	return 0;
}
#endif
