#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <pthread.h>
#include <signal.h>

#include <config.h>
#include <common.h>
#include "core.h"
#include "webapi.h"

#define HEADER_SIZE		10240L
#define MAX_CLIENTS		150
#define INIT_VEC_SIZE	1024
#define VERSION_STRING	"Quantica/0.0.8 (WebAPI)"

int serversock;
void *unaccepted = NULL;

struct socket_request {
	int fd;
	socklen_t addr_len;
	struct sockaddr_in address;
	pthread_t thread;
};

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

void delete_vector(vector_t * vector) {
	unsigned int i = 0;
	for (i = 0; i < vector->size; ++i) {
		free(vector_at(vector, i));
	}
	free_vector(vector);
}

void handle_shutdown(int sigal) {
	puts("\n[info] Shutting down");

	shutdown(serversock, SHUT_RDWR);
	close(serversock);

	free(unaccepted);
	fprintf(stderr, "[info] Cleanup and detach core\n");
	detach_core();

	exit(sigal);
}


void raw_response(FILE *socket_stream, vector_t *headers, const char *status) {
    char squid[39] = {'\0'};
    generate_quid(squid);

	fprintf(socket_stream,
        "HTTP/1.1 %s\r\n"
        "Server: " VERSION_STRING "\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 0\r\n", status);

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
        "Server: " VERSION_STRING "\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %zu\r\n", status, strlen(message)+2);

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

void *handle_request(void *socket) {
	struct socket_request *request = (struct socket_request *)socket;
	unsigned int close = 0;

	FILE *socket_stream = fdopen(request->fd, "r+");
	if (!socket_stream) {
		fprintf(stderr, "[erro] Failed to get file descriptor\n");
		goto disconnect;
	};

	char str_addr[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &request->address.sin_addr, str_addr, INET_ADDRSTRLEN);

	while(1) {
		vector_t *queue = alloc_vector();
		vector_t *headers = alloc_vector();
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

			char *request_line = malloc((strlen(buf)+1) * sizeof(char));
			strcpy(request_line, buf);
			vector_append(queue, (void *)request_line);
		}

		if (feof(socket_stream)) {
			delete_vector(queue);
			delete_vector(headers);
			break;
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

		fprintf(stderr, "[info] %s - ", str_addr);
		if (request_type == HTTP_GET)
			fprintf(stderr, "GET");
		else if (request_type == HTTP_POST)
			fprintf(stderr, "POST");
		else if (request_type == HTTP_HEAD)
			fprintf(stderr, "HEAD");
		else if (request_type == HTTP_OPTIONS)
			fprintf(stderr, "OPTIONS");

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

        if (c_connection) {
            if(!strcmp(c_connection, "close")){
                vector_append(headers, strdup("Connection: close\r\n"));
                close = 1;
            } else if(!strcmp(c_connection, "keep-alive")){
                vector_append(headers, strdup("Connection: keep-alive\r\n"));
                close = 0;
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
		size_t fsz = strlen(_filename);

		if (request_type == HTTP_OPTIONS) {
		    vector_append(headers, strdup("Allow: POST,OPTIONS,GET,HEAD\r\n"));
		    raw_response(socket_stream, headers, "200 OK");
			goto done;
		}

        char *c_buf;
        if (c_length > 0) {
            size_t total_read = 0;
            c_buf = malloc(c_length);
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
                                sprintf(jsonbuf, "{\"quid\":\"%s\",\"description\":\"Data stored in record\",\"status\":\"COMMAND_OK\",\"success\":1}", squid);
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
                                sprintf(jsonbuf, "{\"hash\":\"%s\",\"description\":\"Data hashed with SHA-1\",\"status\":\"COMMAND_OK\",\"success\":1}", strsha);
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
#if 0
                } else if (!strcmp(_filename, "/meta")) {
                    char *var = strtok(c_buf, "&");
                    while(var != NULL) {
                        char *value = strchr(var, '=');
                        if (value) {
                            value[0] = '\0';
                            value++;
                            if (!strcmp(var, "quid")) {
                                int rtn = debugkey(value);
                                if (rtn<0) {
                                    json_response(socket_stream, headers, "200 OK", "{\"description\":\"Failed to show recordmeta\",\"status\":\"META_FAILED\",\"success\":0}");
                                } else {
                                    json_response(socket_stream, headers, "200 OK", "{\"description\":\"Metatdata outputted to console\",\"status\":\"COMMAND_OK\",\"success\":1}");
                                }
                                processed = 1;
                            }
                        }
                        var = strtok(NULL, "&");
                    }
                    if (!processed)
                        json_response(socket_stream, headers, "200 OK", "{\"description\":\"Requested method expects data\",\"status\":\"NO_DATA\",\"success\":0}");
                } else if (!strcmp(_filename, "/test")) {
                    char *var = strtok(c_buf, "&");
                    while(var != NULL) {
                        char *value = strchr(var, '=');
                        if (value) {
                            value[0] = '\0';
                            value++;
                            if (!strcmp(var, "quid")) {
                                int rtn = rremove(value);
                                if (rtn<0) {
                                    json_response(socket_stream, headers, "200 OK", "{\"description\":\"Failed to show recordmeta\",\"status\":\"META_FAILED\",\"success\":0}");
                                } else {
                                    json_response(socket_stream, headers, "200 OK", "{\"description\":\"Test executed\",\"status\":\"COMMAND_OK\",\"success\":1}");
                                }
                                processed = 1;
                            }
                        }
                        var = strtok(NULL, "&");
                    }
                    if (!processed)
                        json_response(socket_stream, headers, "200 OK", "{\"description\":\"Requested method expects data\",\"status\":\"NO_DATA\",\"success\":0}");
#endif
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
                sprintf(jsonbuf, "{\"instance_name\":\"%s\",\"description\":\"The database instance name\",\"status\":\"COMMAND_OK\",\"success\":1}", get_instance_name());
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
                sprintf(jsonbuf, "{\"api_version\":%d,\"db_version\":\"%s\",\"description\":\"Database and component versions\",\"status\":\"COMMAND_OK\",\"success\":1}", API_VERSION, VERSION);
                json_response(socket_stream, headers, "200 OK", jsonbuf);
            }
        } else if (!strcmp(_filename, "/stats")) {
            if (request_type == HTTP_HEAD) {
                raw_response(socket_stream, headers, "200 OK");
            } else {
                char jsonbuf[512] = {'\0'};
                sprintf(jsonbuf, "{\"statistics\":[{\"cardinality\":%lu,\"cardinality_free\":%lu,\"tablecache\":%d,\"datacache\":%d,\"datacache_density\":%d}],\"description\":\"Database statistics\",\"status\":\"COMMAND_OK\",\"success\":1}", stat_getkeys(), stat_getfreekeys(), CACHE_SLOTS, DBCACHE_SLOTS, DBCACHE_DENSITY);
                json_response(socket_stream, headers, "200 OK", jsonbuf);
            }
        } else if (fsz==37 || fsz==39) {
			size_t len;
			char rquid[39] = {'\0'};
            if (fsz==37) {
                strcpy(rquid, _filename);
                rquid[0] = '{'; rquid[37] = '}'; rquid[38] = '\0';
            } else {
                strcpy(rquid, _filename+1);
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
                    data[len] = '\0';
                    char jsonbuf[512] = {'\0'};
                    sprintf(jsonbuf, "{\"data\":\"%s\",\"description\":\"Retrieve record by requested key\",\"status\":\"COMMAND_OK\",\"success\":1}", data);
                    json_response(socket_stream, headers, "200 OK", jsonbuf);
                    free(data);
                }
			}
        } else {
            if (request_type == HTTP_HEAD)
                raw_response(socket_stream, headers, "404 Not Found");
            else
                json_response(socket_stream, headers, "404 Not Found", "{\"description\":\"API URI does not exist\",\"status\":\"NOT_FOUND\",\"success\":0}");
        }

done:
		fflush(socket_stream);
		free(_filename);
		delete_vector(queue);
		delete_vector(headers);

		if (close)
            break;
	}

disconnect:
	if (socket_stream) {
		fclose(socket_stream);
	}
	shutdown(request->fd, SHUT_RDWR);

	if (request->thread) {
		pthread_detach(request->thread);
	}
	free(request);

	return NULL;
}

