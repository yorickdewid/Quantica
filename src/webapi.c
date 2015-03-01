#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <pthread.h>

#include "config.h"
#include "core.h"

#define PORT			8080
#define HEADER_SIZE		10240L
#define MAX_CLIENTS		150
#define INIT_VEC_SIZE	1024
#define VERSION_STRING	"Quantica/0.0.8 (WebAPI)"

struct socket_request {
	int fd;
	socklen_t addr_len;
	struct sockaddr_in address;
	pthread_t thread;
};

typedef struct {
	void **buffer;
	unsigned int size;
	unsigned int alloc_size;
} vector_t;

vector_t *alloc_vector() {
	vector_t *v = (vector_t *)malloc(sizeof(vector_t));
	v->buffer = (void **)malloc(INIT_VEC_SIZE * sizeof(void *));
	v->size = 0;
	v->alloc_size = INIT_VEC_SIZE;

	return v;
}

void free_vector(vector_t *v) {
	free(v->buffer);
	free(v);
}

void vector_append(vector_t *v, void *item) {
	if(v->size == v->alloc_size) {
		v->alloc_size = v->alloc_size * 2;
		v->buffer = (void **)realloc(v->buffer, v->alloc_size * sizeof(void *));
	}

	v->buffer[v->size] = item;
	v->size++;
}

void *vector_at(vector_t *v, unsigned int idx) {
	return idx >= v->size ? NULL : v->buffer[idx];
}

/*
 * Delete a vector
 * Free its contents and then it.
 */
void delete_vector(vector_t * vector) {
	unsigned int i = 0;
	for (i = 0; i < vector->size; ++i) {
		free(vector_at(vector, i));
	}
	free_vector(vector);
}

char from_hex(char ch) {
	return isdigit(ch) ? ch - '0' : tolower(ch) - 'a' + 10;
}

void json_response(FILE *socket_stream, const char *status, const char *message) {
	fprintf(socket_stream,
			"HTTP/1.1 %s\r\n"
			"Server: " VERSION_STRING "\r\n"
			"Content-Type: application/json\r\n"
			"Content-Length: %zu\r\n"
			"X-Xss-Protection: 1; mode=block\r\n"
			"Etag: 20a66c58-543a-a150-bb14-28f1a99ab04e\r\n"
			"X-QUID: {efb4bc8e-a99a-4344-9a4d-0aedab8c7074}\r\n"
			"\r\n"
			"%s\r\n", status, strlen(message), message);
}

