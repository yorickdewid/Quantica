#include <stdver.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <signal.h>

#include <errno.h>

#include <config.h>
#include <common.h>
#include <log.h>
#include <error.h>

#include "zmalloc.h"
#include "base64.h"
#include "webclient.h"

/* Parses a specified URL and returns the struct */
struct http_url *parse_url(const char *url) {
	/* Define variable */
	const char *curstr;
	int len;
	int userpass_flag;
	int bracket_flag;

	/* Allocate the parsed url storage */
	struct http_url *purl = (struct http_url *)tree_zmalloc(sizeof(struct http_url), NULL);
	if (!purl) {
		return NULL;
	}
	memset(purl, 0, sizeof(struct http_url));
	purl->port = 80;
	curstr = url;

	/*
	 * <scheme>:<scheme-specific-part>
	 * <scheme> := [a-z\+\-\.]+
	 *             upper case = lower case for resiliency
	 */
	/* Read scheme */
	const char *tmpstr = strchr(curstr, ':');
	if (!tmpstr) {
		parsed_url_free(purl);
		lprint("[erro] Cannot parse URL\n");
		return NULL;
	}

	/* Get the scheme length */
	len = tmpstr - curstr;

	/* Copy the scheme to the storage */
	char *_tscheme = (char *)tree_zmalloc(len + 1, purl);
	if (!_tscheme) {
		parsed_url_free(purl);
		lprint("[erro] Cannot parse URL\n");
		return NULL;
	}

	strncpy(_tscheme, curstr, len);
	_tscheme[len] = '\0';
	strtolower(_tscheme);

	switch (_tscheme[0]) {
		case 'h':
			if (!strcmp(_tscheme, "http")) {
				purl->scheme = HTTP;
#ifdef SECURE
			} else if (!strcmp(_tscheme, "https")) {
				purl->scheme = HTTPS;
#endif
			} else {
				parsed_url_free(purl);
				lprint("[erro] Invalid scheme\n");
				return NULL;
			}
			break;
#ifdef WEBSOCKET
		case 'w':
			if (!strcmp(_tscheme, "ws")) {
				purl->scheme = WS;
#ifdef SECURE
			} else if (!strcmp(_tscheme, "wss")) {
				purl->scheme = WSS;
#endif
			} else {
				parsed_url_free(purl);
				lprint("[erro] Invalid scheme\n");
				return NULL;
			}
			break;
#endif
		case 'q':
			if (!strcmp(_tscheme, "qdb")) {
				purl->scheme = QDB;
#ifdef SECURE
			} else if (!strcmp(_tscheme, "qdbs")) {
				purl->scheme = QDBS;
#endif
			} else {
				parsed_url_free(purl);
				lprint("[erro] Invalid scheme\n");
				return NULL;
			}
			break;
		default:
			parsed_url_free(purl);
			lprint("[erro] Invalid scheme\n");
			return NULL;
	}

	/* Stepover ':' */
	tmpstr++;
	curstr = tmpstr;

	/*
	 * //<user>:<password>@<host>:<port>/<url-path>
	 * Any ":", "@" and "/" must be encoded.
	 */
	/* Eat "//" */
	for (int i = 0; i < 2; i++) {
		if (*curstr != '/') {
			parsed_url_free(purl);
			lprint("[erro] Cannot parse URL\n");
			return NULL;
		}
		curstr++;
	}

	/* Check if the user (and password) are specified */
	userpass_flag = 0;
	tmpstr = curstr;
	while (*tmpstr != '\0') {
		if (*tmpstr == '@') {
			userpass_flag = 1;
			break;
		} else if ('/' == *tmpstr) {
			userpass_flag = 0;
			break;
		}
		tmpstr++;
	}

