/*
 * Copyright (c) 2015 Quantica, Quenza
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of Quenza nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <getopt.h>
#ifdef LIBCURL
#include <curl/curl.h>
#endif

#include <config.h>
#include <common.h>
#include <log.h>
#include "../src/zmalloc.h"
#include "../src/dict.h"
#ifndef LIBCURL
#include "../src/webclient.h"
#endif

#define ASZ(a) sizeof(a)/sizeof(a[0])

#define BUFFER_SIZE		(256 * 1024)  /* 256 KB */
#define URL_SIZE		256
#define URL_FORMAT		"%s://%s:%s/sql"
#define DEF_HOST		"localhost"
#define DEF_PORT		"4017"

#define CLIENT_VERSION	"0.9.1"

char url[URL_SIZE];
static char line[1024];
static int debug = 0;
static int run = 1;
#ifdef LIBCURL
static CURL *curl = NULL;
#endif

struct write_result {
	char *data;
	int pos;
};

char *skipwhite(char *s) {
	while (isspace(*s))
		++s;
	return s;
}

#ifdef LIBCURL
static size_t write_response(void *ptr, size_t size, size_t nmemb, void *stream) {
	struct write_result *result = (struct write_result *)stream;

	if (result->pos + size * nmemb >= BUFFER_SIZE - 1) {
		fprintf(stderr, "error: buffer too small\n");
		return 0;
	}

	memcpy(result->data + result->pos, ptr, size * nmemb);
	result->pos += size * nmemb;

	return size * nmemb;
}


static void request_init() {
	curl_global_init(CURL_GLOBAL_ALL);
	curl = curl_easy_init();
	if (!curl) {
		curl_easy_cleanup(curl);
		curl_global_cleanup();
	}
}

static void request_cleanup() {
	curl_easy_cleanup(curl);
	curl_global_cleanup();
}

static char *request(const char *rurl, const char *sql) {
	CURLcode status;
	struct curl_slist *headers = NULL;
	char *data = NULL;
	long code;
	char query[URL_SIZE];

	if (!curl)
		request_init();

	data = zmalloc(BUFFER_SIZE);
	if (!data)
		goto error;

	struct write_result write_result = {
		.data = data,
		.pos = 0
	};

	/* Build the query */
	strlcpy(query, "query=", URL_SIZE);
	strlcat(query, sql, URL_SIZE);

	headers = curl_slist_append(headers, "User-Agent: Quantica CLI/" CLIENT_VERSION);
	headers = curl_slist_append(headers, "Accept: application/json");
	headers = curl_slist_append(headers, "Content-Type: application/json");

	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&write_result);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, query);
	curl_easy_setopt(curl, CURLOPT_VERBOSE, debug);
	curl_easy_setopt(curl, CURLOPT_URL, rurl);

	status = curl_easy_perform(curl);
	if (status != 0) {
		fprintf(stderr, "%s\n", curl_easy_strerror(status));
		goto error;
	}

	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
	if (code != 200) {
		fprintf(stderr, "error: server responded with code %ld\n", code);
		goto error;
	}

	curl_slist_free_all(headers);

	/* Zero-terminate the result */
	data[write_result.pos] = '\0';

	return data;

error:
	if (data)
		zfree(data);
	if (curl)
		curl_easy_cleanup(curl);
	if (headers)
		curl_slist_free_all(headers);
	curl_global_cleanup();
	return NULL;
}
#else
static char *request(const char *rurl, const char *sql) {
	char query[URL_SIZE];
	char *headers =
	    "User-Agent: Quantica CLI/" CLIENT_VERSION "\r\n"
	    "Accept: application/json\r\n"
	    "Content-Type: application/json\r\n";

	strlcpy(query, "query=", URL_SIZE);
	strlcat(query, sql, URL_SIZE);

	struct http_response *resp = http_post((char *)rurl, headers, query);
	if (!resp)
		goto error;

	if (resp->status_code != 200) {
		fprintf(stderr, "error: server responded with code %u\n", resp->status_code);
		goto error;
	}

	char *data = zstrdup(resp->body);
	http_response_free(resp);
	return data;

error:
	http_response_free(resp);
	return NULL;
}
#endif

static void local_exit() {
	run = 0;
}

struct localfunction {
	char name[64];
	char description[128];
	void (*vf)(void);
} localfunclist[] = {
	{"quit", "Exit shell", &local_exit},
};

enum dict_type {
	DICT_NULL,
	DICT_TRUE,
	DICT_FALSE,
	DICT_INT,
	DICT_STR,
	DICT_ARR,
	DICT_OBJ
};

