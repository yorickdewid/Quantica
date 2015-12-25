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
	if (*str == '-')
		++str;
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
	snprintf(buf, 16, "%d.%d.%d", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
	return buf;
}

long get_version() {
	return sizeof(int) * VERSION_MAJOR + sizeof(int) * VERSION_MINOR + sizeof(int) * VERSION_PATCH;
}

size_t page_align(size_t val) {
	size_t i = 1;
	while (i < val)
		i <<= 1;
	return i;
}