	/* User and password specification */
	tmpstr = curstr;
	if (userpass_flag) {
		while (*tmpstr != '\0' && *tmpstr != ':' && *tmpstr != '@') {
			tmpstr++;
		}
		len = tmpstr - curstr;
		purl->username = (char *)tree_zmalloc(len + 1, purl);
		if (!purl->username) {
			parsed_url_free(purl);
			lprint("[erro] Cannot parse URL\n");
			return NULL;
		}
		strncpy(purl->username, curstr, len);
		purl->username[len] = '\0';

		/* Proceed current pointer */
		curstr = tmpstr;
		if (*curstr == ':') {
			curstr++;

			tmpstr = curstr;
			while (*tmpstr != '\0' && *tmpstr != '@') {
				tmpstr++;
			}
			len = tmpstr - curstr;

			purl->password = (char *)tree_zmalloc(len + 1, purl);
			if (!purl->password) {
				parsed_url_free(purl);
				lprint("[erro] Cannot parse URL\n");
				return NULL;
			}
			strncpy(purl->password, curstr, len);
			purl->password[len] = '\0';
			curstr = tmpstr;
		}
		/* Skip '@' */
		if (*curstr != '@') {
			parsed_url_free(purl);
			lprint("[erro] Cannot parse URL\n");
			return NULL;
		}
		curstr++;
	}

	if (*curstr == '[') {
		bracket_flag = 1;
	} else {
		bracket_flag = 0;
	}

	/* Proceed on by delimiters with reading host */
	tmpstr = curstr;
	while (*tmpstr != '\0') {
		if (bracket_flag && *tmpstr == ']') {
			tmpstr++;
			break;
		} else if (!bracket_flag && (*tmpstr == ':' || *tmpstr == '/')) {
			break;
		}
		tmpstr++;
	}
	len = tmpstr - curstr;

	purl->host = (char *)tree_zmalloc(len + 1, purl);
	if (!purl->host || len <= 0) {
		parsed_url_free(purl);
		lprint("[erro] Cannot parse URL\n");
		return NULL;
	}
	strncpy(purl->host, curstr, len);
	purl->host[len] = '\0';

	/* Is port number specified? */
	curstr = tmpstr;
	if (*curstr == ':') {
		curstr++;

		tmpstr = curstr;
		while (*tmpstr != '\0' && *tmpstr != '/') {
			tmpstr++;
		}
		len = tmpstr - curstr;

		purl->port = antoi(curstr, len);
		curstr = tmpstr;
	}

	/* End of the string */
	if (*curstr == '\0') {
		return purl;
	}

	/* Skip '/' */
	if (*curstr != '/') {
		parsed_url_free(purl);
		lprint("[erro] Cannot parse URL\n");
		return NULL;
	}
	curstr++;

	/* Parse path */
	tmpstr = curstr;
	while (*tmpstr != '\0' && *tmpstr != '#' && *tmpstr != '?') {
		tmpstr++;
	}
	len = tmpstr - curstr;

	purl->path = (char *)tree_zmalloc(len + 1, purl);
	if (!purl->path) {
		parsed_url_free(purl);
		lprint("[erro] Cannot parse URL\n");
		return NULL;
	}
	strncpy(purl->path, curstr, len);
	purl->path[len] = '\0';
	curstr = tmpstr;

	/* Is query specified? */
	if (*curstr == '?') {
		curstr++;
		tmpstr = curstr;
		while (*tmpstr != '\0' && *tmpstr != '#') {
			tmpstr++;
		}
		len = tmpstr - curstr;

		purl->query = (char *)tree_zmalloc(len + 1, purl);
		if (!purl->query) {
			parsed_url_free(purl);
			lprint("[erro] Cannot parse URL\n");
			return NULL;
		}
		strncpy(purl->query, curstr, len);
		purl->query[len] = '\0';
		curstr = tmpstr;
	}

	/* Is fragment specified? */
	if (*curstr == '#') {
		curstr++;
		tmpstr = curstr;
		while (*tmpstr != '\0') {
			tmpstr++;
		}
		len = tmpstr - curstr;

		purl->fragment = (char *)tree_zmalloc(len + 1, purl);
		if (!purl->fragment) {
			parsed_url_free(purl);
			lprint("[erro] Cannot parse URL\n");
			return NULL;
		}
		strncpy(purl->fragment, curstr, len);
		purl->fragment[len] = '\0';
		curstr = tmpstr;
	}
	return purl;
}