typedef struct dict_object {
	char *name;
	void *data;
	struct dict_object **child;
	unsigned int sz;
	enum dict_type type;
} dict_object_t;

static dict_object_t *parse_object(char *data, size_t data_len, char *name, void *parent) {
	dict_parser p;
	dict_token_t t[data_len];

	dict_init(&p);
	int o = dict_parse(&p, data, data_len, t, data_len);
	if (o < 1)
		return NULL;

	dict_object_t *rtobj = (dict_object_t *)tree_zcalloc(1, sizeof(dict_object_t), parent);
	rtobj->child = (struct dict_object **)tree_zcalloc(o, sizeof(struct dict_object *), rtobj);
	if (name)
		rtobj->name = name;

	switch (t[0].type) {
		case DICT_ARRAY: {
			int i;
			rtobj->type = DICT_ARR;
			for (i = 1; i < o; ++i) {
				switch (t[i].type) {
					case DICT_PRIMITIVE:
						if (dict_cmp(data, &t[i], "null")) {
							rtobj->child[rtobj->sz] = tree_zcalloc(1, sizeof(dict_object_t), rtobj);
							rtobj->child[rtobj->sz]->type = DICT_NULL;
							rtobj->child[rtobj->sz]->child = NULL;
							rtobj->child[rtobj->sz]->sz = 0;
							rtobj->child[rtobj->sz]->name = NULL;
							rtobj->child[rtobj->sz]->data = NULL;
							rtobj->sz++;
						} else if (dict_cmp(data, &t[i], "true")) {
							rtobj->child[rtobj->sz] = tree_zcalloc(1, sizeof(dict_object_t), rtobj);
							rtobj->child[rtobj->sz]->type = DICT_TRUE;
							rtobj->child[rtobj->sz]->child = NULL;
							rtobj->child[rtobj->sz]->sz = 0;
							rtobj->child[rtobj->sz]->name = NULL;
							rtobj->child[rtobj->sz]->data = NULL;
							rtobj->sz++;
						} else if (dict_cmp(data, &t[i], "false")) {
							rtobj->child[rtobj->sz] = tree_zcalloc(1, sizeof(dict_object_t), rtobj);
							rtobj->child[rtobj->sz]->type = DICT_FALSE;
							rtobj->child[rtobj->sz]->child = NULL;
							rtobj->child[rtobj->sz]->sz = 0;
							rtobj->child[rtobj->sz]->name = NULL;
							rtobj->child[rtobj->sz]->data = NULL;
							rtobj->sz++;
						} else {
							rtobj->child[rtobj->sz] = tree_zcalloc(1, sizeof(dict_object_t), rtobj);
							rtobj->child[rtobj->sz]->type = DICT_INT;
							rtobj->child[rtobj->sz]->child = NULL;
							rtobj->child[rtobj->sz]->sz = 0;
							rtobj->child[rtobj->sz]->name = NULL;
							rtobj->child[rtobj->sz]->data = tree_zstrndup(data + t[i].start, t[i].end - t[i].start, rtobj);
							rtobj->sz++;
						}
						break;
					case DICT_STRING:
						rtobj->child[rtobj->sz] = tree_zcalloc(1, sizeof(dict_object_t), rtobj);
						rtobj->child[rtobj->sz]->type = DICT_STR;
						rtobj->child[rtobj->sz]->child = NULL;
						rtobj->child[rtobj->sz]->sz = 0;
						rtobj->child[rtobj->sz]->name = NULL;
						rtobj->child[rtobj->sz]->data = tree_zstrndup(data + t[i].start, t[i].end - t[i].start, rtobj);
						rtobj->sz++;
						break;
					case DICT_OBJECT: {
						rtobj->child[rtobj->sz] = parse_object(data + t[i].start, t[i].end - t[i].start, NULL, rtobj);
						int x, j = 0;
						for (x = 0; x < t[i].size; x++) {
							j += dict_levelcount(&t[i + 1 + j], 0, 0, NULL);
							j += dict_levelcount(&t[i + 1 + j], 0, 0, NULL);
						}
						i += j;
						rtobj->sz++;
						break;
					}
					case DICT_ARRAY: {
						rtobj->child[rtobj->sz] = parse_object(data + t[i].start, t[i].end - t[i].start, NULL, rtobj);
						int x, j = 0;
						for (x = 0; x < t[i].size; x++) {
							j += dict_levelcount(&t[i + 1 + j], 0, 0, NULL);
						}
						i += j;
						rtobj->sz++;
						break;
					}
					default:
						break;
				}
			}
			break;
		}
		case DICT_OBJECT: {
			int i;
			rtobj->type = DICT_OBJ;
			unsigned char setname = 0;
			for (i = 1; i < o; ++i) {
				switch (t[i].type) {
					case DICT_PRIMITIVE:
						if (dict_cmp(data, &t[i], "null")) {
							rtobj->child[rtobj->sz]->type = DICT_NULL;
							rtobj->sz++;
							setname = 0;
						} else if (dict_cmp(data, &t[i], "true")) {
							rtobj->child[rtobj->sz]->type = DICT_TRUE;
							rtobj->sz++;
							setname = 0;
						} else if (dict_cmp(data, &t[i], "false")) {
							rtobj->child[rtobj->sz]->type = DICT_FALSE;
							rtobj->sz++;
							setname = 0;
						} else {
							rtobj->child[rtobj->sz]->type = DICT_INT;
							rtobj->child[rtobj->sz]->data = tree_zstrndup(data + t[i].start, t[i].end - t[i].start, rtobj);
							rtobj->sz++;
							setname = 0;
						}
						break;
					case DICT_STRING:
						if (!setname) {
							rtobj->child[rtobj->sz] = tree_zcalloc(1, sizeof(dict_object_t), rtobj);
							rtobj->child[rtobj->sz]->type = DICT_STR;
							rtobj->child[rtobj->sz]->child = NULL;
							rtobj->child[rtobj->sz]->sz = 0;
							rtobj->child[rtobj->sz]->name = tree_zstrndup(data + t[i].start, t[i].end - t[i].start, rtobj);
							rtobj->child[rtobj->sz]->data = NULL;
							setname = 1;
						} else {
							rtobj->child[rtobj->sz]->data = tree_zstrndup(data + t[i].start, t[i].end - t[i].start, rtobj);
							rtobj->sz++;
							setname = 0;
						}
						break;
					case DICT_OBJECT: {
						rtobj->child[rtobj->sz] = parse_object(data + t[i].start, t[i].end - t[i].start, rtobj->child[rtobj->sz]->name, rtobj);
						int x, j = 0;
						for (x = 0; x < t[i].size; x++) {
							j += dict_levelcount(&t[i + 1 + j], 0, 0, NULL);
							j += dict_levelcount(&t[i + 1 + j], 0, 0, NULL);
						}
						i += j;
						rtobj->sz++;
						setname = 0;
						break;
					}
					case DICT_ARRAY: {
						rtobj->child[rtobj->sz] = parse_object(data + t[i].start, t[i].end - t[i].start, rtobj->child[rtobj->sz]->name, rtobj);
						int x, j = 0;
						for (x = 0; x < t[i].size; x++) {
							j += dict_levelcount(&t[i + 1 + j], 0, 0, NULL);
						}
						i += j;
						rtobj->sz++;
						setname = 0;
						break;
					}
					default:
						break;
				}
			}
			break;
		}
		default:
			break;
	}
	return rtobj;
}