void *handle_request(void *socket) {
	struct socket_request *request = (struct socket_request *)socket;

	FILE *socket_stream = fdopen(request->fd, "r+");
	if (!socket_stream) {
		fprintf(stderr, "[erro] Failed to get file descriptor\n");
		goto disconnect;
	};

	char str_addr[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &request->address.sin_addr, str_addr, INET_ADDRSTRLEN);

	while(1) {
		vector_t * queue = alloc_vector();
		char buf[HEADER_SIZE];
		while (!feof(socket_stream)) {
			char *in = fgets(buf, HEADER_SIZE-2, socket_stream);
			if (!in)
				break;

			if (!strcmp(in, "\r\n") || !strcmp(in,"\n"))
				break;

			if (!strstr(in, "\n")) {
				json_response(socket_stream, "400 Bad Request", "{\"api_version\":\"0.0.8\",\"quid\":\"{efb4bc8e-a99a-4344-9a4d-0aedab8c7074}\",\"description\":\"Bad request: Request line was too long\",\"status\":\"HTTP_ERROR\",\"success\":0}");
				delete_vector(queue);
				goto disconnect;
			}
			/*
			 * Store the request line in the queue for this request.
			 */
			char *request_line = malloc((strlen(buf)+1) * sizeof(char));
			strcpy(request_line, buf);
			vector_append(queue, (void *)request_line);
		}

		if (feof(socket_stream)) {
			delete_vector(queue);
			break;
		}

		char *filename = NULL;
		char * _filename = NULL;
		char *querystring = NULL;
		int request_type = 0;
		char *host = NULL;
		char *http_version = NULL;
		unsigned long c_length = 0L;
		char *c_type = NULL;
		char *c_cookie = NULL;
		char *c_uagent = NULL;
		char *c_referer = NULL;

		unsigned int i;
		for (i=0; i<queue->size; ++i) {
			char *str = (char*)(vector_at(queue, i));

			char *colon = strstr(str, ": ");
			if (!colon) {
				if (i > 0) {
					json_response(socket_stream, "400 Bad Request", "{\"api_version\":\"0.0.8\",\"quid\":\"{efb4bc8e-a99a-4344-9a4d-0aedab8c7074}\",\"description\":\"Bad request: A header line was missing colon\",\"status\":\"HTTP_ERROR\",\"success\":0}");
					delete_vector(queue);
					goto disconnect;
				}

				int r_type_width = 0;
				switch (str[0]) {
					case 'G':
						if (strstr(str, "GET ") == str) {
							r_type_width = 4;
							request_type = 1;
						} else {
							goto unsupported;
						}
						break;
					case 'P':
						if (strstr(str, "POST ") == str) {
							r_type_width = 5;
							request_type = 2;
						} else {
							goto unsupported;
						}
						break;
					case 'H':
						if (strstr(str, "HEAD ") == str) {
							r_type_width = 5;
							request_type = 3;
						} else {
							goto unsupported;
						}
						break;
					default:
						goto unsupported;
				}

				filename = str + r_type_width;
				if (filename[0] == ' ' || filename[0] == '\r' || filename[0] == '\n') {
					json_response(socket_stream, "400 Bad Request", "{\"api_version\":\"0.0.8\",\"quid\":\"{efb4bc8e-a99a-4344-9a4d-0aedab8c7074}\",\"description\":\"Bad request: No request\",\"status\":\"HTTP_ERROR\",\"success\":0}");
					delete_vector(queue);
					goto disconnect;
				}

				http_version = strstr(filename, "HTTP/");
				if (!http_version) {
					json_response(socket_stream, "400 Bad Request", "{\"api_version\":\"0.0.8\",\"quid\":\"{efb4bc8e-a99a-4344-9a4d-0aedab8c7074}\",\"description\":\"Bad request: No HTTP version supplied\",\"status\":\"HTTP_ERROR\",\"success\":0}");
					delete_vector(queue);
					goto disconnect;
				}
				http_version[-1] = '\0';
				char *tmp_newline;
				tmp_newline = strstr(http_version, "\r\n");
				if (tmp_newline)
					tmp_newline[0] = '\0';
				tmp_newline = strstr(http_version, "\n");
				if (tmp_newline)
					tmp_newline[0] = '\0';

				querystring = strstr(filename, "?");
				if (querystring) {
					querystring++;
					querystring[-1] = '\0';
				}
			} else {
				if (i == 0) {
					json_response(socket_stream, "400 Bad Request", "{\"api_version\":\"0.0.8\",\"quid\":\"{efb4bc8e-a99a-4344-9a4d-0aedab8c7074}\",\"description\":\"Bad request: First line was not a request\",\"status\":\"HTTP_ERROR\",\"success\":0}");
					delete_vector(queue);
					goto disconnect;
				}

				colon[0] = '\0';
				colon += 2;
				char * eol = strstr(colon, "\r");
				if (eol) {
					eol[0] = '\0';
					eol[1] = '\0';
				} else {
					eol = strstr(colon, "\n");
					if (eol)
						eol[0] = '\0';
				}

				if (!strcmp(str, "Host")) {
					host = colon;
				} else if (!strcmp(str, "Content-Length")) {
					c_length = atol(colon);
				} else if (!strcmp(str, "Content-Type")) {
					c_type = colon;
				} else if (!strcmp(str, "Cookie")) {
					c_cookie = colon;
				} else if (!strcmp(str, "User-Agent")) {
					c_uagent = colon;
				} else if (!strcmp(str, "Referer")) {
					c_referer = colon;
				}
			}
		}

		fprintf(stderr, "[info] %s - ", str_addr);
		if (request_type == 1)
			fprintf(stderr, "GET");
		else if (request_type == 2)
			fprintf(stderr, "POST");
		else if (request_type == 2)
			fprintf(stderr, "HEAD");

		fprintf(stderr, " %s - ", http_version);
		if (querystring)
			fprintf(stderr, "\"%s?%s\"", filename, querystring);
		else
			fprintf(stderr, "\"%s\"", filename);
		fprintf(stderr, " %lu \"%s\"", c_length, c_uagent);
		if (c_referer)
			fprintf(stderr, " \"%s\"\n", c_referer);
		else
			fprintf(stderr, "\n");

		if (!request_type) {
unsupported:
			/*
			 * We did not understand the request
			 */
			json_response(socket_stream, "501 Not Implemented", "{\"api_version\":\"0.0.8\",\"quid\":\"{efb4bc8e-a99a-4344-9a4d-0aedab8c7074}\",\"description\":\"Not implemented: The request type sent is not understood by the server\",\"status\":\"NOT_IMPLEMENTED\",\"success\":0}");
			delete_vector(queue);
			goto disconnect;
		}

		if (!filename || strstr(filename, "'") || strstr(filename," ") ||
			(querystring && strstr(querystring," "))) {
			json_response(socket_stream, "400 Bad Request", "{\"api_version\":\"0.0.8\",\"quid\":\"{efb4bc8e-a99a-4344-9a4d-0aedab8c7074}\",\"description\":\"Bad request: No filename provided\",\"status\":\"HTTP_ERROR\",\"success\":0}");
			delete_vector(queue);
			goto disconnect;
		}

		_filename = calloc(sizeof(char) * (strlen(filename) + 2), 1);
		strcat(_filename, filename);
		if (strstr(_filename, "%")) {
			char *buf = malloc(strlen(_filename) + 1);
			char *pstr = _filename;
			char *pbuf = buf;
			while (*pstr) {
				if (*pstr == '%') {
					if (pstr[1] && pstr[2]) {
						*pbuf++ = from_hex(pstr[1]) << 4 | from_hex(pstr[2]);
						pstr += 2;
					}
				} else if (*pstr == '+') {
					*pbuf++ = ' ';
				} else {
					*pbuf++ = *pstr;
				}
				pstr++;
			}
			*pbuf = '\0';
			free(_filename);
			_filename = buf;
		}

		if (request_type == 3) {
			fprintf(socket_stream, "\r\n");
			goto done;
		}

		json_response(socket_stream, "200 OK", "{\"api_version\":\"0.0.8\",\"quid\":\"{efb4bc8e-a99a-4344-9a4d-0aedab8c7074}\",\"description\":\"The server is ready to accept requests\",\"status\":\"SERVER_READY\",\"success\":1}");

done:
		fflush(socket_stream);
		delete_vector(queue);
	}

disconnect:
	if (socket_stream) {
		fclose(socket_stream);
	}
	shutdown(request->fd, 2);

	if (request->thread) {
		pthread_detach(request->thread);
	}
	free(request);

	return NULL;
}

