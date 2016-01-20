#include <stdver.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
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
#include <error.h>

#include "zmalloc.h"
#include "dict_marshall.h"
#include "core.h"
#include "sha1.h"
#include "md5.h"
#include "sha2.h"
#include "host.h"
#include "vector.h"
#include "time.h"
#include "hashtable.h"
#include "sql.h"
#include "webapi.h"

#define HEADER_SIZE			10240L
#define VECTOR_RHEAD_SIZE	1024
#define VECTOR_SHEAD_SIZE	512
#define HASHTABLE_DATA_SIZE	10
#define VERSION_STRING		"(WebAPI)"

#define RLOGLINE_SIZE		256
#define RESPONSE_SIZE		512

int serversock4 = 0;
int serversock6 = 0;
int max_sd;
fd_set readfds;
fd_set readsock;
static unsigned long long int client_requests = 0;
unsigned int run = 1;

typedef enum {
	HTTP_GET = 1,
	HTTP_POST,
	HTTP_PUT,
	HTTP_HEAD,
	HTTP_TRACE,
	HTTP_PATCH,
	HTTP_DELETE,
	HTTP_OPTIONS,
	HTTP_CONNECT
} http_method_t;

typedef enum {
	HTTP_OK = 200,
	HTTP_NOT_FOUND = 404
} http_status_t;

typedef struct {
	char *uri;
	hashtable_t *data;
	hashtable_t *querystring;
	hashtable_t *header;
	http_method_t method;
} http_request_t;

struct webroute {
	char uri[32];
	http_status_t (*api_handler)(char **response, http_request_t *req);
	unsigned char require_quid;
	char description[32];
};

http_status_t api_help(char **response, http_request_t *req);

