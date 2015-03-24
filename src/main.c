#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <config.h>
#include <log.h>
#include "webapi.h"

void print_version() {
    printf(PROGNAME " " VERSION " (%s, %s)\n", __DATE__, __TIME__);
}

int main(int argc, char *argv[]) {
    if (argc<2)
        return daemonize();

	int i;
	for (i=1; i<argc; ++i) {
		if (argv[i][0] == '-') {
			switch(argv[i][1]) {
				case 'D':
				case 'd':
					daemonize();
					break;
				case 'V':
				case 'v':
                    print_version();
                    break;
				default:
					printf("Unknown option '-%c'\n", argv[i][1]);
			}
		}
	}

    return 0;
}