void daemonize() {
	fprintf(stderr, "[info] Starting daemon\n");

	fprintf(stderr, "[info] Start database core\n");
	start_core();

	struct sockaddr_in sin;
	int serversock = socket(AF_INET, SOCK_STREAM, 0);
	sin.sin_family = AF_INET;
	sin.sin_port = htons(PORT);
	sin.sin_addr.s_addr = INADDR_ANY;

	int _true = 1;
	if (setsockopt(serversock, SOL_SOCKET, SO_REUSEADDR, &_true, sizeof(int)) < 0) {
		close(serversock);
		fprintf(stderr, "[erro] Failed to set socket option\n");
		return;
	}

	if (bind(serversock, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		fprintf(stderr, "[erro] Failed to bind socket to port %d\n", PORT);
		return;
	}

	listen(serversock, MAX_CLIENTS);
	printf("[info] Listening on port %d.\n", PORT);
	printf("[info] Server version string is " VERSION_STRING ".\n");

	while (1) {
		struct socket_request *incoming = malloc(sizeof(struct socket_request));
		unsigned int c_len = sizeof(incoming->address);
		void *_last_unaccepted = (void *)incoming;
		incoming->fd = accept(serversock, (struct sockaddr *) &(incoming->address), &c_len);
		_last_unaccepted = NULL;
		pthread_create(&incoming->thread, NULL, handle_request, (void *)(incoming));
	}

	fprintf(stderr, "[info] Cleanup and detach core\n");
	detach_core();
}