void daemonize() {
    fprintf(stderr, "[info] %s %s (%s, %s)\n", PROGNAME, VERSION, __DATE__, __TIME__);
	fprintf(stderr, "[info] Starting daemon\n");

	fprintf(stderr, "[info] Start database core\n");
	start_core();

	struct sockaddr_in sin;
	serversock = socket(AF_INET, SOCK_STREAM, 0);
	sin.sin_family = AF_INET;
	sin.sin_port = htons(API_PORT);
	sin.sin_addr.s_addr = INADDR_ANY;

	int _true = 1;
	if (setsockopt(serversock, SOL_SOCKET, SO_REUSEADDR, &_true, sizeof(int)) < 0) {
		close(serversock);
		fprintf(stderr, "[erro] Failed to set socket option\n");
		return;
	}

	if (bind(serversock, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		fprintf(stderr, "[erro] Failed to bind socket to port %d\n", API_PORT);
		return;
	}

	listen(serversock, MAX_CLIENTS);
	fprintf(stderr, "[info] Listening on port %d.\n", API_PORT);
	fprintf(stderr, "[info] Server version string is " VERSION_STRING ".\n");

	signal(SIGINT, handle_shutdown);

	while (1) {
		struct socket_request *incoming = malloc(sizeof(struct socket_request));
		unsigned int c_len = sizeof(incoming->address);
		unaccepted = (void *)incoming;
		incoming->fd = accept(serversock, (struct sockaddr *) &(incoming->address), &c_len);
		unaccepted = NULL;
		pthread_create(&incoming->thread, NULL, handle_request, (void *)(incoming));
	}

	fprintf(stderr, "[info] Cleanup and detach core\n");
	detach_core();
}
