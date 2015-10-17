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

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <getopt.h>
#include <jansson.h>
#ifdef LIBCURL
#include <curl/curl.h>
#endif

#include <config.h>
#include <common.h>
#include <log.h>
#include "../src/zmalloc.h"
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

static json_t *parse_response(char *text) {
	json_error_t error;
	json_t *root = json_loads(text, 0, &error);
	zfree(text);

	if (!root) {
		fprintf(stderr, "error: on line %d: %s\n", error.line, error.text);
		goto err;
	}

	if (!json_is_object(root)) {
		fprintf(stderr, "error: cannot parse result\n");
		goto err;
	}

	json_t *desc = json_object_get(root, "description");
	if (!json_is_string(desc)) {
		fprintf(stderr, "error: cannot parse result\n");
		goto err;
	}

	json_t *success = json_object_get(root, "success");
	if (!json_is_boolean(success)) {
		fprintf(stderr, "error: cannot parse result\n");
		goto err;
	}

	if (json_is_false(success)) {
		fprintf(stderr, "error: %s\n", json_string_value(desc));
		goto err;
	}
	json_object_del(root, "success");
	json_object_del(root, "description");
	json_object_del(root, "status");

	return root;

err:
	json_decref(root);

	return NULL;
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

	json_t *json = parse_response(text);
	if (!json)
		return 1;

	char *rs = json_dumps(json, JSON_ENSURE_ASCII);
	puts(rs);

	json_decref(json);
	zfree(rs);

	return 0;
}

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

	json_t *json = parse_response(text);
	if (!json)
		return 1;

	json_t *version = json_object_get(json, "version");
	if (!json_is_string(version)) {
		fprintf(stderr, "error: cannot parse result\n");
		return 1;
	}
	printf("Quantica: %s\n", json_string_value(version));

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

	return 0;
}
