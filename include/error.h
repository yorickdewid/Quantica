#ifndef ERROR_H_INCLUDED
#define ERROR_H_INCLUDED

#include <stdint.h>

enum error_level {
	EL_DEBUG = 1,
	EL_WARN,
	EL_FATAL
};

enum error_code {
	ENO_QUID = 9,
	ENO_DATA,
	EIO_READ,
	EIO_WRITE,
	EM_ALLOC,
	EQUID_EXIST,
	EDB_LOCKED,
	EREC_LOCKED,
	EREC_NOTFOUND,
	ESQL_TOKEN,
	ESQL_PARSE_END,
	ESQL_PARSE_VAL,
	ESQL_PARSE_TOK,
	ESQL_PARSE_STCT
};

/*
 * Trace error through global structure
 */
struct error {
	enum error_code code;
	enum error_level level;
};

extern struct error _eglobal;

#define ERROR(e, l)			\
	_eglobal.code = e;		\
	_eglobal.level = l;		\

#define ISERROR()	\
	_eglobal.code ? 1 : 0

#define ERRORZEOR()			\
	_eglobal.code = 0;		\
	_eglobal.level = 0;		\

#define IFERROR(e)	\
	(_eglobal.code==e) ? 1 : 0

#define GETERROR()	\
	_eglobal.code

#endif // ERROR_H_INCLUDED