static char *parse_object_get(dict_object_t *obj, const char *key) {
	if (obj->type != DICT_OBJ)
		return NULL;

	unsigned int i;
	for (i = 0; i < obj->sz; ++i) {
		if (!strcmp(obj->child[i]->name, key)) {
			switch (obj->child[i]->type) {
				case DICT_STR:
				case DICT_INT:
					return (char *)obj->child[i]->data;
				case DICT_TRUE:
					return "true";
				case DICT_FALSE:
					return "false";
				case DICT_NULL:
					return "null";
				default:
					break;
			}
		}
	}
	return NULL;
}

static void print_result(dict_object_t *obj, int depth) {
	unsigned int i;
	char *desc = NULL;

	if (!depth && obj->type != DICT_OBJ) {
		lprint("[errno] Cannot parse result\n");
		return;
	}

	if (!depth) {
		unsigned char success = 0;
		for (i = 0; i < obj->sz; ++i) {
			if (!strcmp(obj->child[i]->name, "success")) {
				if (obj->child[i]->type == DICT_TRUE) {
					success = 1;
				}
			} else if (!strcmp(obj->child[i]->name, "description")) {
				desc = (char *)obj->child[i]->data;
			}
		}

		if (!success) {
			lprintf("[errno] %s\n", desc);
			return;
		}
	}

	if (!depth)
		puts("Key\t\tValue\n---------------------");

	if (obj->type == DICT_OBJ) {
		for (i = 0; i < obj->sz; ++i) {
			if (!(!strcmp(obj->child[i]->name, "success") || !strcmp(obj->child[i]->name, "description") || !strcmp(obj->child[i]->name, "status"))) {
				switch (obj->child[i]->type) {
					case DICT_STR:
					case DICT_INT:
						printf("%s\t\t%s\n", obj->child[i]->name, (char *)obj->child[i]->data);
						break;
					case DICT_TRUE:
						printf("%s\t\ttrue\n", obj->child[i]->name);
						break;
					case DICT_FALSE:
						printf("%s\t\tfalse\n", obj->child[i]->name);
						break;
					case DICT_NULL:
						printf("%s\t\tnull\n", obj->child[i]->name);
						break;
					case DICT_OBJ:
					case DICT_ARR:
						print_result(obj->child[i], depth + 1);
						break;
					default:
						break;
				}
			}
		}
	} else {
		for (i = 0; i < obj->sz; ++i) {
			switch (obj->child[i]->type) {
				case DICT_STR:
				case DICT_INT:
					printf("\t\t\t%s\n", (char *)obj->child[i]->data);
					break;
				case DICT_TRUE:
					puts("\t\t\ttrue\n");
					break;
				case DICT_FALSE:
					puts("\t\t\tfalse\n");
					break;
				case DICT_NULL:
					puts("\t\t\tnull\n");
					break;
				case DICT_OBJ:
				case DICT_ARR:
					print_result(obj->child[i], depth + 1);
					break;
				default:
					break;
			}
		}
	}

	if (!depth)
		lprintf("\n[info] %s\n", desc);
}