void *get_in_addr(struct sockaddr *sa) {
	if (sa->sa_family == AF_INET)
		return &(((struct sockaddr_in*)sa)->sin_addr);
	else
		return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void handle_shutdown(int intsig) {
	lprintf("[info] Received SIG:%d\n", intsig);
	lprint("[info] Shutting down\n");

	shutdown(serversock4, SHUT_RDWR);
	shutdown(serversock6, SHUT_RDWR);
	close(serversock4);
	close(serversock6);

	lprint("[info] Cleanup and detach core\n");
	detach_core();

	exit(0);
}

void raw_response(FILE *socket_stream, vector_t *headers, const char *status) {
	char squid[QUID_LENGTH + 1] = {'\0'};
	quid_generate(squid);

	fprintf(socket_stream,
	        "HTTP/1.1 %s\r\n"
	        "Server: " PROGNAME "/%s " VERSION_STRING "\r\n"
	        "Content-Type: application/json\r\n"
	        "Content-Length: 0\r\n", status, get_version_string());

	size_t i;
	for (i = 0; i < headers->size; ++i) {
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
	char squid[QUID_LENGTH + 1] = {'\0'};
	quid_generate(squid);

	fprintf(socket_stream,
	        "HTTP/1.1 %s\r\n"
	        "Server: " PROGNAME "/%s " VERSION_STRING "\r\n"
	        "Content-Type: application/json\r\n"
	        "Content-Length: %zu\r\n", status, get_version_string(), (strlen(message) + 2));

	size_t i;
	for (i = 0; i < headers->size; ++i) {
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

char *get_http_status(http_status_t status) {
	static char buf[16];
	switch (status) {
		case HTTP_OK:
			strlcpy(buf, "200 OK", 16);
			break;
		case HTTP_NOT_FOUND:
			strlcpy(buf, "404 Not Found", 16);
			break;
		default:
			strlcpy(buf, "400 Bad Request", 16);
			break;
	}
	return buf;
}

/*
 * Locate parameter or return NULL
 */
char *get_param(http_request_t *req, char *param_name) {
	if (req->querystring) {
		char *param = (char *)hashtable_get(req->querystring, param_name);
		if (param)
			return param;
	}
	if ((req->method == HTTP_POST || req->method == HTTP_PUT) && req->data->n > 0) {
		return (char *)hashtable_get(req->data, param_name);
	}
	if (req->header->n > 0) {
		return (char *)hashtable_get(req->header, param_name);
	}
	return NULL;
}

/*
 * Default methods
 */
http_status_t response_internal_error(char **response) {
	snprintf(*response, RESPONSE_SIZE, "{\"error\":{\"code\":\"%s\",\"quid\":\"%s\",\"description\":\"%s\"},\"description\":\"An error occured\",\"status\":\"INTERNAL_ERROR\",\"success\":false}", get_error_code(), get_instance_prefix_key(get_error_code()), get_error_description());
	return HTTP_OK;
}

http_status_t response_empty_error(char **response) {
	strlcpy(*response, "{\"description\":\"Request expects data\",\"status\":\"EMPTY_DATA\",\"success\":false}", RESPONSE_SIZE);
	return HTTP_OK;
}

http_status_t api_not_found(char **response, http_request_t *req) {
	unused(req);
	strlcpy(*response, "{\"description\":\"API call does not exist, request 'help' for a list of API calls\",\"status\":\"API_NOT_FOUND\",\"success\":false}", RESPONSE_SIZE);
	return HTTP_NOT_FOUND;
}

/*
 * API methods
 */

http_status_t api_root(char **response, http_request_t *req) {
	unused(req);
	snprintf(*response, RESPONSE_SIZE, "{\"api_version\":%d,\"db_version\":\"%s\",\"instance\":{\"name\":\"%s\",\"quid\":\"%s\"},\"session\":{\"quid\":\"%s\"},\"license\":\"" LICENSE  "\",\"active\":\"%s\",\"description\":\"The server is ready to accept requests\",\"status\":\"SUCCEEDED\",\"success\":true}", API_VERSION, get_version_string(), get_instance_name(), get_instance_key(), get_session_key(), str_bool(get_ready_status()));
	return HTTP_OK;
}

http_status_t api_variables(char **response, http_request_t *req) {
	unused(req);
	char buf[26];
	char buf2[TIMENAME_SIZE + 1];
	buf2[TIMENAME_SIZE] = '\0';

	char *htime = tstostrf(buf, 26, get_timestamp(), ISO_8601_FORMAT);
	if (iserror()) {
		return response_internal_error(response);
	}

	/* Get hostname */
	char *hostname = get_system_fqdn();

	*response = zrealloc(*response, RESPONSE_SIZE * 2);
	snprintf(*response, RESPONSE_SIZE * 2, "{\"server\":{\"uptime\":\"%s\",\"client_requests\":%llu,\"port\":%d,\"host\":\"%s\"},\"pager\":{\"page_size\":%u,\"page_count\":%d,\"allocated\":\"%s\",\"in_use\":\"%s\"},\"engine\":{\"records\":%lu,\"free\":%lu,\"blocks_free\":%lu,\"groups\":%lu,\"indexes\":%lu,\"default_key\":\"%s\"},\"date\":{\"timestamp\":%lld,\"unixtime\":%lld,\"datetime\":\"%s\",\"timename\":\"%s\"},\"version\":{\"major\":%d,\"minor\":%d,\"patch\":%d},\"description\":\"Database statistics\",\"status\":\"SUCCEEDED\",\"success\":true}"
	         , get_uptime()
	         , client_requests
	         , API_PORT
	         , hostname
	         , get_pager_page_size()
	         , get_pager_page_count()
	         , get_pager_alloc_size()
	         , get_total_disk_size()
	         , stat_getkeys()
	         , stat_getfreekeys()
	         , stat_getfreeblocks()
	         , stat_tablesize()
	         , stat_indexsize()
	         , get_instance_prefix_key("000000000000")
	         , get_timestamp()
	         , get_unixtimestamp()
	         , htime
	         , timename_now(buf2)
	         , VERSION_MAJOR
	         , VERSION_MINOR
	         , VERSION_PATCH);

	zfree(hostname);
	return HTTP_OK;
}

http_status_t api_instance(char **response, http_request_t *req) {
	char *name = get_param(req, "name");
	if (name) {
		set_instance_name(name);
		if (iserror()) {
			return response_internal_error(response);
		}
		strlcpy(*response, "{\"description\":\"Instance name set\",\"status\":\"SUCCEEDED\",\"success\":true}", RESPONSE_SIZE);
		return HTTP_OK;
	}
	snprintf(*response, RESPONSE_SIZE, "{\"name\":\"%s\",\"quid\":\"%s\",\"description\":\"Server instance name\",\"status\":\"SUCCEEDED\",\"success\":true}", get_instance_name(), get_instance_key());
	return HTTP_OK;
}

http_status_t api_sha1(char **response, http_request_t *req) {
	char strsha[SHA1_LENGTH + 1];
	strsha[SHA1_LENGTH] = '\0';

	char *data = get_param(req, "data");
	if (data) {
		crypto_sha1(strsha, data);
		if (iserror()) {
			return response_internal_error(response);
		}
		snprintf(*response, RESPONSE_SIZE, "{\"hash\":\"%s\",\"description\":\"Data hashed with SHA1\",\"status\":\"SUCCEEDED\",\"success\":true}", strsha);
		return HTTP_OK;
	}
	return response_empty_error(response);
}

http_status_t api_md5(char **response, http_request_t *req) {
	char strmd5[MD5_SIZE + 1];
	strmd5[MD5_SIZE] = '\0';

	char *data = get_param(req, "data");
	if (data) {
		crypto_md5(strmd5, data);
		if (iserror()) {
			return response_internal_error(response);
		}
		snprintf(*response, RESPONSE_SIZE, "{\"hash\":\"%s\",\"description\":\"Data hashed with MD5\",\"status\":\"SUCCEEDED\",\"success\":true}", strmd5);
		return HTTP_OK;
	}
	return response_empty_error(response);
}

http_status_t api_sha256(char **response, http_request_t *req) {
	char strsha256[(2 * SHA256_DIGEST_SIZE) + 1];
	strsha256[2 * SHA256_DIGEST_SIZE] = '\0';

	char *data = get_param(req, "data");
	if (data) {
		crypto_sha256(strsha256, data);
		if (iserror()) {
			return response_internal_error(response);
		}
		snprintf(*response, RESPONSE_SIZE, "{\"hash\":\"%s\",\"description\":\"Data hashed with SHA256\",\"status\":\"SUCCEEDED\",\"success\":true}", strsha256);
		return HTTP_OK;
	}
	return response_empty_error(response);
}

http_status_t api_sha512(char **response, http_request_t *req) {
	char strsha512[(2 * SHA512_DIGEST_SIZE) + 1];
	strsha512[(2 * SHA512_DIGEST_SIZE)] = '\0';

	char *data = get_param(req, "data");
	if (data) {
		crypto_sha512(strsha512, data);
		if (iserror()) {
			return response_internal_error(response);
		}
		snprintf(*response, RESPONSE_SIZE, "{\"hmac\":\"%s\",\"description\":\"Data hashed with SHA512\",\"status\":\"SUCCEEDED\",\"success\":true}", strsha512);
		return HTTP_OK;
	}
	return response_empty_error(response);
}

http_status_t api_hmac_sha256(char **response, http_request_t *req) {
	char mac[SHA256_BLOCK_SIZE + 1];
	mac[SHA256_BLOCK_SIZE] = '\0';

	char *data = get_param(req, "data");
	char *key = get_param(req, "key");
	if (data && key) {
		crypto_hmac_sha256(mac, key, data);
		nullify(key, strlen(key));
		if (iserror()) {
			return response_internal_error(response);
		}
		snprintf(*response, RESPONSE_SIZE, "{\"hmac\":\"%s\",\"description\":\"Data signed with HMAC-SHA256\",\"status\":\"SUCCEEDED\",\"success\":true}", mac);
		return HTTP_OK;
	}
	return response_empty_error(response);
}

http_status_t api_hmac_sha512(char **response, http_request_t *req) {
	char mac[SHA512_BLOCK_SIZE + 1];
	mac[SHA512_BLOCK_SIZE] = '\0';

	char *data = get_param(req, "data");
	char *key = get_param(req, "key");
	if (data && key) {
		crypto_hmac_sha512(mac, key, data);
		nullify(key, strlen(key));
		if (iserror()) {
			return response_internal_error(response);
		}
		snprintf(*response, RESPONSE_SIZE, "{\"hash\":\"%s\",\"description\":\"Data signed with HMAC-SHA256\",\"status\":\"SUCCEEDED\",\"success\":true}", mac);
		return HTTP_OK;
	}
	return response_empty_error(response);
}

http_status_t api_sql_query(char **response, http_request_t *req) {
	char *query = get_param(req, "query");
	if (query) {
		size_t len = 0, resplen;
		sqlresult_t *data = exec_sqlquery(query, &len);
		if (iserror()) {
			return response_internal_error(response);
		}

		resplen = RESPONSE_SIZE;
		if (len > RESPONSE_SIZE) {
			resplen = RESPONSE_SIZE + len;
			*response = zrealloc(*response, resplen);
		}

		if (data->quid[0] != '\0')
			snprintf(*response, resplen, "{\"quid\":\"%s\",\"items\":%d,\"description\":\"Data stored in record\",\"status\":\"SUCCEEDED\",\"success\":true}", data->quid, data->items);
		else if (data->name) {
			snprintf(*response, resplen, "{\"%s\":\"%s\",\"description\":\"Query executed\",\"status\":\"SUCCEEDED\",\"success\":true}", data->name, (char *)data->data);
			zfree(data->name);
			zfree(data->data);
		} else if (data->data) {
			snprintf(*response, resplen, "{\"data\":%s,\"description\":\"Retrieve record by requested key\",\"status\":\"SUCCEEDED\",\"success\":true}", (char *)data->data);
			zfree(data->data);
		} else
			snprintf(*response, resplen, "{\"description\":\"Query executed\",\"status\":\"SUCCEEDED\",\"success\":true}");
		return HTTP_OK;
	}
	return response_empty_error(response);
}

http_status_t api_vacuum(char **response, http_request_t *req) {
	int size = 0;
	char *page_size = get_param(req, "page_size");
	if (page_size) {
		size = atoi(page_size);
	}
	zvacuum(size);
	if (iserror()) {
		return response_internal_error(response);
	}
	strlcpy(*response, "{\"description\":\"Vacuum succeeded\",\"status\":\"SUCCEEDED\",\"success\":true}", RESPONSE_SIZE);
	return HTTP_OK;
}

http_status_t api_sync(char **response, http_request_t *req) {
	unused(req);
	filesync();
	if (iserror()) {
		return response_internal_error(response);
	}
	strlcpy(*response, "{\"description\":\"Block synchronization succeeded\",\"status\":\"SUCCEEDED\",\"success\":true}", RESPONSE_SIZE);
	return HTTP_OK;
}

http_status_t api_gen_quid(char **response, http_request_t *req) {
	bool qshort = FALSE;
	char *squid = NULL;
	char *param_short = get_param(req, "short");
	if (param_short) {
		if (!strcmp(param_short, "true")) {
			qshort = TRUE;
		}
	}

	if (qshort) {
		squid = (char *)zmalloc(SHORT_QUID_LENGTH + 1);
		quid_generate_short(squid);
	} else {
		squid = (char *)zmalloc(QUID_LENGTH + 1);
		quid_generate(squid);
	}

	if (iserror()) {
		zfree(squid);
		return response_internal_error(response);
	}
	snprintf(*response, RESPONSE_SIZE, "{\"quid\":\"%s\",\"description\":\"New QUID generated\",\"status\":\"SUCCEEDED\",\"success\":true}", squid);
	zfree(squid);
	return HTTP_OK;
}

http_status_t api_gen_rand(char **response, http_request_t *req) {
	bool range = FALSE;
	char *param_range = get_param(req, "range");
	if (param_range) {
		range = TRUE;
	}

	int number = generate_random_number(range ? atoi(param_range) : 0);
	if (iserror()) {
		return response_internal_error(response);
	}
	snprintf(*response, RESPONSE_SIZE, "{\"number\":%d,\"description\":\"Random number\",\"status\":\"SUCCEEDED\",\"success\":true}", number);
	return HTTP_OK;
}

http_status_t api_shutdown(char **response, http_request_t *req) {
	unused(req);
	run = 0;
	strlcpy(*response, "{\"description\":\"Shutting down database\",\"status\":\"SUCCEEDED\",\"success\":true}", RESPONSE_SIZE);
	return HTTP_OK;
}

http_status_t api_base64_enc(char **response, http_request_t *req) {
	char *data = get_param(req, "data");
	if (data) {
		char *enc = crypto_base64_enc(data);
		if (iserror()) {
			zfree(enc);
			return response_internal_error(response);
		}
		snprintf(*response, RESPONSE_SIZE, "{\"encode\":\"%s\",\"description\":\"Data encoded with base64\",\"status\":\"SUCCEEDED\",\"success\":true}", enc);
		zfree(enc);
		return HTTP_OK;
	}
	return response_empty_error(response);
}

http_status_t api_base64_dec(char **response, http_request_t *req) {
	char *data = get_param(req, "data");
	if (data) {
		char *enc = crypto_base64_dec(data);
		if (iserror()) {
			zfree(enc);
			return response_internal_error(response);
		}
		snprintf(*response, RESPONSE_SIZE, "{\"encode\":\"%s\",\"description\":\"Data encoded with base64\",\"status\":\"SUCCEEDED\",\"success\":true}", enc);
		zfree(enc);
		return HTTP_OK;
	}
	return response_empty_error(response);
}

http_status_t api_db_put(char **response, http_request_t *req) {
	char squid[QUID_LENGTH + 1];
	int items = 0;

	char *hint = get_param(req, "hint");
	char *options = get_param(req, "options");
	char *data = get_param(req, "data");
	if (data) {
		db_put(squid, &items, data, strlen(data), hint, options);
		if (iserror()) {
			return response_internal_error(response);
		}
		snprintf(*response, RESPONSE_SIZE, "{\"quid\":\"%s\",\"items\":%d,\"description\":\"Data stored in record\",\"status\":\"SUCCEEDED\",\"success\":true}", squid, items);
		return HTTP_OK;
	}
	return response_empty_error(response);
}

http_status_t api_db_get(char **response, http_request_t *req) {
	size_t len = 0, resplen;
	bool _resolve = TRUE;
	bool getforce = FALSE;

	char *quid = (char *)hashtable_get(req->data, "quid");
	char *resolve = get_param(req, "resolve");
	char *selector = get_param(req, "select");
	char *force = get_param(req, "force");
	char *where = get_param(req, "where");
	if (quid) {
		char *data = NULL;
		if (selector || where) {
			data = db_select(quid, selector, where);
			if (iserror()) {
				return response_internal_error(response);
			}
		} else {
			if (resolve) {
				if (!strcmp(resolve, "false")) {
					_resolve = FALSE;
				}
			}
			if (force) {
				if (!strcmp(force, "true")) {
					getforce = TRUE;
				}
			}

			data = db_get(quid, &len, _resolve, getforce);
			if (iserror()) {
				return response_internal_error(response);
			}
		}

		len = strlen(data);
		resplen = RESPONSE_SIZE;
		if (len > (RESPONSE_SIZE / 2)) {
			resplen = RESPONSE_SIZE + len;
			*response = zrealloc(*response, resplen);
		}
		snprintf(*response, resplen, "{\"data\":%s,\"description\":\"Retrieve record by requested key\",\"status\":\"SUCCEEDED\",\"success\":true}", data);
		zfree(data);
		return HTTP_OK;
	}
	return response_empty_error(response);
}

http_status_t api_db_get_type(char **response, http_request_t *req) {
	char *quid = (char *)hashtable_get(req->data, "quid");
	if (quid) {
		char *type = db_get_type(quid);
		if (iserror()) {
			return response_internal_error(response);
		}
		snprintf(*response, RESPONSE_SIZE, "{\"datatype\":\"%s\",\"description\":\"Datatype of record value\",\"status\":\"SUCCEEDED\",\"success\":true}", type);
		return HTTP_OK;
	}
	return response_empty_error(response);
}

http_status_t api_db_get_schema(char **response, http_request_t *req) {
	char *quid = (char *)hashtable_get(req->data, "quid");
	if (quid) {
		char *schema = db_get_schema(quid);
		if (iserror()) {
			return response_internal_error(response);
		}
		snprintf(*response, RESPONSE_SIZE, "{\"dataschema\":\"%s\",\"description\":\"Datatype of record value\",\"status\":\"SUCCEEDED\",\"success\":true}", schema);
		return HTTP_OK;
	}
	return response_empty_error(response);
}

http_status_t api_db_get_history(char **response, http_request_t *req) {
	char *quid = (char *)hashtable_get(req->data, "quid");
	if (quid) {
		char *history = db_get_history(quid);
		if (iserror()) {
			return response_internal_error(response);
		}
		snprintf(*response, RESPONSE_SIZE, "{\"history_versions\":%s,\"description\":\"Record history\",\"status\":\"SUCCEEDED\",\"success\":true}", history);
		zfree(history);
		return HTTP_OK;
	}
	return response_empty_error(response);
}

http_status_t api_db_get_version(char **response, http_request_t *req) {
	char *quid = (char *)hashtable_get(req->data, "quid");
	if (quid) {
		char *data = db_get_version(quid, req->uri);
		if (iserror()) {
			return response_internal_error(response);
		}
		size_t len = strlen(data);
		size_t resplen = RESPONSE_SIZE;
		if (len > (RESPONSE_SIZE / 2)) {
			resplen = RESPONSE_SIZE + len;
			*response = zrealloc(*response, resplen);
		}
		snprintf(*response, resplen, "{\"data\":%s,\"description\":\"Retrieve historic record by version key\",\"status\":\"SUCCEEDED\",\"success\":true}", data);
		zfree(data);
		return HTTP_OK;
	}
	return response_empty_error(response);
}

http_status_t api_db_delete(char **response, http_request_t *req) {
	bool cascade = TRUE;

	char *quid = (char *)hashtable_get(req->data, "quid");
	char *nocascade = get_param(req, "nocascade");
	if (quid) {
		if (nocascade) {
			if (!strcmp(nocascade, "true")) {
				cascade = FALSE;
			}
		}
		db_delete(quid, cascade);
		if (iserror()) {
			return response_internal_error(response);
		}
		snprintf(*response, RESPONSE_SIZE, "{\"description\":\"Record deleted\",\"status\":\"SUCCEEDED\",\"success\":true}");
		return HTTP_OK;
	}
	return response_empty_error(response);
}

http_status_t api_db_purge(char **response, http_request_t *req) {
	bool cascade = TRUE;

	char *quid = (char *)hashtable_get(req->data, "quid");
	char *nocascade = get_param(req, "nocascade");
	if (quid) {
		if (nocascade) {
			if (!strcmp(nocascade, "true")) {
				cascade = FALSE;
			}
		}
		db_purge(quid, cascade);
		if (iserror()) {
			return response_internal_error(response);
		}
		snprintf(*response, RESPONSE_SIZE, "{\"description\":\"Record purged\",\"status\":\"SUCCEEDED\",\"success\":true}");
		return HTTP_OK;
	}
	return response_empty_error(response);
}

http_status_t api_db_count(char **response, http_request_t *req) {
	char *quid = (char *)hashtable_get(req->data, "quid");
	if (quid) {
		int elements = db_count_group(quid);
		if (iserror()) {
			return response_internal_error(response);
		}
		snprintf(*response, RESPONSE_SIZE, "{\"items\":%d,\"description\":\"Show number of items in group\",\"status\":\"SUCCEEDED\",\"success\":true}", elements);
		return HTTP_OK;
	}
	return response_empty_error(response);
}

http_status_t api_db_update(char **response, http_request_t *req) {
	bool cascade = TRUE;
	int items = 0;

	char *quid = (char *)hashtable_get(req->data, "quid");
	char *data = get_param(req, "data");
	char *nocascade = get_param(req, "nocascade");
	if (quid && data) {
		if (nocascade) {
			if (!strcmp(nocascade, "true")) {
				cascade = FALSE;
			}
		}
		db_update(quid, &items, cascade, data, strlen(data));
		if (iserror()) {
			return response_internal_error(response);
		}
		snprintf(*response, RESPONSE_SIZE, "{\"items\":%d,\"description\":\"Record updated\",\"status\":\"SUCCEEDED\",\"success\":true}", items);
		return HTTP_OK;
	}
	return response_empty_error(response);
}

http_status_t api_db_duplicate(char **response, http_request_t *req) {
	char squid[QUID_LENGTH + 1];
	bool meta = TRUE;
	int items = 0;

	char *quid = (char *)hashtable_get(req->data, "quid");
	char *nometa = get_param(req, "nometa");
	if (quid) {
		if (nometa) {
			if (!strcmp(nometa, "true")) {
				meta = FALSE;
			}
		}
		db_duplicate(quid, squid, &items, meta);
		if (iserror()) {
			return response_internal_error(response);
		}
		snprintf(*response, RESPONSE_SIZE, "{\"quid\":\"%s\",\"items\":%d,\"description\":\"Record duplicated\",\"status\":\"SUCCEEDED\",\"success\":true}", squid, items);
		return HTTP_OK;
	}
	return response_empty_error(response);
}

http_status_t api_db_item_add(char **response, http_request_t *req) {
	int items = 0;

	char *quid = (char *)hashtable_get(req->data, "quid");
	char *data = get_param(req, "data");
	if (quid && data) {
		db_item_add(quid, &items, data, strlen(data));
		if (iserror()) {
			return response_internal_error(response);
		}
		snprintf(*response, RESPONSE_SIZE, "{\"items\":%d,\"description\":\"Items added to group\",\"status\":\"SUCCEEDED\",\"success\":true}", items);
		return HTTP_OK;
	}
	return response_empty_error(response);
}

http_status_t api_db_item_remove(char **response, http_request_t *req) {
	int items = 0;

	char *quid = (char *)hashtable_get(req->data, "quid");
	char *data = get_param(req, "data");
	if (quid && data) {
		db_item_remove(quid, &items, data, strlen(data));
		if (iserror()) {
			return response_internal_error(response);
		}
		snprintf(*response, RESPONSE_SIZE, "{\"items\":%d,\"description\":\"Items removed from group\",\"status\":\"SUCCEEDED\",\"success\":true}", items);
		return HTTP_OK;
	}
	return response_empty_error(response);
}

http_status_t api_index_group(char **response, http_request_t *req) {
	char squid[QUID_LENGTH + 1];
	int items = 0;

	char *quid = (char *)hashtable_get(req->data, "quid");
	char *element = get_param(req, "element");
	if (quid) {
		if (element) {
			db_index_create(quid, squid, &items, element);
			if (iserror()) {
				return response_internal_error(response);
			}
			snprintf(*response, RESPONSE_SIZE, "{\"quid\":\"%s\",\"items\":%d,\"description\":\"Index created\",\"status\":\"SUCCEEDED\",\"success\":true}", squid, items);
			return HTTP_OK;
		}
		char *indexes = db_index_on_group(quid);
		if (iserror()) {
			zfree(indexes);
			return response_internal_error(response);
		}
		snprintf(*response, RESPONSE_SIZE, "{\"name\":%s,\"description\":\"Listening indexes on group\",\"status\":\"SUCCEEDED\",\"success\":true}", indexes);
		zfree(indexes);
		return HTTP_OK;
	}
	return response_empty_error(response);
}

http_status_t api_index_rebuild(char **response, http_request_t *req) {
	int items = 0;

	char *quid = (char *)hashtable_get(req->data, "quid");
	if (quid) {
		db_index_rebuild(quid, &items);
		if (iserror()) {
			return response_internal_error(response);
		}
		snprintf(*response, RESPONSE_SIZE, "{\"items\":%d,\"description\":\"Index rebuild\",\"status\":\"SUCCEEDED\",\"success\":true}", items);
		return HTTP_OK;
	}
	return response_empty_error(response);
}

http_status_t api_db_get_meta(char **response, http_request_t *req) {
	char *quid = (char *)hashtable_get(req->data, "quid");
	char *executable = get_param(req, "executable");
	char *freeze = get_param(req, "freeze");
	char *importance = get_param(req, "importance");
	char *lifecycle = get_param(req, "lifecycle");
	char *system_lock = get_param(req, "system_lock");
	char *force = get_param(req, "force");
	if (quid) {
		struct record_status status;
		bool getforce = FALSE;
		if (force) {
			if (!strcmp(force, "true")) {
				getforce = TRUE;
			}
		}
		if (executable || freeze || importance || lifecycle || system_lock) {
			db_record_get_meta(quid, FALSE, &status);
			if (iserror()) {
				return response_internal_error(response);
			}

			/* Set record values if given */
			if (executable)
				status.exec = atoi(executable);
			if (freeze)
				status.freeze = atoi(freeze);
			if (importance)
				status.importance = atoi(importance);
			if (lifecycle)
				strlcpy(status.lifecycle, lifecycle, STATUS_LIFECYCLE_SIZE);
			if (system_lock)
				status.syslock = atoi(system_lock);

			db_record_set_meta(quid, &status);
			if (iserror()) {
				return response_internal_error(response);
			}
			snprintf(*response, RESPONSE_SIZE, "{\"description\":\"Record updated\",\"status\":\"SUCCEEDED\",\"success\":true}");
			return HTTP_OK;
		}
		db_record_get_meta(quid, getforce, &status);
		if (iserror()) {
			return response_internal_error(response);
		}
		if (status.has_alias)
			snprintf(*response, RESPONSE_SIZE, "{\"metadata\":{\"nodata\":%s,\"freeze\":%s,\"executable\":%s,\"system_lock\":%s,\"lifecycle\":\"%s\",\"importance\":%d,\"type\":\"%s\",\"alias\":\"%s\"},\"description\":\"Record metadata queried\",\"status\":\"SUCCEEDED\",\"success\":true}", str_bool(status.nodata), str_bool(status.freeze), str_bool(status.exec), str_bool(status.syslock), status.lifecycle, status.importance, status.type, status.alias);
		else
			snprintf(*response, RESPONSE_SIZE, "{\"metadata\":{\"nodata\":%s,\"freeze\":%s,\"executable\":%s,\"system_lock\":%s,\"lifecycle\":\"%s\",\"importance\":%d,\"type\":\"%s\",\"alias\":null},\"description\":\"Record metadata queried\",\"status\":\"SUCCEEDED\",\"success\":true}", str_bool(status.nodata), str_bool(status.freeze), str_bool(status.exec), str_bool(status.syslock), status.lifecycle, status.importance, status.type);
		return HTTP_OK;
	}
	return response_empty_error(response);
}

http_status_t api_db_decode(char **response, http_request_t *req) {
	char *quid = (char *)hashtable_get(req->data, "quid");
	if (quid) {
		char *data = key_decode(quid);
		if (iserror()) {
			return response_internal_error(response);
		}
		snprintf(*response, RESPONSE_SIZE, "{\"data\":%s,\"description\":\"Record decoded\",\"status\":\"SUCCEEDED\",\"success\":true}", data);
		zfree(data);
		return HTTP_OK;
	}
	return response_empty_error(response);
}

http_status_t api_alias_name(char **response, http_request_t *req) {
	char *quid = (char *)hashtable_get(req->data, "quid");
	char *name = get_param(req, "name");
	if (quid) {
		if (name) {
			db_alias_update(quid, name);
			if (iserror()) {
				return response_internal_error(response);
			}
			snprintf(*response, RESPONSE_SIZE, "{\"description\":\"Alias set\",\"status\":\"SUCCEEDED\",\"success\":true}");
			return HTTP_OK;
		}
		char *current_name = db_alias_get_name(quid);
		if (iserror()) {
			zfree(current_name);
			return response_internal_error(response);
		}
		snprintf(*response, RESPONSE_SIZE, "{\"name\":\"%s\",\"description\":\"Get in list\",\"status\":\"SUCCEEDED\",\"success\":true}", current_name);
		zfree(current_name);
		return HTTP_OK;
	}
	return response_empty_error(response);
}

http_status_t api_alias_get(char **response, http_request_t *req) {
	size_t len = 0, resplen;
	bool _resolve = TRUE;
	char *resolve = get_param(req, "resolve");
	if (resolve) {
		if (!strcmp(resolve, "false")) {
			_resolve = FALSE;
		}
	}

	char *data = db_alias_get_data(req->uri, &len, _resolve);
	if (iserror()) {
		return response_internal_error(response);
	}

	resplen = RESPONSE_SIZE;
	if (len > (RESPONSE_SIZE / 2)) {
		resplen = RESPONSE_SIZE + len;
		*response = zrealloc(*response, resplen);
	}
	snprintf(*response, resplen, "{\"data\":%s,\"description\":\"Retrieve record by requested key\",\"status\":\"SUCCEEDED\",\"success\":true}", data);
	zfree(data);
	return HTTP_OK;
}

http_status_t api_alias_all(char **response, http_request_t *req) {
	unused(req);
	size_t len = 0, resplen;

	char *list = db_alias_all();
	if (iserror()) {
		return response_internal_error(response);
	}

	if (!list)
		list = zstrdup("null");

	len = strlen(list);
	resplen = RESPONSE_SIZE;
	if (len > (RESPONSE_SIZE / 2)) {
		resplen = RESPONSE_SIZE + len;
		*response = zrealloc(*response, resplen);
	}
	snprintf(*response, resplen, "{\"aliasses\":%s,\"description\":\"Listening aliasses\",\"status\":\"SUCCEEDED\",\"success\":true}", list);
	zfree(list);
	return HTTP_OK;
}

http_status_t api_index_all(char **response, http_request_t *req) {
	unused(req);
	size_t len = 0, resplen;

	char *list = db_index_all();
	if (iserror()) {
		return response_internal_error(response);
	}

	if (!list)
		list = zstrdup("null");

	len = strlen(list);
	resplen = RESPONSE_SIZE;
	if (len > (RESPONSE_SIZE / 2)) {
		resplen = RESPONSE_SIZE + len;
		*response = zrealloc(*response, resplen);
	}
	snprintf(*response, resplen, "{\"indexes\":%s,\"description\":\"Listening indexes\",\"status\":\"SUCCEEDED\",\"success\":true}", list);
	zfree(list);
	return HTTP_OK;
}

http_status_t api_page_all(char **response, http_request_t *req) {
	unused(req);
	size_t len = 0, resplen;

	char *list = db_pager_all();
	if (iserror()) {
		return response_internal_error(response);
	}

	if (!list)
		list = zstrdup("null");

	len = strlen(list);
	resplen = RESPONSE_SIZE;
	if (len > (RESPONSE_SIZE / 2)) {
		resplen = RESPONSE_SIZE + len;
		*response = zrealloc(*response, resplen);
	}
	snprintf(*response, resplen, "{\"pages\":%s,\"description\":\"Listening pages\",\"status\":\"SUCCEEDED\",\"success\":true}", list);
	zfree(list);
	return HTTP_OK;
}

http_status_t api_auth_token(char **response, http_request_t *req) {
	size_t len = 0, resplen;

	char *key = get_param(req, "key");
	if (key) {
		char *token = auth_token(key);
		if (iserror()) {
			return response_internal_error(response);
		}

		len = strlen(token);
		resplen = RESPONSE_SIZE;
		if (len > (RESPONSE_SIZE / 2)) {
			resplen = RESPONSE_SIZE + len;
			*response = zrealloc(*response, resplen);
		}
		snprintf(*response, resplen, "{\"token\":\"%s\",\"description\":\"Request token\",\"status\":\"SUCCEEDED\",\"success\":true}", token);
		zfree(token);
		return HTTP_OK;
	}
	return response_empty_error(response);
}

/*
 * Webrequest router
 */
static const struct webroute route[] = {
	/* URL				callback			QUID	Description */
	{"/",				api_root,			FALSE,	"WebAPI endpoint"},

	/* Daemon related operations				*/
	{"/sync",			api_sync,			FALSE,	"Flush datastorage to disk"},
	{"/vacuum",			api_vacuum,			FALSE,	"Vacuum the datastorage"},
	{"/instance",		api_instance,		FALSE,	"Get/set daemon instance name"},
	{"/shutdown",		api_shutdown,		FALSE,	"Shutting down the daemon"},
	{"/vars",			api_variables,		FALSE, 	"List current config and settings"},
	{"/help",			api_help,			FALSE,	"Show all API calls"},
	{"/pager",			api_page_all,		FALSE,	"Show all storage pages"},

	/* Encryption and encoding operations		*/
	{"/sha1",			api_sha1,			FALSE, 	"SHA1 hash function"},
	{"/md5",			api_md5,			FALSE,	"MD5 hash function"},
	{"/sha256",			api_sha256,			FALSE, 	"SHA256 hash function"},
	{"/sha512",			api_sha512,			FALSE, 	"SHA512 hash function"},
	{"/quid",			api_gen_quid,		FALSE,	"Generate random QUID"},
	{"/random",			api_gen_rand,		FALSE,	"Generate random number"},
	{"/base64/enc",		api_base64_enc,		FALSE,	"Base64 encode function"},
	{"/base64/dec",		api_base64_dec,		FALSE, 	"Base64 decode function"},
	{"/hmac/sha256",	api_hmac_sha256,	FALSE,	"Generate HMAC-SHA256 signature"},
	{"/hmac/sha512",	api_hmac_sha512,	FALSE,	"Generate HMAC-SHA512 signature"},

	/* SQL interface							*/
	{"/sql",			api_sql_query,		FALSE,	"SQL interface endpoint"},

	/* Database operations						*/
	{"/put",			api_db_put,			FALSE, 	"Insert new dataset"},
	{"/store",			api_db_put,			FALSE,	"Insert new dataset"},
	{"/insert",			api_db_put,			FALSE,	"Insert new dataset"},
	{"/get",			api_db_get,			TRUE,	"Retrieve dataset by key"},
	{"/retrieve",		api_db_get,			TRUE,	"Retrieve dataset by key"},
	{"/count",			api_db_count,		TRUE,	"Count items in group"},
	{"/update",			api_db_update,		TRUE,	"Update dataset by key"},
	{"/duplicate",		api_db_duplicate,	TRUE,	"Duplicate record into new record"},
	{"/copy",			api_db_duplicate,	TRUE,	"Duplicate record into new record"},
	{"/delete",			api_db_delete,		TRUE,	"Delete dataset by key"},
	{"/remove",			api_db_delete,		TRUE,	"Delete dataset by key"},
	{"/purge",			api_db_purge,		TRUE,	"Purge dataset and key"},
	{"/meta",			api_db_get_meta,	TRUE,	"Get/set metadata on key"},
	{"/decode",			api_db_decode,		TRUE,	"Decode QUID"},
	{"/type",			api_db_get_type,	TRUE,	"Show datatype"},
	{"/schema",			api_db_get_schema,	TRUE,	"Show data schema"},

	/* Record version control */
	{"/history",		api_db_get_history,	TRUE,	"Show record history"},
	{"/history/*",		api_db_get_version,	TRUE,	"Show record history"},

	/* Items operations							*/
	{"/attach",			api_db_item_add,	TRUE,	"Bind item to group"},
	{"/detach",			api_db_item_remove,	TRUE,	"Remove item from group"},

	/* Alias operations							*/
	{"/alias/*",		api_alias_get,		FALSE,	"Get dataset by alias name"},
	{"/alias",			api_alias_all,		FALSE,	"Show all aliasses and origins"},
	{"/alias",			api_alias_name,		TRUE,	"Show or set alias name by key"},

	/* Database index operations				*/
	{"/index",			api_index_group,	TRUE,	"Show or set index on element"},
	{"/rebuild",		api_index_rebuild,	TRUE,	"Rebuild the index"},
	{"/index",			api_index_all,		FALSE,	"Show all indexes and groups"},

	/* Authentication and authorization */
	{"/auth/token",		api_auth_token,		FALSE,	"Aquire authentication token"},

};

http_status_t api_help(char **response, http_request_t *req) {
	unused(req);
	size_t nsz = RSIZE(route);

	marshall_t *marshall = (marshall_t *)tree_zcalloc(1, sizeof(marshall_t), NULL);
	marshall->child = (marshall_t **)tree_zcalloc(nsz, sizeof(marshall_t *), marshall);
	marshall->type = MTYPE_OBJECT;

	for (unsigned int i = 0; i < nsz; ++i) {
		marshall->child[marshall->size] = tree_zcalloc(1, sizeof(marshall_t), marshall);
		marshall->child[marshall->size]->type = MTYPE_STRING;
		marshall->child[marshall->size]->name = tree_zstrdup(route[i].uri, marshall);
		marshall->child[marshall->size]->name_len = strlen(route[i].uri);
		marshall->child[marshall->size]->data = tree_zstrdup(route[i].description, marshall);
		marshall->child[marshall->size]->data_len = strlen(route[i].description);
		marshall->size++;
	}

	char *data = marshall_serialize(marshall);
	size_t len = strlen(data);
	size_t resplen = RESPONSE_SIZE;
	if (len > (RESPONSE_SIZE / 2)) {
		resplen = RESPONSE_SIZE + len;
		*response = zrealloc(*response, resplen);
	}
	snprintf(*response, resplen, "{\"api\":%s,\"description\":\"Available API calls\",\"status\":\"SUCCEEDED\",\"success\":true}", data);
	zfree(data);
	marshall_free(marshall);

	return HTTP_OK;
}

void handle_request(int sd, fd_set * set) {
	FILE *socket_stream = fdopen(sd, "r+");
	if (!socket_stream) {
		lprint("[erro] Failed to get file descriptor\n");
		goto disconnect;
	};

	/* No errors from this point on */
	error_clear();

	struct sockaddr_storage addr;
	char str_addr[INET6_ADDRSTRLEN];
	socklen_t len = sizeof(addr);
	if (!getpeername(sd, (struct sockaddr*)&addr, &len)) {
		inet_ntop(addr.ss_family, get_in_addr((struct sockaddr *)&addr), str_addr, sizeof(str_addr));
	} else {
		str_addr[0] = '?';
		str_addr[1] = '\0';
	}

	vector_t *queue = alloc_vector(VECTOR_RHEAD_SIZE);
	vector_t *headers = alloc_vector(VECTOR_SHEAD_SIZE);
	hashtable_t *postdata = alloc_hashtable(HASHTABLE_DATA_SIZE);
	hashtable_t *getdata = NULL;
	hashtable_t *headdata = alloc_hashtable(HASHTABLE_DATA_SIZE);
	char buf[HEADER_SIZE];
	while (!feof(socket_stream)) {
		char *in = fgets(buf, HEADER_SIZE - 2, socket_stream);
		if (!in)
			break;

		if (!strcmp(in, "\r\n") || !strcmp(in, "\n"))
			break;

		if (!strstr(in, "\n")) {
			raw_response(socket_stream, headers, "400 Bad Request");
			fflush(socket_stream);
			vector_free(queue);
			vector_free(headers);
			goto disconnect;
		}

		size_t request_sz = (strlen(buf) + 1) * sizeof(char);
		char *request_line = tree_zmalloc(request_sz, queue);
		strlcpy(request_line, buf, request_sz);
		vector_append(queue, (void *)request_line);
	}

	if (feof(socket_stream)) {
		vector_free(queue);
		vector_free(headers);
		goto disconnect;
	}

	unsigned int keepalive = 0;
	char *filename = NULL;
	char *_filename = NULL;
	char *querystring = NULL;
	int request_type = 0;
	char *host = NULL;
	char *http_version = NULL;
	unsigned long c_length = 0L;
	char *c_uagent = NULL;
	char *c_referer = NULL;
	char *c_connection = NULL;
	char *c_buf = NULL;
	client_requests++;

	unsigned int i;
	for (i = 0; i < queue->size; ++i) {
		char *str = (char *)vector_at(queue, i);

		char *colon = strchr(str, ':');
		if (!colon) {
			if (i > 0) {
				raw_response(socket_stream, headers, "400 Bad Request");
				fflush(socket_stream);
				vector_free(queue);
				vector_free(headers);
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
					} else if (strstr(str, "PUT ") == str) {
						r_type_width = 4;
						request_type = HTTP_PUT;
					} else if (strstr(str, "PATCH ") == str) {
						r_type_width = 6;
						request_type = HTTP_PATCH;
						goto unsupported;
					} else {
						goto unsupported;
					}
					break;
				case 'T':
					if (strstr(str, "TRACE ") == str) {
						r_type_width = 6;
						request_type = HTTP_TRACE;
						goto unsupported;
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
				case 'D':
					if (strstr(str, "DELETE ") == str) {
						r_type_width = 7;
						request_type = HTTP_DELETE;
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
				case 'C':
					if (strstr(str, "CONNECT ") == str) {
						r_type_width = 8;
						request_type = HTTP_CONNECT;
						raw_response(socket_stream, headers, "400 Bad Request");
						fflush(socket_stream);
						vector_free(queue);
						vector_free(headers);
						goto disconnect;
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
				vector_free(queue);
				vector_free(headers);
				goto disconnect;
			}

			http_version = strstr(filename, "HTTP/");
			if (!http_version) {
				raw_response(socket_stream, headers, "400 Bad Request");
				fflush(socket_stream);
				vector_free(queue);
				vector_free(headers);
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
				goto done;
			}

			colon[0] = '\0';
			if (colon[1] == ' ')
				colon += 2;
			else
				colon++;
			char *eol = strchr(colon, '\r');
			if (eol) {
				eol[0] = '\0';
				eol[1] = '\0';
			} else {
				eol = strchr(colon, '\n');
				if (eol)
					eol[0] = '\0';
			}
			strtolower(str);
			strtolower(colon);

			if (!strcmp(str, "host")) {
				host = colon;
				unused(host);
			} else if (!strcmp(str, "content-length")) {
				c_length = atol(colon);
			} else if (!strcmp(str, "user-agent")) {
				c_uagent = colon;
			} else if (!strcmp(str, "referer")) {
				c_referer = colon;
			} else if (!strcmp(str, "connection")) {
				c_connection = colon;
			} else {
				if (strcmp(str, "accept"))
					hashtable_put(&headdata, str, colon);
			}
		}
	}

	char logreqline[RLOGLINE_SIZE];
	memset(logreqline, 0, RLOGLINE_SIZE);
	snprintf(logreqline, RLOGLINE_SIZE, "[info] %s - ", str_addr);
	switch (request_type) {
		case HTTP_GET:
			strlcat(logreqline, "GET", RLOGLINE_SIZE);
			break;
		case HTTP_POST:
			strlcat(logreqline, "POST", RLOGLINE_SIZE);
			break;
		case HTTP_PUT:
			strlcat(logreqline, "PUT", RLOGLINE_SIZE);
			break;
		case HTTP_HEAD:
			strlcat(logreqline, "HEAD", RLOGLINE_SIZE);
			break;
		case HTTP_TRACE:
			strlcat(logreqline, "TRACE", RLOGLINE_SIZE);
			break;
		case HTTP_PATCH:
			strlcat(logreqline, "PATCH", RLOGLINE_SIZE);
			break;
		case HTTP_DELETE:
			strlcat(logreqline, "DELETE", RLOGLINE_SIZE);
			break;
		case HTTP_OPTIONS:
			strlcat(logreqline, "OPTIONS", RLOGLINE_SIZE);
			break;
		case HTTP_CONNECT:
			strlcat(logreqline, "CONNECT", RLOGLINE_SIZE);
			break;
		default:
			strlcat(logreqline, "UNKNOWN", RLOGLINE_SIZE);
			break;
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
	lprint(logreqline);

	if (c_connection) {
		if (!strcmp(c_connection, "close")) {
			vector_append_str(headers, "Connection: close\r\n");
		} else if (!strcmp(c_connection, "keep-alive")) {
			vector_append_str(headers, "Connection: keep-alive\r\n");
			keepalive = 1;
		}
	}

	if (!request_type) {
unsupported:
		raw_response(socket_stream, headers, "405 Method Not Allowed");
		fflush(socket_stream);
		goto done;
	}

	if (!filename || strstr(filename, "'") || strstr(filename, " ") || (querystring && strstr(querystring, " "))) {
		raw_response(socket_stream, headers, "400 Bad Request");
		fflush(socket_stream);
		goto done;
	}

	size_t _filename_sz = strlen(filename) + 2;
	_filename = calloc(_filename_sz, 1);
	strlcpy(_filename, filename, _filename_sz);
	if (strstr(_filename, "%")) {
		char *_buf = zmalloc(strlen(_filename) + 1);
		char *pstr = _filename;
		char *pbuf = _buf;
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
		zfree(_filename);
		_filename = _buf;
	}
	size_t fsz = strlen(_filename);
	if (fsz > 1 && _filename[fsz - 1] == '/')
		_filename[fsz - 1] = '\0';

	if (request_type == HTTP_OPTIONS) {
		vector_append_str(headers, "Allow: POST,OPTIONS,GET,HEAD\r\n");
		raw_response(socket_stream, headers, "200 OK");
		goto done;
	}

	if (c_length > 0) {
		size_t total_read = 0;
		c_buf = (char *)zmalloc(c_length + 1);
		while ((total_read < c_length) && (!feof(socket_stream))) {
			size_t diff = c_length - total_read;
			if (diff > 1024)
				diff = 1024;
			size_t chunk_read = fread(total_read + c_buf, 1, diff, socket_stream);
			total_read += chunk_read;
		}
		c_buf[total_read] = '\0';

		char *var = strtok(c_buf, "&");
		while (var != NULL) {
			char *value = strchr(var, '=');
			if (value) {
				value[0] = '\0';
				value++;
				hashtable_put(&postdata, var, value);
			}
			var = strtok(NULL, "&");
		}
	}

	if (querystring) {
		getdata = alloc_hashtable(HASHTABLE_DATA_SIZE);
		char *var = strtok(querystring, "&");
		while (var != NULL) {
			char *value = strchr(var, '=');
			if (value) {
				value[0] = '\0';
				value++;
				hashtable_put(&getdata, var, value);
			}
			var = strtok(NULL, "&");
		}
	}

	char *pch = NULL;
	size_t nsz = RSIZE(route);
	char *resp_message = (char *)zmalloc(RESPONSE_SIZE);
	http_status_t status = 0;
	http_request_t req;
	req.data = postdata;
	req.querystring = getdata;
	req.header = headdata;
	req.method = request_type;
	while (nsz-- > 0) {
		if (route[nsz].require_quid) {
			char squid[QUID_LENGTH + 1];
			if (fsz < QUID_LENGTH + 1)
				continue;

			/* Get QUID from filename */
			char *_pfilename = _filename + QUID_LENGTH + 1;
			strlcpy(squid, _filename + 1, QUID_LENGTH + 1);
			if (_pfilename[0] != '/') {
				_pfilename = _filename + QUID_LENGTH - 1;
				squid[QUID_LENGTH - 2] = '\0';
			}
			if (_pfilename[0] != '/')
				continue;

			if (_pfilename) {
				if ((pch = strchr(route[nsz].uri, '*')) != NULL) {
					size_t posc = pch - route[nsz].uri;
					if (!strncmp(route[nsz].uri, _pfilename, posc)) {
						hashtable_put(&req.data, "quid", squid);
						req.uri = _pfilename;
						req.uri += posc;
						status = route[nsz].api_handler(&resp_message, &req);
						goto respond;
					} else {
						continue;
					}
				} else if (!strcmp(route[nsz].uri, _pfilename)) {
					hashtable_put(&req.data, "quid", squid);
					req.uri = _pfilename;
					status = route[nsz].api_handler(&resp_message, &req);
					goto respond;
				}
			}
		} else if ((pch = strchr(route[nsz].uri, '*')) != NULL) {
			size_t posc = pch - route[nsz].uri;
			if (!strncmp(route[nsz].uri, _filename, posc)) {
				req.uri = _filename;
				req.uri += posc;
				status = route[nsz].api_handler(&resp_message, &req);
				goto respond;
			} else {
				continue;
			}
		} else if (!strcmp(route[nsz].uri, _filename)) {
			req.uri = _filename;
			status = route[nsz].api_handler(&resp_message, &req);
			goto respond;
		}
	}
	if (!status)
		status = api_not_found(&resp_message, &req);

respond:
	if (request_type == HTTP_HEAD) {
		raw_response(socket_stream, headers, get_http_status(status));
	} else {
		json_response(socket_stream, headers, get_http_status(status), resp_message);
	}
	zfree(resp_message);

done:
	fflush(socket_stream);
	if (c_buf)
		zfree(c_buf);
	zfree(_filename);
	vector_free(queue);
	vector_free(headers);
	if (getdata) {
		free_hashtable(getdata);
	}
	free_hashtable(postdata);
	free_hashtable(headdata);

	if (keepalive) {
		error_clear();
#if __KEEPALIVE__
		return;
#endif
	}

disconnect:
	/* Erase any errors */
	error_clear();

	if (socket_stream) {
		fclose(socket_stream);
		socket_stream = NULL;
	}
	shutdown(sd, SHUT_RDWR);
	FD_CLR(sd, set);
}

int start_webapi() {
	start_core();

	lprint("[info] Starting daemon\n");
	lprintf("[info] " PROGNAME " %s ("__DATE__", "__TIME__")\n", get_version_string());
	lprintf("[info] Current time: %lld\n", get_timestamp());
	lprintf("[info] Instance %s %s\n", get_instance_name(), get_instance_key());

	struct addrinfo hints, *servinfo, *p;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if (getaddrinfo(NULL, itoa(API_PORT), &hints, &servinfo) != 0) {
		lprint("[erro] Failed to get address info\n");
		freeaddrinfo(servinfo);
		detach_core();
		return 1;
	}

	for (p = servinfo; p != NULL; p = p->ai_next) {
		if (p->ai_family == AF_INET) {
			if (serversock4 == 0) {
				serversock4 = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
				if (serversock4 < 0) {
					lprint("[erro] Failed to create socket4\n");
					freeaddrinfo(servinfo);
					detach_core();
					return 1;
				}

				int _true = 1;
				if (setsockopt(serversock4, SOL_SOCKET, SO_REUSEADDR, &_true, sizeof(int)) < 0) {
					lprint("[erro] Failed to set socket option\n");
					freeaddrinfo(servinfo);
					detach_core();
					close(serversock4);
					return 1;
				}

				int opts = fcntl(serversock4, F_GETFL);
				if (opts < 0) {
					lprint("[erro] Failed to set nonblock on sock4\n");
					freeaddrinfo(servinfo);
					detach_core();
					close(serversock4);
					return 1;
				}

				opts = (opts | O_NONBLOCK);
				if (fcntl(serversock4, F_SETFL, opts) < 0) {
					lprint("[erro] Failed to set nonblock on sock4\n");
					freeaddrinfo(servinfo);
					detach_core();
					close(serversock4);
					return 1;
				}

				if (bind(serversock4, p->ai_addr, p->ai_addrlen) < 0) {
					lprintf("[erro] Failed to bind socket to port %d\n", API_PORT);
					freeaddrinfo(servinfo);
					detach_core();
					close(serversock4);
					return 1;
				}
			}
		} else if (p->ai_family == AF_INET6) {
			if (serversock6 == 0) {
				serversock6 = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
				if (serversock6 < 0) {
					lprint("[erro] Failed to create socket6\n");
					freeaddrinfo(servinfo);
					detach_core();
					return 1;
				}

				int _true = 1;
				if (setsockopt(serversock6, SOL_SOCKET, SO_REUSEADDR, &_true, sizeof(int)) < 0) {
					lprint("[erro] Failed to set socket option\n");
					freeaddrinfo(servinfo);
					detach_core();
					close(serversock6);
					return 1;
				}

				if (setsockopt(serversock6, IPPROTO_IPV6, IPV6_V6ONLY, &_true, sizeof(int)) < 0) {
					lprint("[erro] Failed to set socket option\n");
					freeaddrinfo(servinfo);
					detach_core();
					close(serversock6);
					return 1;
				}

				int opts = fcntl(serversock6, F_GETFL);
				if (opts < 0) {
					lprint("[erro] Failed to set nonblock on sock6\n");
					freeaddrinfo(servinfo);
					detach_core();
					close(serversock6);
					return 1;
				}

				opts = (opts | O_NONBLOCK);
				if (fcntl(serversock6, F_SETFL, opts) < 0) {
					lprint("[erro] Failed to set nonblock on sock6\n");
					freeaddrinfo(servinfo);
					detach_core();
					close(serversock6);
					return 1;
				}

				if (bind(serversock6, p->ai_addr, p->ai_addrlen) < 0) {
					lprintf("[erro] Failed to bind socket to port %d\n", API_PORT);
					freeaddrinfo(servinfo);
					detach_core();
					close(serversock6);
					return 1;
				}
			}
		}
	}

	freeaddrinfo(servinfo);

	if (serversock4 == 0 && serversock6 == 0) {
		lprint("[erro] Failed to bind any protocol\n");
		detach_core();
		return 1;
	}

	if (serversock4 != 0) {
		if (listen(serversock4, SOMAXCONN) < 0) {
			lprint("[erro] Failed to listen on socket4\n");
			detach_core();
			close(serversock4);
			return 1;
		}
		lprintf("[info] Listening on 0.0.0.0:%d\n", API_PORT);
	}

	if (serversock6 != 0) {
		if (listen(serversock6, SOMAXCONN) < 0) {
			lprint("[erro] Failed to listen on socket6\n");
			detach_core();
			close(serversock6);
			return 1;
		}
		lprintf("[info] Listening on [::]:%d\n", API_PORT);
	}
	lprintf("[info] Server agent " PROGNAME "/%s " VERSION_STRING "\n", get_version_string());

	signal(SIGINT, handle_shutdown);

	FD_ZERO(&readfds);
	FD_SET(serversock4, &readfds);
	FD_SET(serversock6, &readfds);
	max_sd = serversock4;
	if (serversock6 > max_sd)
		max_sd = serversock6;

	while (run) {
		int sd;
		readsock = readfds;
select_restart:
		if (select(max_sd + 1, &readsock, NULL, NULL, NULL) < 0) {
			if (errno == EINTR) {
				goto select_restart;
			}
			lprint("[erro] Failed to select socket\n");
			detach_core();
			close(serversock4);
			close(serversock6);
			return 1;
		}
		for (sd = 0; sd <= max_sd; ++sd) {
			if (FD_ISSET(sd, &readsock)) {
				if (sd == serversock4 || sd == serversock6) {
					struct sockaddr_storage addr;
					socklen_t size = sizeof(struct sockaddr_storage);
					int nsock = accept(sd, (struct sockaddr*)&addr, &size);
					if (nsock == -1) {
						lprint("[erro] Failed to acccept connection\n");
						detach_core();
						close(serversock4);
						close(serversock6);
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

	lprint("[info] Cleanup and detach core\n");
	detach_core();
	return 0;
}
