#include <string.h>

#include "zmalloc.h"

char *stresc(char *src, size_t *_len) {
	char c;
	size_t len = strlen(src);
	size_t nlen = len + 1;
	char *psrc = src;

	for (; *src; ++src) {
		if (*src == '\"' ||
		        *src == '\a' ||
		        *src == '\b' ||
		        *src == '\t' ||
		        *src == '\v' ||
		        *src == '\r' ||
		        *src == '\f' ||
		        *src == '\n' ||
		        *src == '\\')
			nlen++;
	}
	src = psrc;

	char *dst = (char *)zmalloc(nlen);
	if (!dst)
		return NULL;
	*_len = nlen;
	char *pdst = dst;
	memcpy(dst, src, len);

	while ((c = *(src++))) {
		switch (c) {
			case '\a':
				*(dst++) = '\\';
				*(dst++) = 'a';
				break;
			case '\b':
				*(dst++) = '\\';
				*(dst++) = 'b';
				break;
			case '\t':
				*(dst++) = '\\';
				*(dst++) = 't';
				break;
			case '\n':
				*(dst++) = '\\';
				*(dst++) = 'n';
				break;
			case '\v':
				*(dst++) = '\\';
				*(dst++) = 'v';
				break;
			case '\f':
				*(dst++) = '\\';
				*(dst++) = 'f';
				break;
			case '\r':
				*(dst++) = '\\';
				*(dst++) = 'r';
				break;
			case '\\':
				*(dst++) = '\\';
				*(dst++) = '\\';
				break;
			case '\"':
				*(dst++) = '\\';
				*(dst++) = '\"';
				break;
			default:
				*(dst++) = c;
		}
	}
	*dst = '\0';
	dst = pdst;

	return dst;
}
