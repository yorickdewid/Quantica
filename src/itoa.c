#define INT_DIGITS 19

char *itoa(long i) {
	static char buf[INT_DIGITS + 2];
	char *p = buf + INT_DIGITS + 1;
	if (i >= 0) {
		do {
			*--p = '0' + (i % 10);
			i /= 10;
		} while (i != 0);
		return p;
	} else {
		do {
			*--p = '0' - (i % 10);
			i /= 10;
		} while (i != 0);
		*--p = '-';
	}
	return p;
}