/* Handle redirects */
struct http_response *handle_redirect(struct http_response *hresp, char *custom_headers, char *data, int head, struct http_response * (*callbk)(char *, char *, char *, int)) {
	if (hresp->status_code > 300 && hresp->status_code < 400) {
		char *token = strtok(hresp->response_headers, "\r\n");
		while (token) {
			size_t i = strlen(token) + 1;
			char _tok[i];
			strncpy(_tok, token, i);
			strtolower(_tok);
			if (strstr(_tok, "location:")) {
				char *loc = strchr(token, ':');
				loc++;
				if (loc[0] == ' ')
					loc++;

				/* Copy to local buffer */
				char _loc[strlen(loc) + 1];
				strncpy(_loc, loc, i);
				http_response_free(hresp);
				return callbk(_loc, custom_headers, data, head);
			}
			token = strtok(NULL, "\r\n");
		}
	}
	return hresp;
}

/* Create the actual HTTP request */
struct http_response *http_req(char *http_headers, struct http_url *purl) {
	/* Parse url */
	if (!purl) {
		lprint("[erro] Cannot parse URL\n");
		return NULL;
	}

	signal(SIGPIPE, SIG_IGN);

	/* Declare variable */
	int sock = -1;
	int tmpres;

	/* Allocate memeory for htmlcontent */
	struct http_response *hresp = (struct http_response *)zmalloc(sizeof(struct http_response));
	if (!hresp) {
		lprint("[erro] Unable to allocate memory\n");
		return NULL;
	}
	memset(hresp, 0, sizeof(struct http_response));

	struct addrinfo hints = {
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM
	};

	/* Take care of IPv6 adresses */
	if (purl->host[0] == '[' && purl->host[strlen(purl->host) - 1] == ']') {
		purl->host++;
		purl->host[strlen(purl->host) - 1] = '\0';
	}

	struct addrinfo *servinfo;
	if (getaddrinfo(purl->host, "http", &hints, &servinfo) < 0) {
		http_response_free(hresp);
		lprint("[erro] Cannot resolve host\n");
		return NULL;
	}

	// Loop through all the results and connect to the first we can
	int rt;
	struct addrinfo *p = servinfo;
	for (; p != NULL; p = p->ai_next) {
		sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (p->ai_addr->sa_family == AF_INET) {
			struct sockaddr_in *in = (struct sockaddr_in *)p->ai_addr;
			in->sin_port = htons(purl->port);
		} else {
			struct sockaddr_in6 *in = (struct sockaddr_in6 *)p->ai_addr;
			in->sin6_port = htons(purl->port);
		}
		if (!(rt = connect(sock, p->ai_addr, p->ai_addrlen)))
			break;
	}
	freeaddrinfo(servinfo);

	if (sock < 0 || rt < 0) {
		http_response_free(hresp);
		lprint("[erro] Cannot connect to host\n");
		return NULL;
	}

	/* Timeout in seconds */
	struct timeval tv;
	memset(&tv, 0, sizeof(struct timeval));
	tv.tv_sec = 10;

	setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (struct timeval *)&tv, sizeof(struct timeval));
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *)&tv, sizeof(struct timeval));

	/* Send headers to server */
	unsigned int sent = 0;
	while (sent < strlen(http_headers)) {
		tmpres = send(sock, http_headers + sent, strlen(http_headers) - sent, 0);
		if (tmpres < 0) {
			http_response_free(hresp);
			lprint("[erro] Cannot write to host\n");
			return NULL;
		}
		sent += tmpres;
	}

	/* Receive response */
	int i = 0;
	int amnt_recvd = 0;
	int curr_sz = 4096;
	int old_sz = curr_sz;
	hresp->rawresp = (char *)zmalloc(curr_sz);
	memset(hresp->rawresp, 0, curr_sz);
	while ((amnt_recvd = recv(sock, hresp->rawresp + i, 4096, 0)) > 0) {
		i += amnt_recvd;
		old_sz = curr_sz;
		curr_sz += 4096;

		char *new_buf = zmalloc(curr_sz);
		memset(new_buf, 0, curr_sz);
		memcpy(new_buf, hresp->rawresp, old_sz);
		free(hresp->rawresp);
		hresp->rawresp = new_buf;
	}

	/* Close socket */
	close(sock);

	if (!strlen(hresp->rawresp)) {
		http_response_free(hresp);
		lprint("[erro] Invalid response\n");
		return NULL;
	}

	/* Parse body */
	char *body = strstr(hresp->rawresp, "\r\n\r\n");
	body[0] = '\0';
	body += 4;
	hresp->body = body;

	/* Parse response headers and status code */
	char *headers = strstr(hresp->rawresp, "\r\n");
	headers[0] = '\0';
	headers += 2;
	hresp->response_headers = headers;

	/* Parse status code */
	char *pch = strchr(hresp->rawresp, ' ');
	if (pch) {
		if (pch[0] == ' ')
			pch++;
		hresp->status_code = antoi(pch, 3);
	}

	/* Assign request headers */
	hresp->request_headers = http_headers;

	/* Assign request url */
	hresp->request_uri = purl;

	/* Return response */
	return hresp;
}

