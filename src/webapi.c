#ifdef LINUX
#if __STDC_VERSION__ >= 199901L
#define _XOPEN_SOURCE 700
#else
#define _XOPEN_SOURCE 500
#endif /* __STDC_VERSION__ */
#endif // LINUX

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ctype.h>
#include <signal.h>

#include <config.h>
#include <common.h>
#include <log.h>
#include "core.h"
#include "time.h"
#include "webapi.h"

#define HEADER_SIZE		10240L
#define INIT_VEC_SIZE	1024
#define VERSION_STRING	"(WebAPI)"

#define RLOGLINE_SIZE	256

int serversock4 = 0;
int serversock6 = 0;
int max_sd;
fd_set readfds;
fd_set readsock;
static unsigned long int client_requests = 0;

enum method {
    HTTP_GET = 1,
    HTTP_POST,
    HTTP_HEAD,
    HTTP_OPTIONS
};

typedef struct {
	void **buffer;
	unsigned int size;
	unsigned int alloc_size;
} vector_t;

vector_t *alloc_vector(size_t sz) {
	vector_t *v = (vector_t *)malloc(sizeof(vector_t));
	v->buffer = (void **)malloc(sz * sizeof(void *));
	v->size = 0;
	v->alloc_size = sz;

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

void delete_vector(vector_t * vector) {
	unsigned int i = 0;
	for (i = 0; i < vector->size; ++i) {
		free(vector_at(vector, i));
	}
	free_vector(vector);
}

void *get_in_addr(struct sockaddr *sa) {
	if (sa->sa_family == AF_INET)
		return &(((struct sockaddr_in*)sa)->sin_addr);
	else
		return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void handle_shutdown(int sigal) {
	lprintf("[info] Shutting down\n");

	shutdown(serversock4, SHUT_RDWR);
	close(serversock4);

	lprintf("[info] Cleanup and detach core\n");
	detach_core();

	exit(sigal);
}

void raw_response(FILE *socket_stream, vector_t *headers, const char *status) {
    char squid[39] = {'\0'};
    generate_quid(squid);

	fprintf(socket_stream,
        "HTTP/1.1 %s\r\n"
        "Server: " PROGNAME "/%s " VERSION_STRING "\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 0\r\n", status, get_version_string());

    size_t i;
    for (i=0; i<headers->size; ++i) {
			char *str = (char*)(vector_at(headers, i));
			fprintf(socket_stream, "%s", str);
    }

	fprintf(socket_stream,
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET,POST,HEAD,OPTIONS\r\n"
        "X-QUID: %s\r\n"
        "\r\n", squid);
}

void json_response(FILE *socket_stream, vector_t *headers, const char *status, const char *message) {
    char squid[39] = {'\0'};
    generate_quid(squid);

	fprintf(socket_stream,
        "HTTP/1.1 %s\r\n"
        "Server: " PROGNAME "/%s " VERSION_STRING "\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %zu\r\n", status, get_version_string(), strlen(message)+2);

    size_t i;
    for (i=0; i<headers->size; ++i) {
			char *str = (char*)(vector_at(headers, i));
			fprintf(socket_stream, "%s", str);
    }

	fprintf(socket_stream,
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET,POST,HEAD,OPTIONS\r\n"
        "X-QUID: %s\r\n"
        "\r\n"
        "%s\r\n", squid, message);
}

void handle_request(int sd, fd_set *set) {
	FILE *socket_stream = fdopen(sd, "r+");
	if (!socket_stream) {
		lprintf("[erro] Failed to get file descriptor\n");
		goto disconnect;
	};

    struct sockaddr_storage addr;
    char str_addr[INET6_ADDRSTRLEN];
    socklen_t len = sizeof(addr);
    getpeername(sd, (struct sockaddr*)&addr, &len);
	inet_ntop(addr.ss_family, get_in_addr((struct sockaddr *)&addr), str_addr, sizeof(str_addr));

	vector_t *queue = alloc_vector(INIT_VEC_SIZE);
	vector_t *headers = alloc_vector(INIT_VEC_SIZE);
	char buf[HEADER_SIZE];
	while (!feof(socket_stream)) {
		char *in = fgets(buf, HEADER_SIZE-2, socket_stream);
		if (!in)
			break;

		if (!strcmp(in, "\r\n") || !strcmp(in,"\n"))
			break;

		if (!strstr(in, "\n")) {
			raw_response(socket_stream, headers, "400 Bad Request");
			fflush(socket_stream);
			delete_vector(queue);
			delete_vector(headers);
			goto disconnect;
		}

		size_t request_sz = (strlen(buf)+1) * sizeof(char);
		char *request_line = malloc(request_sz);
		strlcpy(request_line, buf, request_sz);
		vector_append(queue, (void *)request_line);
	}

	if (feof(socket_stream)) {
		delete_vector(queue);
		delete_vector(headers);
		goto disconnect;
	}

	char *filename = NULL;
	char * _filename = NULL;
	char *querystring = NULL;
	int request_type;
	char *host = NULL;
	char *http_version = NULL;
	unsigned long c_length = 0L;
	char *c_type = NULL;
	char *c_cookie = NULL;
	char *c_uagent = NULL;
	char *c_referer = NULL;
	char *c_connection = NULL;
	char *c_buf = NULL;
	client_requests++;

	unsigned int i;
	for (i=0; i<queue->size; ++i) {
		char *str = (char*)(vector_at(queue, i));

		char *colon = strstr(str, ": ");
		if (!colon) {
			if (i > 0) {
				raw_response(socket_stream, headers, "400 Bad Request");
				fflush(socket_stream);
				delete_vector(queue);
				delete_vector(headers);
				goto disconnect;
			}

			int r_type_width = 0;
			switch (str[0]) {
				case 'G':
					if (strstr(str, "GET ") == str) {
						r_type_width = 4;
						request_type = HTTP_GET;
					} else {
						goto unsupported;
					}
					break;
				case 'P':
					if (strstr(str, "POST ") == str) {
						r_type_width = 5;
						request_type = HTTP_POST;
					} else {
						goto unsupported;
					}
					break;
				case 'H':
					if (strstr(str, "HEAD ") == str) {
						r_type_width = 5;
						request_type = HTTP_HEAD;
					} else {
						goto unsupported;
					}
					break;
				case 'O':
					if (strstr(str, "OPTIONS ") == str) {
						r_type_width = 8;
						request_type = HTTP_OPTIONS;
					} else {
						goto unsupported;
					}
					break;
				default:
					goto unsupported;
			}

			filename = str + r_type_width;
			if (filename[0] == ' ' || filename[0] == '\r' || filename[0] == '\n') {
				raw_response(socket_stream, headers, "400 Bad Request");
				fflush(socket_stream);
				delete_vector(queue);
				delete_vector(headers);
				goto disconnect;
			}

			http_version = strstr(filename, "HTTP/");
			if (!http_version) {
				raw_response(socket_stream, headers, "400 Bad Request");
				fflush(socket_stream);
				delete_vector(queue);
				delete_vector(headers);
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
				raw_response(socket_stream, headers, "400 Bad Request");
				fflush(socket_stream);
				delete_vector(queue);
				delete_vector(headers);
				goto disconnect;
			}

			colon[0] = '\0';
			colon += 2;
			char *eol = strstr(colon, "\r");
			if (eol) {
				eol[0] = '\0';
				eol[1] = '\0';
			} else {
				eol = strstr(colon, "\n");
				if (eol)
					eol[0] = '\0';
			}
			strtolower(str);
			strtolower(colon);

			if (!strcmp(str, "host")) {
				host = colon;
				(void)(host);
			} else if (!strcmp(str, "content-length")) {
				c_length = atol(colon);
			} else if (!strcmp(str, "content-type")) {
				c_type = colon;
				(void)(c_type);
			} else if (!strcmp(str, "cookie")) {
				c_cookie = colon;
				(void)(c_cookie);
			} else if (!strcmp(str, "user-agent")) {
				c_uagent = colon;
			} else if (!strcmp(str, "referer")) {
				c_referer = colon;
			} else if (!strcmp(str, "connection")) {
				c_connection = colon;
			}
		}
	}

	char logreqline[RLOGLINE_SIZE];
	memset(logreqline, 0, RLOGLINE_SIZE);
	snprintf(logreqline, RLOGLINE_SIZE, "[info] %s - ", str_addr);
	if (request_type == HTTP_GET) {
		snprintf(logreqline + strlen(logreqline), RLOGLINE_SIZE, "GET");
	} else if (request_type == HTTP_POST) {
		snprintf(logreqline + strlen(logreqline), RLOGLINE_SIZE, "POST");
	} else if (request_type == HTTP_HEAD) {
		snprintf(logreqline + strlen(logreqline), RLOGLINE_SIZE, "HEAD");
	} else if (request_type == HTTP_OPTIONS) {
		snprintf(logreqline + strlen(logreqline), RLOGLINE_SIZE, "OPTIONS");
	}

	snprintf(logreqline + strlen(logreqline), RLOGLINE_SIZE, " %s - ", http_version);
	if (querystring) {
		snprintf(logreqline + strlen(logreqline), RLOGLINE_SIZE, "\"%s?%s\"", filename, querystring);
	} else {
		snprintf(logreqline + strlen(logreqline), RLOGLINE_SIZE, "\"%s\"", filename);
	}
	snprintf(logreqline + strlen(logreqline), RLOGLINE_SIZE, " %lu \"%s\"", c_length, c_uagent);
	if (c_referer) {
		snprintf(logreqline + strlen(logreqline), RLOGLINE_SIZE, " \"%s\"\n", c_referer);
	} else {
		snprintf(logreqline + strlen(logreqline), RLOGLINE_SIZE, "\n");
	}
	lprintf(logreqline);

	if (c_connection) {
		if(!strcmp(c_connection, "close")){
			vector_append(headers, strdup("Connection: close\r\n"));
		} else if(!strcmp(c_connection, "keep-alive")){
			vector_append(headers, strdup("Connection: keep-alive\r\n"));
		}
	}

	if (!request_type) {
unsupported:
		raw_response(socket_stream, headers, "405 Method Not Allowed");
		fflush(socket_stream);
		delete_vector(queue);
		delete_vector(headers);
		goto disconnect;
	}

	if (!filename || strstr(filename, "'") || strstr(filename, " ") ||
		(querystring && strstr(querystring," "))) {
		raw_response(socket_stream, headers, "400 Bad Request");
		fflush(socket_stream);
		delete_vector(queue);
		delete_vector(headers);
		goto disconnect;
	}

	size_t _filename_sz = sizeof(char) * (strlen(filename) + 2);
	_filename = calloc(_filename_sz, 1);
	strlcat(_filename, filename, _filename_sz);
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
	size_t fsz = strlen(_filename);

	if (request_type == HTTP_OPTIONS) {
		vector_append(headers, strdup("Allow: POST,OPTIONS,GET,HEAD\r\n"));
		raw_response(socket_stream, headers, "200 OK");
		goto done;
	}

	if (c_length > 0) {
		size_t total_read = 0;
		c_buf = (char *)malloc(c_length+1);
		while ((total_read < c_length) && (!feof(socket_stream))) {
			size_t diff = c_length - total_read;
			if (diff > 1024) diff = 1024;
			size_t read = fread(c_buf, 1, diff, socket_stream);
			total_read += read;
		}
		c_buf[total_read] = '\0';
	}

	if (request_type == HTTP_POST) {
		if (c_buf) {
			int processed = 0;
			if (!strcmp(_filename, "/store")) {
				char squid[39] = {'\0'};
				char *var = strtok(c_buf, "&");
				while(var != NULL) {
					char *value = strchr(var, '=');
					if (value) {
						value[0] = '\0';
						value++;
						if (!strcmp(var, "rdata")) {
							int rtn = store(squid, value, strlen(value));
							if (rtn<0) {
								json_response(socket_stream, headers, "200 OK", "{\"description\":\"Storing data failed\",\"status\":\"STORE_FAILED\",\"success\":0}");
								goto done;
							}
							char jsonbuf[512] = {'\0'};
							snprintf(jsonbuf, 512, "{\"quid\":\"%s\",\"description\":\"Data stored in record\",\"status\":\"COMMAND_OK\",\"success\":1}", squid);
							json_response(socket_stream, headers, "200 OK", jsonbuf);
							processed = 1;
						}
					}
					var = strtok(NULL, "&");
				}
				if (!processed)
					json_response(socket_stream, headers, "200 OK", "{\"description\":\"Requested method expects data\",\"status\":\"NO_DATA\",\"success\":0}");
			} else if (!strcmp(_filename, "/replace")) {
				char *var = strtok(c_buf, "&");
				while(var != NULL) {
					char *value = strchr(var, '=');
					if (value) {
						value[0] = '\0';
						value++;
						if (!strcmp(var, "quid")) {
							int rtn = update_key(value, "value", strlen("value"));
							if (rtn<0) {
								json_response(socket_stream, headers, "200 OK", "{\"description\":\"Storing data failed\",\"status\":\"STORE_FAILED\",\"success\":0}");
								goto done;
							}
							char jsonbuf[512] = {'\0'};
							snprintf(jsonbuf, 512, "{\"description\":\"Data stored in record\",\"status\":\"COMMAND_OK\",\"success\":1}");
							json_response(socket_stream, headers, "200 OK", jsonbuf);
							processed = 1;
						}
					}
					var = strtok(NULL, "&");
				}
				if (!processed)
					json_response(socket_stream, headers, "200 OK", "{\"description\":\"Requested method expects data\",\"status\":\"NO_DATA\",\"success\":0}");
			} else if (!strcmp(_filename, "/sha1")) {
				char strsha[40] = {'\0'};
				char *var = strtok(c_buf, "&");
				while(var != NULL) {
					char *value = strchr(var, '=');
					if (value) {
						value[0] = '\0';
						value++;
						if (!strcmp(var, "rdata")) {
							int rtn = sha1(strsha, value);
							if (!rtn) {
								json_response(socket_stream, headers, "200 OK", "{\"description\":\"Hashing data failed\",\"status\":\"STORE_FAILED\",\"success\":0}");
								goto done;
							}
							char jsonbuf[512] = {'\0'};
							snprintf(jsonbuf, 512, "{\"hash\":\"%s\",\"description\":\"Data hashed with SHA-1\",\"status\":\"COMMAND_OK\",\"success\":1}", strsha);
							json_response(socket_stream, headers, "200 OK", jsonbuf);
							processed = 1;
						}
					}
					var = strtok(NULL, "&");
				}
				if (!processed)
					json_response(socket_stream, headers, "200 OK", "{\"description\":\"Requested method expects data\",\"status\":\"NO_DATA\",\"success\":0}");
			} else if (!strcmp(_filename, "/delete")) {
				char *var = strtok(c_buf, "&");
				while(var != NULL) {
					char *value = strchr(var, '=');
					if (value) {
						value[0] = '\0';
						value++;
						if (!strcmp(var, "quid")) {
							int rtn = delete(value);
							if (rtn<0) {
								json_response(socket_stream, headers, "200 OK", "{\"description\":\"Failed to delete record\",\"status\":\"DELETE_FAILED\",\"success\":0}");
							} else {
								json_response(socket_stream, headers, "200 OK", "{\"description\":\"Record deleted from storage\",\"status\":\"COMMAND_OK\",\"success\":1}");
							}
							processed = 1;
						}
					}
					var = strtok(NULL, "&");
				}
				if (!processed)
					json_response(socket_stream, headers, "200 OK", "{\"description\":\"Requested method expects data\",\"status\":\"NO_DATA\",\"success\":0}");
			} else if (!strcmp(_filename, "/update")) {
				struct microdata md;
				char *var = strtok(c_buf, "&");
				while(var != NULL) {
					char *value = strchr(var, '=');
					if (value) {
						value[0] = '\0';
						value++;
						if (!strcmp(var, "lifecycle")) {
							md.lifecycle = atoi(value);
						} else if (!strcmp(var, "importance")) {
							md.importance = atoi(value);
						} else if (!strcmp(var, "syslock")) {
							md.syslock = atoi(value);
						} else if (!strcmp(var, "exec")) {
							md.exec = atoi(value);
						} else if (!strcmp(var, "freeze")) {
							md.freeze = atoi(value);
						} else if (!strcmp(var, "error")) {
							md.error = atoi(value);
						} else if (!strcmp(var, "flag")) {
							md.type = atoi(value);
						} else if (!strcmp(var, "quid")) {
							int rtn = update(value, &md);
							if (rtn<0) {
								json_response(socket_stream, headers, "200 OK", "{\"description\":\"Failed to update record\",\"status\":\"UPDATE_FAILED\",\"success\":0}");
							} else {
								json_response(socket_stream, headers, "200 OK", "{\"description\":\"Record updated\",\"status\":\"COMMAND_OK\",\"success\":1}");
							}
							processed = 1;
						}
					}
					var = strtok(NULL, "&");
				}
				if (!processed)
					json_response(socket_stream, headers, "200 OK", "{\"description\":\"Requested method expects data\",\"status\":\"NO_DATA\",\"success\":0}");
			} else if (!strcmp(_filename, "/instance")) {
				char *var = strtok(c_buf, "&");
				while(var != NULL) {
					char *value = strchr(var, '=');
					if (value) {
						value[0] = '\0';
						value++;
						if (!strcmp(var, "name")) {
							set_instance_name(value);
							json_response(socket_stream, headers, "200 OK", "{\"description\":\"Database instance name changed\",\"status\":\"COMMAND_OK\",\"success\":1}");
							processed = 1;
						}
					}
					var = strtok(NULL, "&");
				}
				if (!processed)
					json_response(socket_stream, headers, "200 OK", "{\"description\":\"Requested method expects data\",\"status\":\"NO_DATA\",\"success\":0}");
			} else {
				json_response(socket_stream, headers, "404 Not Found", "{\"description\":\"API URI does not exist\",\"status\":\"NOT_FOUND\",\"success\":0}");
			};
		} else {
			json_response(socket_stream, headers, "200 OK", "{\"description\":\"Requested method expects data\",\"status\":\"NO_DATA\",\"success\":0}");
		}
		goto done;
	}

	if (!strcmp(_filename, "/")) {
		if (request_type == HTTP_HEAD)
			raw_response(socket_stream, headers, "200 OK");
		else
			json_response(socket_stream, headers, "200 OK", "{\"description\":\"The server is ready to accept requests\",\"status\":\"SERVER_READY\",\"success\":1}");
	} else if (!strcmp(_filename, "/license")) {
		if (request_type == HTTP_HEAD)
			raw_response(socket_stream, headers, "200 OK");
		else
			json_response(socket_stream, headers, "200 OK", "{\"license\":\"BSD\",\"description\":\"Quantica is licensed under the New BSD license\",\"status\":\"COMMAND_OK\",\"success\":1}");
	} else if (!strcmp(_filename, "/help")) {
		if (request_type == HTTP_HEAD)
			raw_response(socket_stream, headers, "200 OK");
		else
			json_response(socket_stream, headers, "200 OK", "{\"api_options\":[\"/\",\"/help\",\"/license\",\"/stats\"],\"description\":\"Available API calls\",\"status\":\"COMMAND_OK\",\"success\":1}");
	} else if (!strcmp(_filename, "/instance")) {
		if (request_type == HTTP_HEAD) {
			raw_response(socket_stream, headers, "200 OK");
		} else {
			char jsonbuf[128] = {'\0'};
			snprintf(jsonbuf, 128, "{\"instance_name\":\"%s\",\"description\":\"The database instance name\",\"status\":\"COMMAND_OK\",\"success\":1}", get_instance_name());
			json_response(socket_stream, headers, "200 OK", jsonbuf);
		}
	} else if (!strcmp(_filename, "/vacuum")) {
		if (request_type == HTTP_HEAD) {
			raw_response(socket_stream, headers, "200 OK");
		} else {
			if(vacuum()<0) {
				json_response(socket_stream, headers, "200 OK", "{\"description\":\"Vacuum failed\",\"status\":\"VACUUM_FAILED\",\"success\":0}");
			} else {
				json_response(socket_stream, headers, "200 OK", "{\"description\":\"Vacuum succeeded\",\"status\":\"COMMAND_OK\",\"success\":1}");
			}
		}
	} else if (!strcmp(_filename, "/version")) {
		if (request_type == HTTP_HEAD) {
			raw_response(socket_stream, headers, "200 OK");
		} else {
			char jsonbuf[512] = {'\0'};
			snprintf(jsonbuf, 512, "{\"api_version\":%d,\"db_version\":\"%s\",\"description\":\"Database and component versions\",\"status\":\"COMMAND_OK\",\"success\":1}", API_VERSION, get_version_string());
			json_response(socket_stream, headers, "200 OK", jsonbuf);
		}
	} else if (!strcmp(_filename, "/status")) {
		if (request_type == HTTP_HEAD) {
			raw_response(socket_stream, headers, "200 OK");
		} else {
			char jsonbuf[512] = {'\0'};
			char suptime[32];
			snprintf(jsonbuf, 512, "{\"cardinality\":%lu,\"cardinality_free\":%lu,\"tablecache\":%d,\"datacache\":%d,\"datacache_density\":%d,\"uptime\":\"%s\",\"client_requests\":%lu,\"description\":\"Database statistics\",\"status\":\"COMMAND_OK\",\"success\":1}", stat_getkeys(), stat_getfreekeys(), CACHE_SLOTS, DBCACHE_SLOTS, DBCACHE_DENSITY, get_uptime(suptime, 32), client_requests);
			json_response(socket_stream, headers, "200 OK", jsonbuf);
		}
	} else if (fsz==37 || fsz==39) {
		size_t len;
		char rquid[39] = {'\0'};
		if (fsz==37) {
			strlcpy(rquid, _filename, 39);
			rquid[0] = '{'; rquid[37] = '}'; rquid[38] = '\0';
		} else {
			strlcpy(rquid, _filename+1, 39);
		}
		char *data = request_quid(rquid, &len);
		if (data==NULL) {
			if (request_type == HTTP_HEAD)
				raw_response(socket_stream, headers, "200 OK");
			else
				json_response(socket_stream, headers, "200 OK", "{\"description\":\"The requested key does not exist\",\"status\":\"QUID_NOT_FOUND\",\"success\":0}");
		} else{
			if (request_type == HTTP_HEAD) {
				raw_response(socket_stream, headers, "200 OK");
			} else {
				data = realloc(data, len+1);
				data[len] = '\0';
				char jsonbuf[512] = {'\0'};
				snprintf(jsonbuf, 512, "{\"data\":\"%s\",\"description\":\"Retrieve record by requested key\",\"status\":\"COMMAND_OK\",\"success\":1}", data);
				json_response(socket_stream, headers, "200 OK", jsonbuf);
				free(data);
			}
		}
	} else {
		if (request_type == HTTP_HEAD) {
			raw_response(socket_stream, headers, "404 Not Found");
		} else {
			json_response(socket_stream, headers, "404 Not Found", "{\"description\":\"API URI does not exist\",\"status\":\"NOT_FOUND\",\"success\":0}");
		}
	}

done:
	fflush(socket_stream);
	if(c_buf)
		free(c_buf);
	free(_filename);
	delete_vector(queue);
	delete_vector(headers);

disconnect:
	if (socket_stream) {
		fclose(socket_stream);
	}
	shutdown(sd, SHUT_RDWR);
    FD_CLR(sd, set);

	return;
}

int start_webapi() {
	start_core();

    lprintf("[info] " PROGNAME " %s ("__DATE__", "__TIME__")\n",get_version_string());
    lprintf("[info] Current time: %lld\n", get_timestamp());
	lprintf("[info] Starting daemon\n");
	lprintf("[info] Start database core\n");

	struct addrinfo hints, *servinfo, *p;
	memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, itoa(API_PORT), &hints, &servinfo) != 0) { //TODO itoa
		lprintf("[erro] Failed to get address info\n");
		return 1;
    }

	for(p=servinfo; p!=NULL; p=p->ai_next) {
		if (p->ai_family == AF_INET) {
			if (serversock4 == 0) {
				serversock4 = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
				if (serversock4 < 0) {
					lprintf("[erro] Failed to create socket4\n");
					return 1;
				}

				int _true = 1;
				if (setsockopt(serversock4, SOL_SOCKET, SO_REUSEADDR, &_true, sizeof(int)) < 0) {
					close(serversock4);
					lprintf("[erro] Failed to set socket option\n");
					return 1;
				}

			#ifdef NONBLOCKING
				int opts = fcntl(serversock4, F_GETFL);
				if (opts < 0){
					lprintf("[erro] Failed to set nonblock on sock4\n");
					return 1;
				}

				opts = (opts | O_NONBLOCK);
				if (fcntl(serversock4, F_SETFL, opts) < 0){
					lprintf("[erro] Failed to set nonblock on sock4\n");
					return 1;
				}
			#endif // NONBLOCKING

				if (bind(serversock4, p->ai_addr, p->ai_addrlen) < 0) {
					lprintf("[erro] Failed to bind socket to port %d\n", API_PORT);
					close(serversock4);
					return 1;
				}
			}
		} else if (p->ai_family == AF_INET6) {
			if (serversock6 == 0) {
				serversock6 = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
				if (serversock6 < 0) {
					lprintf("[erro] Failed to create socket6\n");
					return 1;
				}

				int _true = 1;
				if (setsockopt(serversock6, SOL_SOCKET, SO_REUSEADDR, &_true, sizeof(int)) < 0) {
					close(serversock6);
					lprintf("[erro] Failed to set socket option\n");
					return 1;
				}

				if (setsockopt(serversock6, IPPROTO_IPV6, IPV6_V6ONLY, &_true, sizeof(int)) < 0) {
					close(serversock6);
					lprintf("[erro] Failed to set socket option\n");
					return 1;
				}

			#ifdef NONBLOCKING
				int opts = fcntl(serversock6, F_GETFL);
				if (opts < 0){
					lprintf("[erro] Failed to set nonblock on sock6\n");
					return 1;
				}

				opts = (opts | O_NONBLOCK);
				if (fcntl(serversock6, F_SETFL, opts) < 0){
					lprintf("[erro] Failed to set nonblock on sock6\n");
					return 1;
				}
			#endif // NONBLOCKING

				if (bind(serversock6, p->ai_addr, p->ai_addrlen) < 0) {
					lprintf("[erro] Failed to bind socket to port %d\n", API_PORT);
					close(serversock6);
					return 1;
				}
			}
		}
	}

	freeaddrinfo(servinfo);

	if (serversock4 == 0 && serversock6 == 0) {
		lprintf("[erro] Failed to bind any protocol\n");
		return 1;
	}

	if (serversock4 != 0) {
		if (listen(serversock4, SOMAXCONN) < 0) {
			lprintf("[erro] Failed to listen on socket4\n");
			return 1;
		}
		lprintf("[info] Listening on 0.0.0.0:%d\n", API_PORT);
	}

	if (serversock6 != 0) {
		if (listen(serversock6, SOMAXCONN) < 0) {
			lprintf("[erro] Failed to listen on socket6\n");
			return 1;
		}
		lprintf("[info] Listening on :::%d\n", API_PORT);
	}
	lprintf("[info] Server version " PROGNAME "/%s " VERSION_STRING "\n", get_version_string());

	signal(SIGINT, handle_shutdown);

    FD_ZERO(&readfds);
    FD_SET(serversock4, &readfds);
    FD_SET(serversock6, &readfds);
    max_sd = serversock4;
    if (serversock6 > max_sd)
		max_sd = serversock6;

    while (1) {
        int sd;
        readsock = readfds;
        if (select(max_sd+1, &readsock, NULL, NULL, NULL) < 0) {
            lprintf("[erro] Failed to select socket\n"); //TODO EINTR
            return 1;
        }
        for (sd=0; sd<=max_sd; ++sd) {
            if (FD_ISSET(sd, &readsock)) {
                if (sd == serversock4 || sd == serversock6) {
					struct sockaddr_storage addr;
                    socklen_t size = sizeof(struct sockaddr_storage);
                    int nsock = accept(sd, (struct sockaddr*)&addr, &size);
                    if (nsock == -1) {
                        lprintf("[erro] Failed to acccept connection\n");
                        return 1;
                    }
					FD_SET(nsock, &readfds);
					if (nsock > max_sd) {
						max_sd = nsock;
					}
                } else {
                    handle_request(sd, &readfds);
                }
            }
        }
    }

    close(serversock4);
    close(serversock6);

	lprintf("[info] Cleanup and detach core\n");
	detach_core();
	return 0;
}
