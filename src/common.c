#include <ctype.h>

#include <config.h>
#include <common.h>

char from_hex(char ch) {
	return isdigit(ch) ? ch - '0' : tolower(ch) - 'a' + 10;
}

char *strtolower(char *str){
    for (; *str; ++str)
        *str = tolower(*str);
    return str;
}
