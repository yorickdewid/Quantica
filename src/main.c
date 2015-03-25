#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <config.h>
#include <log.h>
#include "webapi.h"

void print_version() {
	puts(PROGNAME " " VERSION " ("__DATE__", "__TIME__")");
}

void print_usage() {
	puts(
		PROGNAME " " VERSION " ("__DATE__", "__TIME__")\n"
		"Usage: "PROGNAME" [-?hvfd]\n"
		"\nOptions:\n"
		"  -?,-h         : this help\n"
		"  -v            : show version and exit\n"
		"  -d            : run as daemon (default)\n"
		"  -f            : run in foreground"
	);
}

int daemonize() {
	pid_t pid, sid;

	pid = fork();
	if (pid < 0) {
		return 1;
	}

	if (pid > 0) {
		return 0;
	}
	umask(0);

	sid = setsid();
	if (sid < 0) {
		/* Log the failure */
		return 1;
	}

#if 0
	if ((chdir("/")) < 0) {
			/* Log the failure */
			return 1;
	}
#endif

	/* Close out the standard file descriptors */
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	/* Daemon-specific initialization goes here */
	start_webapi();
	return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2)
        return daemonize();

	int i;
	for (i=1; i<argc; ++i) {
		if (argv[i][0] == '-') {
			switch(argv[i][1]) {
				case 'D':
				case 'd':
					daemonize();
					break;
				case 'F':
				case 'f':
					lprintf("[info] Running in foreground\n");
					start_webapi();
					break;
				case 'H':
				case 'h':
				case '?':
					print_usage();
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
