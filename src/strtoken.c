#include <stddef.h>
#include <string.h>

char *strtoken(register char *s, register const char *delim) {
	register char *spanp;
	register int c, sc;
	char *tok;
	static char *last;
	static int sk = 0;

	if (s != NULL)
		sk = 0;
	if (s == NULL && (s = last) == NULL)
		return NULL;

	/*
	 * Skip (span) leading delimiters (s += strspn(s, delim), sort of).
	 */
cont:
	c = *s++;
	if (c == '"' || c == '\'')
		sk = sk ? 0 : 1;
	for (spanp = (char *)delim; (sc = *spanp++) != 0;) {
		if (c == sc)
			goto cont;
	}

	/* no non-delimiter characters */
	if (c == 0) {
		last = NULL;
		return (NULL);
	}
	tok = s - 1;

	/*
	 * Scan token (scan for delimiters: s += strcspn(s, delim), sort of).
	 * Note that delim must have one NUL; we stop if we see that, too.
	 */
	for (;;) {
		c = *s++;
		spanp = (char *)delim;
		if (c == '"' || c == '\'')
			sk = sk ? 0 : 1;
		if (sk)
			continue;
		do {
			if ((sc = *spanp++) == c) {
				if (c == 0)
					s = NULL;
				else
					s[-1] = 0;
				last = s;
				return tok;
			}
		} while (sc != 0);
	}
}
