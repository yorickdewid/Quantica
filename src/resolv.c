#ifdef LINUX
#if __STDC_VERSION__ >= 199901L
#define _XOPEN_SOURCE 700
#else
#define _XOPEN_SOURCE 500
#endif /* __STDC_VERSION__ */
#endif // LINUX

#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <config.h>
#include <common.h>
#include <log.h>
#include <error.h>

#include "zmalloc.h"
#include "resolv.h"

#ifdef RESOLV

/* Retrieves the IP adress of a hostname */
int resolve_host(char *hostname, char *ip) {
	struct addrinfo hints, *servinfo = NULL;
	int proto = 0;
	int rv;

	char *_htmp = (char *)zstrdup(hostname);
	char *host = _htmp;
	if (host[0] == '[' && host[4] == ']')
		host = strdtrim(host);

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(host, NULL, &hints, &servinfo)) < 0) {
		lprint("[erro] Cannot resolve host\n");
		zfree(_htmp);
		return 0;
	}

	// Loop through all the results and connect to the first we can
	struct addrinfo *p = servinfo;
	for (; p != NULL; p = p->ai_next) {
		void *ptr = NULL;
		switch (p->ai_family) {
			case AF_INET:
				ptr = &((struct sockaddr_in *)p->ai_addr)->sin_addr;
				break;
			case AF_INET6:
				ptr = &((struct sockaddr_in6 *)p->ai_addr)->sin6_addr;
				break;
			default:
				break;
		}
		inet_ntop(p->ai_family, ptr, ip, INET6_ADDRSTRLEN);
		proto = p->ai_family;
		break;
	}

	zfree(_htmp);
	freeaddrinfo(servinfo);
	return proto;
}

#endif
