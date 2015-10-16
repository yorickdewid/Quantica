#ifndef HTTP_PARSE_H_INCLUDED
#define HTTP_PARSE_H_INCLUDED

#include <netinet/in.h>

#define parsed_url_free(v) tree_zfree(v);

typedef enum {
	HTTP,
	HTTPS,
	WS,
	WSS,
	QDB,
	QDBS
} http_scheme_t;

/* Represents an url */
struct http_url {
	http_scheme_t scheme;		/* mandatory */
	char *host;					/* mandatory */
	unsigned short port;		/* optional */
	char *path;					/* optional */
	char *query;				/* optional */
	char *fragment;				/* optional */
	char *username;				/* optional */
	char *password;				/* optional */
};

/* Represents an HTTP html response */
struct http_response {
	struct http_url *request_uri;
	char *body;
	char *status_code;
	int status_code_int;
	char *status_text;
	char *request_headers;
	char *response_headers;
	char *rawresp;
};

struct http_url *parse_url(const char *url);

struct http_response *http_req(char *http_headers, struct http_url *purl);
struct http_response *http_get(char *url, char *custom_headers);
struct http_response *http_head(char *url, char *custom_headers);
struct http_response *http_post(char *url, char *custom_headers, char *post_data);

void http_response_free(struct http_response *hresp);

#endif // HTTP_PARSE_H_INCLUDED
