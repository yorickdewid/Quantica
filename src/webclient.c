#ifdef LINUX
#if __STDC_VERSION__ >= 199901L
#define _XOPEN_SOURCE 700
#else
#define _XOPEN_SOURCE 500
#endif /* __STDC_VERSION__ */
#endif // LINUX

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <errno.h>

#include <config.h>
#include <common.h>
#include <log.h>
#include <error.h>

#include "zmalloc.h"
#include "base64.h"
#include "webclient.h"

/* Retrieves the IP adress of a hostname */
//TODO IPv6
char *resolve_host(char *hostname) {
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_in *sin;
	static char ip[INET_ADDRSTRLEN];
	int rv;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC; // use AF_INET6 to force IPv6
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(hostname, NULL, &hints, &servinfo)) < 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		lprint("[erro] Cannot resolve host\n");
		return NULL;
	}

	// Loop through all the results and connect to the first we can
	for (p = servinfo; p != NULL; p = p->ai_next) {
		sin = (struct sockaddr_in *)p->ai_addr;
		if (sin->sin_family == AF_INET) {
			strcpy(ip , inet_ntoa(sin->sin_addr));
			break;
		}
	}

	freeaddrinfo(servinfo);
	return ip;
}

/*
	Check whether the character is permitted in scheme string
*/
int is_scheme_char(int c) {
	return (!isalpha(c) && '+' != c && '-' != c && '.' != c) ? 0 : 1;
}

/* Parses a specified URL and returns the struct */
struct parsed_url *parse_url(const char *url) {

	/* Define variable */
	const char *curstr;
	int len;
	int i;
	int userpass_flag;
	int bracket_flag;

	/* Allocate the parsed url storage */
	struct parsed_url *purl = (struct parsed_url *)tree_zmalloc(sizeof(struct parsed_url), NULL);
	if (!purl) {
		return NULL;
	}
	purl->port = API_PORT;
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