/* Perform HTTP GET/POST/HEAD request */
struct http_response *http_get(char *url, char *custom_headers, char *data, int head) {
	struct http_url *purl = parse_url(url);
	if (!purl) {
		printf("Unable to parse url");
		return NULL;
	}

	/* Declare variable */
	char *http_headers = (char *)zmalloc(1024);

	/* Build query/headers */
	if (head) {
		if (purl->path) {
			if (purl->query) {
				sprintf(http_headers, "HEAD /%s?%s HTTP/1.1\r\nHost: %s\r\nAccept: */*\r\nConnection: close\r\n", purl->path, purl->query, purl->host);
			} else {
				sprintf(http_headers, "HEAD /%s HTTP/1.1\r\nHost: %s\r\nAccept: */*\r\nConnection: close\r\n", purl->path, purl->host);
			}
		} else {
			if (purl->query) {
				sprintf(http_headers, "HEAD /?%s HTTP/1.1\r\nHost: %s\r\nAccept: */*\r\nConnection: close\r\n", purl->query, purl->host);
			} else {
				sprintf(http_headers, "HEAD / HTTP/1.1\r\nHost: %s\r\nAccept: */*\r\nConnection: close\r\n", purl->host);
			}
		}
	} else if (data) {
		if (purl->path) {
			if (purl->query) {
				sprintf(http_headers, "POST /%s?%s HTTP/1.1\r\nHost: %s\r\nAccept: */*\r\nContent-Length: %zu\r\nContent-Type: application/x-www-form-urlencoded\r\n", purl->path, purl->query, purl->host, strlen(data));
			} else {
				sprintf(http_headers, "POST /%s HTTP/1.1\r\nHost: %s\r\nAccept: */*\r\nContent-Length: %zu\r\nContent-Type: application/x-www-form-urlencoded\r\n", purl->path, purl->host, strlen(data));
			}
		} else {
			if (purl->query) {
				sprintf(http_headers, "POST /?%s HTTP/1.1\r\nHost: %s\r\nAccept: */*\r\nContent-Length: %zu\r\nContent-Type: application/x-www-form-urlencoded\r\n", purl->query, purl->host, strlen(data));
			} else {
				sprintf(http_headers, "POST / HTTP/1.1\r\nHost: %s\r\nAccept: */*\r\nContent-Length: %zu\r\nContent-Type: application/x-www-form-urlencoded\r\n", purl->host, strlen(data));
			}
		}
	} else {
		if (purl->path) {
			if (purl->query) {
				sprintf(http_headers, "GET /%s?%s HTTP/1.1\r\nHost: %s\r\nAccept: */*\r\n", purl->path, purl->query, purl->host);
			} else {
				sprintf(http_headers, "GET /%s HTTP/1.1\r\nHost: %s\r\nAccept: */*\r\n", purl->path, purl->host);
			}
		} else {
			if (purl->query) {
				sprintf(http_headers, "GET /?%s HTTP/1.1\r\nHost: %s\r\nAccept: */*\r\n", purl->query, purl->host);
			} else {
				sprintf(http_headers, "GET / HTTP/1.1\r\nHost: %s\r\nAccept: */*\r\n", purl->host);
			}
		}
	}

