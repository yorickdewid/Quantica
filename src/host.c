#include <stdver.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>

#include <log.h>
#include "zmalloc.h"

/*
 * Get full qualified hostname
 */
char *get_system_fqdn() {
	struct addrinfo hints, *info, *p;
	int gai_result;
	char *name = NULL;

	char hostname[1024];
	hostname[1023] = '\0';
	gethostname(hostname, 1023);

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC; /*either IPV4 or IPV6*/
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_CANONNAME;

	if ((gai_result = getaddrinfo(hostname, "http", &hints, &info)) != 0) {
		lprintf("[erro] getaddrinfo: %s\n", gai_strerror(gai_result));
		goto done;
	}

	for (p = info; p != NULL; p = p->ai_next) {
		if (p->ai_canonname) {
			name = zstrdup(p->ai_canonname);
			goto done;
		}
	}

done:
	freeaddrinfo(info);
	return name;
}