	/* Check restrictions */
	for (i = 0; i < len; i++) {
		if (is_scheme_char(curstr[i]) == 0) {//TODO
			parsed_url_free(purl);
			lprint("[erro] Cannot parse URL\n");
			return NULL;
		}
	}

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
	for (i = 0; i < 2; i++) {
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
	while ('\0' != *tmpstr) {
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
	curstr = tmpstr;

	/* Is port number specified? */
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

	/* Get ip */
	purl->ip = resolve_host(purl->host);

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

/*
	Handles redirect if needed for get requests
*/
struct http_response *handle_redirect_get(struct http_response* hresp, char *custom_headers) {
	unused(custom_headers);
	if (hresp->status_code_int > 300 && hresp->status_code_int < 399) {
		char *token = strtok(hresp->response_headers, "\r\n");
		while (token != NULL) {
			//if (str_contains(token, "Location:")) {
			if (strstr(token, "Location:")) {
				/* Extract url */
				//char *location = str_replace("Location: ", "", token);
				char *loc = strchr(token, ':');
				puts(loc);
				//return http_get(loc, custom_headers);
			}
			token = strtok(NULL, "\r\n");
		}
	}
	/* We're not dealing with a redirect, just return the same structure */
	return hresp;
}

/*
	Handles redirect if needed for head requests
*/
struct http_response *handle_redirect_head(struct http_response* hresp, char *custom_headers) {
	unused(custom_headers);
	if (hresp->status_code_int > 300 && hresp->status_code_int < 399) {
		char *token = strtok(hresp->response_headers, "\r\n");
		while (token != NULL) {
			//if (str_contains(token, "Location:")) {
			if (strstr(token, "Location:")) {
				/* Extract url */
				//char *location = str_replace("Location: ", "", token);
				char *loc = strchr(token, ':');
				puts(loc);
				//return http_head(location, custom_headers);
			}
			token = strtok(NULL, "\r\n");
		}
	}
	/* We're not dealing with a redirect, just return the same structure */
	return hresp;
}

/*
	Handles redirect if needed for post requests
*/
struct http_response *handle_redirect_post(struct http_response* hresp, char *custom_headers, char *post_data) {
	unused(custom_headers);
	unused(post_data);
	if (hresp->status_code_int > 300 && hresp->status_code_int < 399) {
		char *token = strtok(hresp->response_headers, "\r\n");
		while (token != NULL) {
			//if (str_contains(token, "Location:")) {
			if (strstr(token, "Location:")) {
				/* Extract url */
				//char *location = str_replace("Location: ", "", token);
				char *loc = strchr(token, ':');
				puts(loc);
				//return http_post(location, custom_headers, post_data);
			}
			token = strtok(NULL, "\r\n");
		}
	}
	/* We're not dealing with a redirect, just return the same structure */
	return hresp;
}

/*
	Makes a HTTP request and returns the response
*/
struct http_response *http_req(char *http_headers, struct parsed_url *purl) {
	/* Parse url */
	if (purl == NULL) {
		printf("Unable to parse url");
		return NULL;
	}

	/* Declare variable */
	int sock;
	int tmpres;
	//char buf[BUFSIZ + 1];
	struct sockaddr_in *remote;

	/* Allocate memeory for htmlcontent */
	struct http_response *hresp = (struct http_response*) malloc(sizeof(struct http_response));
	if (hresp == NULL) {
		printf("Unable to allocate memory for htmlcontent.");
		return NULL;
	}
	memset(hresp, 0, sizeof(struct http_response));
	/*hresp->body = NULL;
	hresp->request_headers = NULL;
	hresp->response_headers = NULL;
	hresp->status_code = NULL;
	hresp->status_text = NULL;*/

	/* Create TCP socket */
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("Can't create TCP socket");
		return NULL;
	}

	/* Set remote->sin_addr.s_addr */
	remote = (struct sockaddr_in *) malloc(sizeof(struct sockaddr_in *));
	remote->sin_family = AF_INET;
	tmpres = inet_pton(AF_INET, purl->ip, (void *)(& (remote->sin_addr.s_addr)));
	if (tmpres < 0) {
		printf("Can't set remote->sin_addr.s_addr");
		return NULL;
	} else if (tmpres == 0) {
		printf("Not a valid IP");
		return NULL;
	}
	remote->sin_port = htons(80);
	//remote->sin_port = htons(purl->port);

	/* Connect */
	if (connect(sock, (struct sockaddr *) remote, sizeof(struct sockaddr)) < 0) {
		printf("Could not connect");
		return NULL;
	}
	free(remote);

	/* Send headers to server */
	unsigned int sent = 0;
	while (sent < strlen(http_headers)) {
		tmpres = send(sock, http_headers + sent, strlen(http_headers) - sent, 0);
		if (tmpres == -1) {
			printf("Can't send headers");
			return NULL;
		}
		sent += tmpres;
	}

	// receive response
	int i = 0;
	int amntRecvd = 0;
	int currentSize = 4096;
	int oldSize = currentSize;
	hresp->rawresp = (char*) malloc(currentSize);
	while ((amntRecvd = recv(sock, hresp->rawresp + i, 4096, 0)) > 0) {
		i += amntRecvd;
		oldSize = currentSize;
		currentSize += 4096;
		char *newBuffer = malloc(currentSize);
		memcpy(newBuffer, hresp->rawresp, oldSize);
		free(hresp->rawresp);
		hresp->rawresp = newBuffer;
	}

	/* Recieve into response*/
	/*	char *response = (char*)malloc(BUFSIZ);
		char BUF[BUFSIZ];
		size_t recived_len = 0;
		while ((recived_len = recv(sock, response, BUFSIZ-1, 0)) > 0) {
			response[recived_len] = '\0';
			response = (char*)realloc(response, recived_len + strlen(BUF) + 1);
			sprintf(response, "%s%s", response, BUF);
		}
		if (recived_len < 0) {
			free(http_headers);
			free(response);
			close(sock);
			printf("Unabel to recieve");
			return NULL;
		}*/

	/* Reallocate response */
	//response = (char*)realloc(response, strlen(response) + 1);

	/* Close socket */
	close(sock);

	/* Parse status code and text */
	//char *status_line = get_until(hresp->rawresp, "\r\n");
	//printf("!!%s!!\n", hresp->rawresp);

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
	hresp->status_text = hresp->rawresp;
	//printf("<%s>\n", hresp->status_code);
	//printf("[%s]\n", hresp->response_headers);
	//char *status_line = zstrdup(hresp->rawresp);
	//status_line[strlen(status_line)-2] = '\0';
	//printf("!!%s!!\n", status_line);
	//if (!strcmp())
	//status_line = str_replace("HTTP/1.1 ", "", status_line);
	//char *status_code = str_ndup(status_line, 4);
	//status_code = str_replace(" ", "", status_code);
	//char *status_text = str_replace(status_code, "", status_line);
	//status_text = str_replace(" ", "", status_text);
	//hresp->status_code = status_line;
	//hresp->status_code_int = atoi(status_code);
	//hresp->status_text = status_line;

	/* Parse response headers */
	//char *headers = get_until(hresp->rawresp, "\r\n\r\n");
	/*char *headers = zstrdup(hresp->rawresp);
	headers[strlen(headers)-4] = '\0';
	hresp->response_headers = headers;*/

	/* Assign request headers */
	hresp->request_headers = http_headers;

	/* Assign request url */
	hresp->request_uri = purl;

	/* Return response */
	return hresp;
}

/*
	Makes a HTTP GET request to the given url
*/
struct http_response *http_get(char *url, char *custom_headers) {
	/* Parse url */
	struct parsed_url *purl = parse_url(url);
	if (!purl) {
		printf("Unable to parse url");
		return NULL;
	}

	/* Declare variable */
	char *http_headers = (char*) malloc(1024);

	/* Build query/headers */
	if (purl->path != NULL) {
		if (purl->query != NULL) {
			sprintf(http_headers, "GET /%s?%s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n", purl->path, purl->query, purl->host);
		} else {
			sprintf(http_headers, "GET /%s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n", purl->path, purl->host);
		}
	} else {
		if (purl->query != NULL) {
			sprintf(http_headers, "GET /?%s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n", purl->query, purl->host);
		} else {
			sprintf(http_headers, "GET / HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n", purl->host);
		}
	}

	/* Handle authorisation if needed */
	if (purl->username != NULL) {
		/* Format username:password pair */
		char *upwd = (char*) malloc(1024);
		sprintf(upwd, "%s:%s", purl->username, purl->password);
		upwd = (char*) realloc(upwd, strlen(upwd) + 1);

		/* Base64 encode */

		size_t encsz = base64_encode_len(strlen(upwd));
		char *base64 = (char *)zmalloc(encsz + 1);
		base64_encode(base64, upwd, strlen(upwd));
		base64[encsz] = '\0';

		//char *base64 = base64_encode(upwd);

		/* Form header */
		char *auth_header = (char*) malloc(1024);
		sprintf(auth_header, "Authorization: Basic %s\r\n", base64);
		auth_header = (char*) realloc(auth_header, strlen(auth_header) + 1);

		/* Add to header */
		http_headers = (char*) realloc(http_headers, strlen(http_headers) + strlen(auth_header) + 2);
		sprintf(http_headers, "%s%s", http_headers, auth_header);
	}

	/* Add custom headers, and close */
	if (custom_headers != NULL) {
		sprintf(http_headers, "%s%s\r\n", http_headers, custom_headers);
	} else {
		puts(http_headers);
		sprintf(http_headers, "%s\r\n", http_headers);
	}
	http_headers = (char*) realloc(http_headers, strlen(http_headers) + 1);

	/* Make request and return response */
	struct http_response *hresp = http_req(http_headers, purl);

	/* Handle redirect */
	return handle_redirect_get(hresp, custom_headers);
}

/*
	Makes a HTTP POST request to the given url
*/
struct http_response* http_post(char *url, char *custom_headers, char *post_data) {
	/* Parse url */
	struct parsed_url *purl = parse_url(url);
	if (purl == NULL) {
		printf("Unable to parse url");
		return NULL;
	}

	/* Declare variable */
	char *http_headers = (char*) malloc(1024);

	/* Build query/headers */
	if (purl->path != NULL) {
		if (purl->query != NULL) {
			sprintf(http_headers, "POST /%s?%s HTTP/1.1\r\nHost:%s\r\nConnection:close\r\nContent-Length:%zu\r\nContent-Type:application/x-www-form-urlencoded\r\n", purl->path, purl->query, purl->host, strlen(post_data));
		} else {
			sprintf(http_headers, "POST /%s HTTP/1.1\r\nHost:%s\r\nConnection:close\r\nContent-Length:%zu\r\nContent-Type:application/x-www-form-urlencoded\r\n", purl->path, purl->host, strlen(post_data));
		}
	} else {
		if (purl->query != NULL) {
			sprintf(http_headers, "POST /?%s HTTP/1.1\r\nHost:%s\r\nConnection:close\r\nContent-Length:%zu\r\nContent-Type:application/x-www-form-urlencoded\r\n", purl->query, purl->host, strlen(post_data));
		} else {
			sprintf(http_headers, "POST / HTTP/1.1\r\nHost:%s\r\nConnection:close\r\nContent-Length:%zu\r\nContent-Type:application/x-www-form-urlencoded\r\n", purl->host, strlen(post_data));
		}
	}

	/* Handle authorisation if needed */
	if (purl->username != NULL) {
		/* Format username:password pair */
		char *upwd = (char*) malloc(1024);
		sprintf(upwd, "%s:%s", purl->username, purl->password);
		upwd = (char*) realloc(upwd, strlen(upwd) + 1);

		/* Base64 encode */
		size_t encsz = base64_encode_len(strlen(upwd));
		char *base64 = (char *)zmalloc(encsz + 1);
		base64_encode(base64, upwd, strlen(upwd));
		base64[encsz] = '\0';

		//char *base64 = base64_encode(upwd);

		/* Form header */
		char *auth_header = (char*) malloc(1024);
		sprintf(auth_header, "Authorization: Basic %s\r\n", base64);
		auth_header = (char*) realloc(auth_header, strlen(auth_header) + 1);

		/* Add to header */
		http_headers = (char*) realloc(http_headers, strlen(http_headers) + strlen(auth_header) + 2);
		sprintf(http_headers, "%s%s", http_headers, auth_header);
	}

	if (custom_headers != NULL) {
		sprintf(http_headers, "%s%s\r\n", http_headers, custom_headers);
		sprintf(http_headers, "%s\r\n%s", http_headers, post_data);
	} else {
		sprintf(http_headers, "%s\r\n%s", http_headers, post_data);
	}
	http_headers = (char*) realloc(http_headers, strlen(http_headers) + 1);

	/* Make request and return response */
	struct http_response *hresp = http_req(http_headers, purl);

	/* Handle redirect */
	return handle_redirect_post(hresp, custom_headers, post_data);
}

/*
	Makes a HTTP HEAD request to the given url
*/
struct http_response *http_head(char *url, char *custom_headers) {
	/* Parse url */
	struct parsed_url *purl = parse_url(url);
	if (purl == NULL) {
		printf("Unable to parse url");
		return NULL;
	}

	/* Declare variable */
	char *http_headers = (char*) malloc(1024);

	/* Build query/headers */
	if (purl->path != NULL) {
		if (purl->query != NULL) {
			sprintf(http_headers, "HEAD /%s?%s HTTP/1.1\r\nHost:%s\r\nConnection:close\r\n", purl->path, purl->query, purl->host);
		} else {
			sprintf(http_headers, "HEAD /%s HTTP/1.1\r\nHost:%s\r\nConnection:close\r\n", purl->path, purl->host);
		}
	} else {
		if (purl->query != NULL) {
			sprintf(http_headers, "HEAD/?%s HTTP/1.1\r\nHost:%s\r\nConnection:close\r\n", purl->query, purl->host);
		} else {
			sprintf(http_headers, "HEAD / HTTP/1.1\r\nHost:%s\r\nConnection:close\r\n", purl->host);
		}
	}

	/* Handle authorisation if needed */
	if (purl->username != NULL) {
		/* Format username:password pair */
		char *upwd = (char*) malloc(1024);
		sprintf(upwd, "%s:%s", purl->username, purl->password);
		upwd = (char*) realloc(upwd, strlen(upwd) + 1);

		/* Base64 encode */
		size_t encsz = base64_encode_len(strlen(upwd));
		char *base64 = (char *)zmalloc(encsz + 1);
		base64_encode(base64, upwd, strlen(upwd));
		base64[encsz] = '\0';
		//char *base64 = base64_encode(upwd);

		/* Form header */
		char *auth_header = (char*) malloc(1024);
		sprintf(auth_header, "Authorization: Basic %s\r\n", base64);
		auth_header = (char*) realloc(auth_header, strlen(auth_header) + 1);

		/* Add to header */
		http_headers = (char*) realloc(http_headers, strlen(http_headers) + strlen(auth_header) + 2);
		sprintf(http_headers, "%s%s", http_headers, auth_header);
	}

	if (custom_headers != NULL) {
		sprintf(http_headers, "%s%s\r\n", http_headers, custom_headers);
	} else {
		sprintf(http_headers, "%s\r\n", http_headers);
	}
	http_headers = (char*) realloc(http_headers, strlen(http_headers) + 1);

	/* Make request and return response */
	struct http_response *hresp = http_req(http_headers, purl);

	/* Handle redirect */
	return handle_redirect_head(hresp, custom_headers);
}

/*
	Do HTTP OPTIONs requests
*/
struct http_response* http_options(char *url) {
	/* Parse url */
	struct parsed_url *purl = parse_url(url);
	if (purl == NULL) {
		printf("Unable to parse url");
		return NULL;
	}

	/* Declare variable */
	char *http_headers = (char*) malloc(1024);

	/* Build query/headers */
	if (purl->path != NULL) {
		if (purl->query != NULL) {
			sprintf(http_headers, "OPTIONS /%s?%s HTTP/1.1\r\nHost:%s\r\nConnection:close\r\n", purl->path, purl->query, purl->host);
		} else {
			sprintf(http_headers, "OPTIONS /%s HTTP/1.1\r\nHost:%s\r\nConnection:close\r\n", purl->path, purl->host);
		}
	} else {
		if (purl->query != NULL) {
			sprintf(http_headers, "OPTIONS/?%s HTTP/1.1\r\nHost:%s\r\nConnection:close\r\n", purl->query, purl->host);
		} else {
			sprintf(http_headers, "OPTIONS / HTTP/1.1\r\nHost:%s\r\nConnection:close\r\n", purl->host);
		}
	}

	/* Handle authorisation if needed */
	if (purl->username != NULL) {
		/* Format username:password pair */
		char *upwd = (char*) malloc(1024);
		sprintf(upwd, "%s:%s", purl->username, purl->password);
		upwd = (char*) realloc(upwd, strlen(upwd) + 1);

		/* Base64 encode */
		size_t encsz = base64_encode_len(strlen(upwd));
		char *base64 = (char *)zmalloc(encsz + 1);
		base64_encode(base64, upwd, strlen(upwd));
		base64[encsz] = '\0';
		//char *base64 = base64_encode(upwd);

		/* Form header */
		char *auth_header = (char*) malloc(1024);
		sprintf(auth_header, "Authorization: Basic %s\r\n", base64);
		auth_header = (char*) realloc(auth_header, strlen(auth_header) + 1);

		/* Add to header */
		http_headers = (char*) realloc(http_headers, strlen(http_headers) + strlen(auth_header) + 2);
		sprintf(http_headers, "%s%s", http_headers, auth_header);
	}

	/* Build headers */
	sprintf(http_headers, "%s\r\n", http_headers);
	http_headers = (char*) realloc(http_headers, strlen(http_headers) + 1);

	/* Make request and return response */
	struct http_response *hresp = http_req(http_headers, purl);

	/* Handle redirect */
	return hresp;
}

/*
	Free memory of http_response
*/
void http_response_free(struct http_response *hresp) {
	if (hresp != NULL) {
		if (hresp->request_uri != NULL)
			parsed_url_free(hresp->request_uri);
		//if(hresp->body != NULL) free(hresp->body);
		if (hresp->status_code != NULL)
			free(hresp->status_code);
		if (hresp->status_text != NULL)
			free(hresp->status_text);
		//if (hresp->request_headers != NULL)
		//	free(hresp->request_headers);
		//if (hresp->response_headers != NULL)
		//		free(hresp->response_headers);
		//if (hresp->rawresp)
		//	free(hresp->rawresp);
		free(hresp);
	}
}
