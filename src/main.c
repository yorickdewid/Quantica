#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <config.h>
#include <common.h>
#include <log.h>
#include "webapi.h"

void print_version() {
	zprintf(PROGNAME " %s ("__DATE__", "__TIME__")\n", get_version_string());
}

void print_usage() {
	zprintf(
	    PROGNAME " %s ("__DATE__", "__TIME__")\n"
	    "Usage: " PROGNAME " [-?hvfd]\n"
	    "\nOptions:\n"
	    "  -?,-h    this help\n"
	    "  -v       show version and exit\n"
	    "  -d       run as daemon (default)\n"
	    "  -f       run in foreground\n"
	    "  -s       working directory\n"
	    , get_version_string());
}

int daemonize() {
	pid_t pid, sid;

	pid = fork();
	if (pid < 0) {
		lprint("[erro] Failed to fork into background\n");
		return 1;
	}

	if (pid > 0) {
		return 0;
	}
	umask(0);

	sid = setsid();
	if (sid < 0) {
		lprint("[erro] Failed to promote to session leader\n");
		return 1;
	}

	/* Redirect standard file descriptors */
	freopen("/dev/null", "r", stdin);
	freopen("/dev/null", "w", stdout);
	freopen("/dev/null", "w", stderr);

	/* Daemon initialization */
	start_webapi();
	return 0;
}

#ifdef DAEMON

int main(int argc, char *argv[]) {
	if (argc < 2)
		return daemonize();

	for (int i = 1; i < argc; ++i) {
		if (argv[i][0] == '-') {
			switch (argv[i][1]) {

				/* Daemonize */
				case 'D':
				case 'd':
					daemonize();
					break;

				/* Change working directory */
				case 'S':
				case 's':
					if (i + 1 >= argc)
						break;

					char *dir = argv[i + 1];
					if (!dir)
						break;

					if (chdir(dir) < 0) {
						lprint("[erro] Failed to change directory\n");
						return 1;
					}
					daemonize();
					break;

				/* Run in foreground */
				case 'F':
				case 'f':
					lprint("[info] Running in foreground\n");
					start_webapi();
					break;

				/* Show help */
				case 'H':
				case 'h':
				case '?':
					print_usage();
					break;

				/* Version info */
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

#endif