/* Check if local command exist and execute it
 */
static int localcommand(char *cmd) {
	unsigned int i;
	void (*vfunc)(void);

	/* Show a list of available commands */
	if (!strcmp("help", cmd)) {
		for (i = 0; i < ASZ(localfunclist); ++i) {
			printf(" %s\t\t%s\n", localfunclist[i].name, localfunclist[i].description);
		}
		return 0;
	}
	if (!strcmp("\\q", cmd) || !strcmp("exit", cmd)) {
		run = 0;
		return 1;
	}

	for (i = 0; i < ASZ(localfunclist); ++i) {
		if (!strcmp(localfunclist[i].name, cmd)) {
			vfunc = localfunclist[i].vf;
			vfunc();
			return 1;
		}
	}
	char *text = request(url, cmd);
	if (!text)
		return 0;

	dict_object_t *rt = parse_object(text, strlen(text), NULL, NULL);
	if (!rt) {
		lprint("[errno] Failed to parse\n");
		return 1;
	}

	print_result(rt, 0);

	tree_zfree(rt);
	zfree(text);

	return 0;
}

#ifdef CLIENT

int main(int argc, char *argv[]) {
	extern char *optarg;
	extern int optind;
	extern int opterr;
	char *text;
	int c;
	int sflag = 0;
	char *host = DEF_HOST;
	char *port = DEF_PORT;
	static char usage[] = "Usage: %s [-ds] [-h host] [-p port] \n";

	opterr = 0;
	while ((c = getopt(argc, argv, "dh:p:s")) > 0) {
		switch (c) {
			case 'd':
				debug = 1;
				break;
			case 'h':
				host = optarg;
				break;
			case 'p':
				port = optarg;
				break;
			case 's':
				sflag = 1;
				break;
			default:
				fprintf(stderr, usage, argv[0]);
				return 1;
		}
	}

#ifdef LIBCURL
	request_init();
#endif

	snprintf(url, URL_SIZE, URL_FORMAT, sflag ? "https" : "http", host, port);
	text = request(url, "SELECT VERSION()");
	if (!text)
		return 1;

	dict_object_t *rt = parse_object(text, strlen(text), NULL, NULL);
	if (!rt)
		return 1;

	char *version = parse_object_get(rt, "version");
	if (!version) {
		fprintf(stderr, "error: cannot parse result\n");
		return 1;
	}
	printf("Quantica: %s\n", version);

	while (run) {
		printf(">>> ");
		fflush(NULL);

		/* Read a command line */
		if (!fgets(line, 1024, stdin))
			return 0;

		char *cmd = skipwhite(line);
		if (!strlen(cmd))
			continue;

		cmd[strlen(cmd) - 1] = '\0';
		localcommand(cmd);
	}

#ifdef LIBCURL
	request_cleanup();
#endif

	tree_zfree(rt);
	zfree(text);

	return 0;
}

#endif
