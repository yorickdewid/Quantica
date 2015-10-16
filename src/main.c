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
	printf(PROGNAME " %s ("__DATE__", "__TIME__")\n", get_version_string());
}
#include "webclient.h"
#include "zmalloc.h"
void print_usage() {
	char *hd = "GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\nUser-Agent: httpreq\r\n\r\n";
	struct http_url *purl = parse_url("http://localhost:80/");
	//struct parsed_url *purl = parse_url("http://user:passwd@127.0.0.1:33/kaas/index.php?arie=1#top");
	if (!purl)
		return;

	printf("scheme %d\n", purl->scheme);
	printf("host %s\n", purl->host);
	printf("ip %s\n", purl->ip);
	printf("port %u\n", purl->port);
	printf("path %s\n", purl->path);
	printf("query %s\n", purl->query);
	printf("fragment %s\n", purl->fragment);
	printf("username %s\n", purl->username);
	printf("password %s\n", purl->password);
	struct http_response *hrep = http_req(hd, purl);
	if (!hrep)
		return;

	printf("[CODE: %s]\n", hrep->status_text);
	printf("[HEAD: %s]\n", hrep->response_headers);
	printf("[BODY: %s]\n", hrep->body);

	http_response_free(hrep);
	//parsed_url_free(purl);

return;
	printf(
	    PROGNAME " %s ("__DATE__", "__TIME__")\n"
	    "Usage: "PROGNAME" [-?hvfd]\n"
	    "\nOptions:\n"
	    "  -?,-h    this help\n"
	    "  -v       show version and exit\n"
	    "  -d       run as daemon (default)\n"
	    "  -f       run in foreground\n"
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

#if SECURE_CHROOT
	if ((chdir("/")) < 0) {
		lprint("[erro] Failed to change directory\n");
		return 1;
	}
#endif

	/* Close out the standard file descriptors */
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	/* Daemon initialization */
	start_webapi();
	return 0;
}

int main(int argc, char *argv[]) {
	if (argc < 2)
		return daemonize();

	int i;
	for (i = 1; i < argc; ++i) {
		if (argv[i][0] == '-') {
			switch (argv[i][1]) {
				case 'D':
				case 'd':
					daemonize();
					break;
				case 'F':
				case 'f':
					lprint("[info] Running in foreground\n");
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
