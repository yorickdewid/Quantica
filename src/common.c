#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

#include <config.h>
#include <common.h>

char from_hex(char ch) {
	return isdigit(ch) ? ch - '0' : tolower(ch) - 'a' + 10;
}

int8_t strisbool(char *str) {
	if (!strcmp(str, "true") || !strcmp(str, "TRUE"))
		return TRUE;
	if (!strcmp(str, "false") || !strcmp(str, "FALSE"))
		return FALSE;
	return -1;
}

char *strtolower(char *str) {
	for (; *str; ++str)
		*str = tolower(*str);
	return str;
}

char *strtoupper(char *str) {
	for (; *str; ++str)
		*str = toupper(*str);
	return str;
}

bool strisdigit(char *str) {
	for (; *str; ++str)
		if (!isdigit(*str))
			return FALSE;
	return TRUE;
}

bool strisalpha(char *str) {
	for (; *str; ++str) {
		if (!isalpha(*str))
			return FALSE;
	}
	return TRUE;
}

bool strisualpha(char *str) {
	for (; *str; ++str) {
		if (*str == '_')
			continue;
		if (!isalpha(*str))
			return FALSE;
	}
	return TRUE;
}

/* Remove first and last character from string */
char *strdtrim(char *str) {
	char *p = str;
	p++;
	p[strlen(p) - 1] = 0;
	return p;
}

/* Match string agains tokens */
bool strismatch(const char *str, const char *tok) {
	for (; *str; ++str) {
		bool flag = FALSE;
		const char *pt = tok;
		for (; *tok; ++tok)
			if (*str == *tok)
				flag = TRUE;
		tok = pt;
		if (!flag)
			return FALSE;
	}
	return TRUE;
}

/* Count charactes in string */
int strccnt(const char *str, char c) {
	int cnt = 0;
	while ((str = strchr(str, c)) != NULL) {
		cnt++;
		str++;
	}
	return cnt;
}

char *str_bool(bool b) {
	return b ? "true" : "false";
}

char *str_null() {
	return "null";
}

uint16_t _ntohs(uint16_t x) {
#if BYTE_ORDER == LITTLE_ENDIAN
	unsigned char *s = (unsigned char *)&x;
	return (uint16_t)(s[0] << 8 | s[1]);
#else
	return x;
#endif
}

uint32_t _htonl(uint32_t x) {
#if BYTE_ORDER == LITTLE_ENDIAN
	unsigned char *s = (unsigned char  *)&x;
	return (uint32_t)(s[0] << 24 | s[1] << 16 | s[2] << 8 | s[3]);
#else
	return x;
#endif
}

uint32_t _ntohl(uint32_t x) {
#if BYTE_ORDER == LITTLE_ENDIAN
	unsigned char *s = (unsigned char *)&x;
	return (uint32_t)(s[0] << 24 | s[1] << 16 | s[2] << 8 | s[3]);
#else
	return x;
#endif
}

uint16_t _htons(uint16_t x) {
#if BYTE_ORDER == LITTLE_ENDIAN
	unsigned char *s = (unsigned char *)&x;
	return (uint16_t)(s[0] << 8 | s[1]);
#else
	return x;
#endif
}

__be32 to_be32(uint32_t x) {
	return (FORCE __be32) _htonl(x);
}

__be16 to_be16(uint16_t x) {
	return (FORCE __be16) _htons(x);
}

__be64 to_be64(uint64_t x) {
#if (BYTE_ORDER == LITTLE_ENDIAN)
	return (FORCE __be64)(((uint64_t) _htonl((uint32_t) x) << 32) | _htonl((uint32_t)(x >> 32)));
#else
	return (FORCE __be64) x;
#endif
}

uint32_t from_be32(__be32 x) {
	return _ntohl((FORCE uint32_t) x);
}

uint16_t from_be16(__be16 x) {
	return _ntohs((FORCE uint16_t) x);
}

uint64_t from_be64(__be64 x) {
#if (BYTE_ORDER == LITTLE_ENDIAN)
	return ((uint64_t) _ntohl((uint32_t)(FORCE uint64_t) x) << 32) | _ntohl((uint32_t)((FORCE uint64_t) x >> 32));
#else
	return (FORCE uint64_t) x;
#endif
}

int file_exists(const char *path) {
	int fd = open(path, O_RDWR);
	if (fd > -1) {
		close(fd);
		return 1;
	}
	return 0;
}

char *get_version_string() {
	static char buf[16];
	snprintf(buf, 16, "%d.%d.%d", VERSION_RELESE, VERSION_MAJOR, VERSION_MINOR);
	return buf;
}

long get_version() {
	return sizeof(int) * VERSION_RELESE + sizeof(int) * VERSION_MAJOR + sizeof(int) * VERSION_MINOR;
}
