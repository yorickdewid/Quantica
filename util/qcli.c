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
#include <curl/curl.h>

#define ASZ(a) sizeof(a)/sizeof(a[0])

#define BUFFER_SIZE		(256 * 1024)  /* 256 KB */
#define URL_SIZE		256
#define URL_FORMAT		"%s://%s:%s/sql"
#define DEF_HOST		"localhost"
#define DEF_PORT		"4017"

char url[URL_SIZE];
static char line[1024];
static int debug = 0;
static int run = 1;
static CURL *curl = NULL;

struct write_result {
	char *data;
	int pos;
};

char *skipwhite(char *s) {
	while (isspace(*s))
		++s;
	return s;
}

static size_t write_response(void *ptr, size_t size, size_t nmemb, void *stream) {
	struct write_result *result = (struct write_result *)stream;

	if(result->pos + size * nmemb >= BUFFER_SIZE - 1) {
		fprintf(stderr, "error: too small buffer\n");
		return 0;
	}

	memcpy(result->data + result->pos, ptr, size * nmemb);
	result->pos += size * nmemb;

	return size * nmemb;
}

void request_init() {
	curl_global_init(CURL_GLOBAL_ALL);
	curl = curl_easy_init();
	if(!curl) {
		curl_easy_cleanup(curl);
		curl_global_cleanup();
	}
}

void request_cleanup() {
	curl_easy_cleanup(curl);
	curl_global_cleanup();
}


static char *request(const char *url, const char *sql) {
	CURLcode status;
	struct curl_slist *headers = NULL;
	char *data = NULL;
	long code;
	char query[URL_SIZE];

	if(!curl)
		request_init();

	data = malloc(BUFFER_SIZE);
	if(!data)
		goto error;

	struct write_result write_result = {
		.data = data,
		.pos = 0
	};

	/* Build the query */
	strcpy(query, "query=");
	strcat(query, sql);

	headers = curl_slist_append(headers, "User-Agent: Quantica CLI");
	headers = curl_slist_append(headers, "Accept: application/json");
	headers = curl_slist_append(headers, "Content-Type: application/json");

	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &write_result);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, query);
	curl_easy_setopt(curl, CURLOPT_VERBOSE, debug);
	curl_easy_setopt(curl, CURLOPT_URL, url);

	status = curl_easy_perform(curl);
	if (status != 0) {
		fprintf(stderr, "error: unable to request data from %s:\n", url);
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
	if(data)
		free(data);
	if(curl)
		curl_easy_cleanup(curl);
	if(headers)
		curl_slist_free_all(headers);
	curl_global_cleanup();
	return NULL;
}

void local_exit() {
	run = 0;
}

struct localfunction {
	char name[64];
	char description[128];
	void (*vf)(void);
} localfunclist[] = {
	{"quit", "Exit shell", &local_exit},
};

json_t *parse(char *text) {
	json_error_t error;
	json_t *root = json_loads(text, 0, &error);
	free(text);

	if (!root) {
		fprintf(stderr, "error: on line %d: %s\n", error.line, error.text);
		return NULL;
	}

	if(!json_is_object(root)) {
		fprintf(stderr, "error: cannot parse result\n");
		json_decref(root);
		return NULL;
	}

	json_t *desc = json_object_get(root, "description");
	if(!json_is_string(desc)) {
		fprintf(stderr, "error: cannot parse result\n");
		return NULL;
	}

	json_t *success = json_object_get(root, "success");
	if (!json_is_boolean(success)) {
		fprintf(stderr, "error: cannot parse result\n");
		return NULL;
	}

	if (json_is_false(success)) {
		fprintf(stderr, "error: %s\n", json_string_value(desc));
		return NULL;
	}
	json_object_del(root, "success");
	json_object_del(root, "description");
	json_object_del(root, "status");

	return root;
}

/* Check if local command exist and execute it
 */
static int localcommand(char *cmd) {
	unsigned int i;
	void (*vfunc)(void);
	for (i=0; i<ASZ(localfunclist); ++i) {
		/* Show a list of available commands */
		if (!strcmp("help", cmd)) {
			printf(" %s\t\t%s\n", localfunclist[i].name, localfunclist[i].description);
			continue;
		}
		if (!strcmp("\\q", cmd) || !strcmp("exit", cmd)) {
			run = 0;
			return 1;
		}
		if (!strcmp(localfunclist[i].name, cmd)) {
			vfunc = localfunclist[i].vf;
			vfunc();
			return 1;
		}
	}
	char *text = request(url, cmd);
	if(!text)
		return 0;

	json_t *json = parse(text);
	if (!json)
		return 1;

	puts(json_dumps(json, JSON_ENSURE_ASCII));
	return 0;
}

int main(int argc, char *argv[]) {
	extern char *optarg;
	extern int optind;
	char *text;
	int c, err = 0; 
	int sflag=0;
	char *host = DEF_HOST;
	char *port = DEF_PORT;
	static char usage[] = "Usage: %s [-ds] [-h host] [-p port] \n";

	while ((c = getopt(argc, argv, "dh:p:s")) != -1)
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
		case '?':
			err = 1;
			break;
		}
	if (err) {
		fprintf(stderr, usage, argv[0]);
		exit(1);
	}

	request_init();

	snprintf(url, URL_SIZE, URL_FORMAT, sflag ? "https" : "http", host, port);
	text = request(url, "SELECT VERSION()");
	if(!text)
		return 1;

	json_t *json = parse(text);
	if (!json)
		return 1;

	json_t *version = json_object_get(json, "version");
	if(!json_is_string(version)) {
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
		cmd[strlen(cmd)-1] = '\0';
		localcommand(cmd);
	}

	request_cleanup();
	json_decref(json);
	return 0;
}
