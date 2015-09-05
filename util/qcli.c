#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <jansson.h>
#include <curl/curl.h>

#define ASZ(a) sizeof(a)/sizeof(a[0])

#define BUFFER_SIZE  (256 * 1024)  /* 256 KB */
#define URL_SIZE     256

char url[URL_SIZE];
static char line[1024];
char *_url = "localhost";
char *_port = "4017";

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

static char *request(const char *url, const char *sql) {
	CURL *curl = NULL;
	CURLcode status;
	struct curl_slist *headers = NULL;
	char *data = NULL;
	long code;
	char query[URL_SIZE];

	curl_global_init(CURL_GLOBAL_ALL);
	curl = curl_easy_init();
	if(!curl)
		goto error;

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

	curl_easy_setopt(curl, CURLOPT_URL, url);

	headers = curl_slist_append(headers, "User-Agent: Quantica CLI");
	headers = curl_slist_append(headers, "Accept: application/json");
	headers = curl_slist_append(headers, "Content-Type: application/json");

	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &write_result);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, query);

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

	curl_easy_cleanup(curl);
	curl_slist_free_all(headers);
	curl_global_cleanup();

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
	exit(0);
}

struct localfunction {
	char name[64];
	char description[128];
	void (*vf)(void);
} localfunclist[] = {
	{"quit", "Exit shell", &local_exit},
};

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
		if (!strcmp(localfunclist[i].name, cmd)) {
			vfunc = localfunclist[i].vf;
			vfunc();
			return 1;
		}
	}
	char *text = request(url, cmd);
	puts(text);
	return 0;
}

int main(int argc, const char *argv[]) {
	char *text;

	json_t *root;
	json_error_t error;

	if (argc > 1) {
	if (!strcmp(argv[1], "-h"))
			fprintf(stderr, "usage: %s hostname port\n", argv[0]);
		return 2;
	}

	snprintf(url, URL_SIZE, "http://%s:%s/sql", _url, _port);
	text = request(url, "SELECT VERSION()");
	if(!text)
		return 1;

	root = json_loads(text, 0, &error);
	free(text);

	if (!root) {
		fprintf(stderr, "error: on line %d: %s\n", error.line, error.text);
		return 1;
	}

	if(!json_is_object(root)) {
		fprintf(stderr, "error: cannot parse result\n");
		json_decref(root);
		return 1;
	}

	json_t *success = json_object_get(root, "success");
	if (!json_is_boolean(success)) {
		fprintf(stderr, "error: cannot parse result\n");
		return 1;
	}

	json_t *desc = json_object_get(root, "description");
	if(!json_is_string(desc)) {
		fprintf(stderr, "error: cannot parse result\n");
		return 1;
	}

	json_t *version = json_object_get(root, "version");
	if(!json_is_string(version)) {
		fprintf(stderr, "error: cannot parse result\n");
		return 1;
	}
	printf("Quantica: %s\n", json_string_value(version));

	while (1) {
		printf(">>> ");
		fflush(NULL);

		/* Read a command line */
		if (!fgets(line, 1024, stdin))
			return 0;

		char *cmd = skipwhite(line);
		cmd[strlen(cmd)-1] = '\0';
		localcommand(cmd);
	}

	json_decref(root);
	return 0;
}