	/* Handle authorisation if needed */
	if (purl->username) {
		/* Format username:password pair */
		char *upwd = (char *)zmalloc(1024);
		sprintf(upwd, "%s:%s", purl->username, purl->password);
		upwd = (char *)zrealloc(upwd, strlen(upwd) + 1);

		/* Base64 encode */
		size_t encsz = base64_encode_len(strlen(upwd));
		char *base64 = (char *)zmalloc(encsz + 1);
		base64_encode(base64, upwd, strlen(upwd));
		base64[encsz] = '\0';

		/* Form header */
		char *auth_header = (char *)zmalloc(1024);
		sprintf(auth_header, "Authorization: Basic %s\r\n", base64);
		auth_header = (char *)zrealloc(auth_header, strlen(auth_header) + 1);

		/* Add to header */
		size_t nsz = strlen(http_headers) + strlen(auth_header) + 2;
		http_headers = (char *)zrealloc(http_headers, nsz);
		strncat(http_headers, auth_header, nsz);
	}

	/* Add custom headers, and close */
	if (!head && data) {
		if (custom_headers) {
			strcat(http_headers, custom_headers);
			strcat(http_headers, "\r\n");
			strcat(http_headers, data);
		} else {
			strcat(http_headers, "\r\n");
		}
	} else {
		if (custom_headers) {
			strcat(http_headers, custom_headers);
			strcat(http_headers, "\r\n");
		} else {
			strcat(http_headers, "\r\n");
		}
	}
	http_headers = (char *)zrealloc(http_headers, strlen(http_headers) + 1);

	/* Make request and return response */
	struct http_response *hresp = http_req(http_headers, purl);
	if (!hresp) {
		parsed_url_free(purl);
		zfree(http_headers);
		return NULL;
	}

	/* Handle redirect */
	return handle_redirect(hresp, custom_headers, data, head, &http_get);
}

/* Perform HTTP OPTIONS request */
struct http_response *http_options(char *url) {
	struct http_url *purl = parse_url(url);
	if (!purl) {
		lprintf("[erro] Unable to parse url");
		return NULL;
	}

	/* Declare variable */
	char *http_headers = (char *)zmalloc(1024);

	/* Build query/headers */
	if (purl->path != NULL) {
		if (purl->query != NULL) {
			sprintf(http_headers, "OPTIONS /%s?%s HTTP/1.1\r\nHost: %s\r\nAccept: */*\r\n", purl->path, purl->query, purl->host);
		} else {
			sprintf(http_headers, "OPTIONS /%s HTTP/1.1\r\nHost: %s\r\nAccept: */*\r\n", purl->path, purl->host);
		}
	} else {
		if (purl->query != NULL) {
			sprintf(http_headers, "OPTIONS/?%s HTTP/1.1\r\nHost: %s\r\nAccept: */*\r\n", purl->query, purl->host);
		} else {
			sprintf(http_headers, "OPTIONS / HTTP/1.1\r\nHost: %s\r\nAccept: */*\r\n", purl->host);
		}
	}

	/* Handle authorisation if needed */
	if (purl->username) {
		/* Format username:password pair */
		char *upwd = (char *)zmalloc(1024);
		sprintf(upwd, "%s:%s", purl->username, purl->password);
		upwd = (char *)zrealloc(upwd, strlen(upwd) + 1);

		/* Base64 encode */
		size_t encsz = base64_encode_len(strlen(upwd));
		char *base64 = (char *)zmalloc(encsz + 1);
		base64_encode(base64, upwd, strlen(upwd));
		base64[encsz] = '\0';

		/* Form header */
		char *auth_header = (char *)zmalloc(1024);
		sprintf(auth_header, "Authorization: Basic %s\r\n", base64);
		auth_header = (char *)zrealloc(auth_header, strlen(auth_header) + 1);

		/* Add to header */
		size_t nsz = strlen(http_headers) + strlen(auth_header) + 2;
		http_headers = (char *)zrealloc(http_headers, nsz);
		strncat(http_headers, auth_header, nsz);
	}

	/* Build headers */
	strcat(http_headers, "\r\n");
	http_headers = (char *)zrealloc(http_headers, strlen(http_headers) + 1);

	/* Make request and return response */
	struct http_response *hresp = http_req(http_headers, purl);

	/* Handle redirect */
	return hresp;
}

/* Free memory of http_response */
void http_response_free(struct http_response *hresp) {
	if (hresp) {
		parsed_url_free(hresp->request_uri);
		free(hresp->request_headers);
		free(hresp->rawresp);
		free(hresp);
	}
}
