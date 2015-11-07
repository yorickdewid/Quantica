#ifdef LINUX
#if __STDC_VERSION__ >= 199901L
#define _XOPEN_SOURCE 700
#else
#define _XOPEN_SOURCE 500
#endif /* __STDC_VERSION__ */
#endif // LINUX

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
#include "core.h"
#include "sha1.h"
#include "md5.h"
#include "sha2.h"
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
	http_method_t method;
} http_request_t;

struct webroute {
	char uri[32];
	http_status_t (*api_handler)(char **response, http_request_t *req);
	unsigned char require_quid;
};

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

http_status_t api_not_found(char **response, http_request_t *req) {
	unused(req);
	strlcpy(*response, "{\"description\":\"API URI does not exist\",\"status\":\"NOT_FOUND\",\"success\":false}", RESPONSE_SIZE);
	return HTTP_NOT_FOUND;
}

http_status_t api_root(char **response, http_request_t *req) {
	unused(req);
	snprintf(*response, RESPONSE_SIZE, "{\"api_version\":%d,\"db_version\":\"%s\",\"instance\":{\"name\":\"%s\",\"quid\":\"%s\"},\"session\":{\"quid\":\"%s\"},\"license\":\"" LICENSE  "\",\"active\":true,\"description\":\"The server is ready to accept requests\",\"status\":\"COMMAND_OK\",\"success\":true}", API_VERSION, get_version_string(), get_instance_name(), get_instance_key(), get_session_key());
	return HTTP_OK;
}

http_status_t api_variables(char **response, http_request_t *req) {
	unused(req);
	char buf[26];
	char buf2[TIMENAME_SIZE + 1];
	buf2[TIMENAME_SIZE] = '\0';
	char *htime = tstostrf(buf, 32, get_timestamp(), ISO_8601_FORMAT);
	snprintf(*response, RESPONSE_SIZE, "{\"server\":{\"uptime\":\"%s\",\"client_requests\":%llu,\"port\":%d},\"engine\":{\"records\":%lu,\"free\":%lu,\"groups\":%lu,\"tablecache\":%d,\"datacache\":%d,\"datacache_density\":%d,\"dataheap\":\"" BINDATA "\"},\"date\":{\"timestamp\":%lld,\"unixtime\":%lld,\"datetime\":\"%s\",\"timename\":\"%s\"},\"version\":{\"release\":%d,\"major\":%d,\"minor\":%d},\"description\":\"Database statistics\",\"status\":\"COMMAND_OK\",\"success\":true}", get_uptime(), client_requests, API_PORT, stat_getkeys(), stat_getfreekeys(), stat_tablesize(), CACHE_SLOTS, DBCACHE_SLOTS, DBCACHE_DENSITY, get_timestamp(), get_unixtimestamp(), htime, timename_now(buf2), VERSION_RELESE, VERSION_MAJOR, VERSION_MINOR);
	return HTTP_OK;
}

http_status_t api_help(char **response, http_request_t *req) {
	unused(req);
	strlcpy(*response, "{\"api_options\":[\"/\",\"/help\",\"/license\",\"/stats\"],\"description\":\"Available API calls\",\"status\":\"COMMAND_OK\",\"success\":true}", RESPONSE_SIZE);
	return HTTP_OK;
}

http_status_t api_instance(char **response, http_request_t *req) {
	if (req->method == HTTP_POST || req->method == HTTP_PUT) {
		char *param_name = (char *)hashtable_get(req->data, "name");
		if (param_name) {
			set_instance_name(param_name);
			strlcpy(*response, "{\"description\":\"Instance name set\",\"status\":\"COMMAND_OK\",\"success\":true}", RESPONSE_SIZE);
			return HTTP_OK;
		}
		strlcpy(*response, "{\"description\":\"Request expects data\",\"status\":\"EMPTY_DATA\",\"success\":false}", RESPONSE_SIZE);
		return HTTP_OK;
	}
	snprintf(*response, RESPONSE_SIZE, "{\"name\":\"%s\",\"quid\":\"%s\",\"description\":\"Server instance name\",\"status\":\"COMMAND_OK\",\"success\":true}", get_instance_name(), get_instance_key());
	return HTTP_OK;
}

http_status_t api_sha1(char **response, http_request_t *req) {
	if (req->method == HTTP_POST || req->method == HTTP_PUT) {
		char *param_data = (char *)hashtable_get(req->data, "data");
		if (param_data) {
			char strsha[SHA1_LENGTH + 1];
			strsha[SHA1_LENGTH] = '\0';
			if (crypto_sha1(strsha, param_data) < 0) {
				strsha[SHA1_LENGTH] = '\0';
				snprintf(*response, RESPONSE_SIZE, "{\"error_code\":%d,\"description\":\"Unknown error\",\"status\":\"ERROR_UNKNOWN\",\"success\":false}", GETERROR());
				return HTTP_OK;
			}
			snprintf(*response, RESPONSE_SIZE, "{\"hash\":\"%s\",\"description\":\"Data hashed with SHA-1\",\"status\":\"COMMAND_OK\",\"success\":true}", strsha);
			return HTTP_OK;
		}
		strlcpy(*response, "{\"description\":\"Request expects data\",\"status\":\"EMPTY_DATA\",\"success\":false}", RESPONSE_SIZE);
		return HTTP_OK;
	}
	strlcpy(*response, "{\"description\":\"This call requires POST/PUT requests\",\"status\":\"WRONG_METHOD\",\"success\":false}", RESPONSE_SIZE);
	return HTTP_OK;
}

http_status_t api_md5(char **response, http_request_t *req) {
	if (req->method == HTTP_POST || req->method == HTTP_PUT) {
		char *param_data = (char *)hashtable_get(req->data, "data");
		if (param_data) {
			char strmd5[MD5_SIZE + 1];
			strmd5[MD5_SIZE] = '\0';
			crypto_md5(strmd5, param_data);
			snprintf(*response, RESPONSE_SIZE, "{\"hash\":\"%s\",\"description\":\"Data hashed with MD5\",\"status\":\"COMMAND_OK\",\"success\":true}", strmd5);
			return HTTP_OK;
		}
		strlcpy(*response, "{\"description\":\"Request expects data\",\"status\":\"EMPTY_DATA\",\"success\":false}", RESPONSE_SIZE);
		return HTTP_OK;
	}
	strlcpy(*response, "{\"description\":\"This call requires POST/PUT requests\",\"status\":\"WRONG_METHOD\",\"success\":false}", RESPONSE_SIZE);
	return HTTP_OK;
}

http_status_t api_sha256(char **response, http_request_t *req) {
	if (req->method == HTTP_POST || req->method == HTTP_PUT) {
		char *param_data = (char *)hashtable_get(req->data, "data");
		if (param_data) {
			char strsha256[(2 * SHA256_DIGEST_SIZE) + 1];
			strsha256[2 * SHA256_DIGEST_SIZE] = '\0';
			crypto_sha256(strsha256, param_data);
			snprintf(*response, RESPONSE_SIZE, "{\"hash\":\"%s\",\"description\":\"Data hashed with SHA256\",\"status\":\"COMMAND_OK\",\"success\":true}", strsha256);
			return HTTP_OK;
		}
		strlcpy(*response, "{\"description\":\"Request expects data\",\"status\":\"EMPTY_DATA\",\"success\":false}", RESPONSE_SIZE);
		return HTTP_OK;
	}
	strlcpy(*response, "{\"description\":\"This call requires POST/PUT requests\",\"status\":\"WRONG_METHOD\",\"success\":false}", RESPONSE_SIZE);
	return HTTP_OK;
}

http_status_t api_sha512(char **response, http_request_t *req) {
	if (req->method == HTTP_POST || req->method == HTTP_PUT) {
		char *param_data = (char *)hashtable_get(req->data, "data");
		if (param_data) {
			char strsha512[(2 * SHA512_DIGEST_SIZE) + 1];
			strsha512[(2 * SHA512_DIGEST_SIZE)] = '\0';
			crypto_sha512(strsha512, param_data);
			snprintf(*response, RESPONSE_SIZE, "{\"hmac\":\"%s\",\"description\":\"Data hashed with SHA512\",\"status\":\"COMMAND_OK\",\"success\":true}", strsha512);
			return HTTP_OK;
		}
		strlcpy(*response, "{\"description\":\"Request expects data\",\"status\":\"EMPTY_DATA\",\"success\":false}", RESPONSE_SIZE);
		return HTTP_OK;
	}
	strlcpy(*response, "{\"description\":\"This call requires POST/PUT requests\",\"status\":\"WRONG_METHOD\",\"success\":false}", RESPONSE_SIZE);
	return HTTP_OK;
}

http_status_t api_hmac_sha256(char **response, http_request_t *req) {
	if (req->method == HTTP_POST || req->method == HTTP_PUT) {
		char *param_data = (char *)hashtable_get(req->data, "data");
		char *param_key = (char *)hashtable_get(req->data, "key");
		if (param_data && param_key) {
			char mac[SHA256_BLOCK_SIZE + 1];
			mac[SHA256_BLOCK_SIZE] = '\0';
			crypto_hmac_sha256(mac, param_key, param_data);
			snprintf(*response, RESPONSE_SIZE, "{\"hmac\":\"%s\",\"description\":\"Data signed with HMAC-SHA256\",\"status\":\"COMMAND_OK\",\"success\":true}", mac);
			return HTTP_OK;
		}
		strlcpy(*response, "{\"description\":\"Request expects data\",\"status\":\"EMPTY_DATA\",\"success\":false}", RESPONSE_SIZE);
		return HTTP_OK;
	}
	strlcpy(*response, "{\"description\":\"This call requires POST/PUT requests\",\"status\":\"WRONG_METHOD\",\"success\":false}", RESPONSE_SIZE);
	return HTTP_OK;
}

http_status_t api_hmac_sha512(char **response, http_request_t *req) {
	if (req->method == HTTP_POST || req->method == HTTP_PUT) {
		char *param_data = (char *)hashtable_get(req->data, "data");
		char *param_key = (char *)hashtable_get(req->data, "key");
		if (param_data && param_key) {
			char mac[SHA512_BLOCK_SIZE + 1];
			mac[SHA512_BLOCK_SIZE] = '\0';
			crypto_hmac_sha512(mac, param_key, param_data);
			snprintf(*response, RESPONSE_SIZE, "{\"hash\":\"%s\",\"description\":\"Data signed with HMAC-SHA256\",\"status\":\"COMMAND_OK\",\"success\":true}", mac);
			return HTTP_OK;
		}
		strlcpy(*response, "{\"description\":\"Request expects data\",\"status\":\"EMPTY_DATA\",\"success\":false}", RESPONSE_SIZE);
		return HTTP_OK;
	}
	strlcpy(*response, "{\"description\":\"This call requires POST/PUT requests\",\"status\":\"WRONG_METHOD\",\"success\":false}", RESPONSE_SIZE);
	return HTTP_OK;
}

http_status_t api_sqlquery(char **response, http_request_t *req) {
	if (req->method == HTTP_POST || req->method == HTTP_PUT) {
		char *param_data = (char *)hashtable_get(req->data, "query");
		if (param_data) {
			size_t len = 0, resplen;
			sqlresult_t *data = exec_sqlquery(param_data, &len);
			if (ISERROR()) {
				if (IFERROR(EREC_NOTFOUND)) {
					snprintf(*response, RESPONSE_SIZE, "{\"error_code\":%d,\"description\":\"The requested record does not exist\",\"status\":\"REC_NOTFOUND\",\"success\":false}", GETERROR());
					return HTTP_OK;
				} else if (IFERROR(ESQL_TOKEN)) {
					snprintf(*response, RESPONSE_SIZE, "{\"error_code\":%d,\"description\":\"Invalid token in query\",\"status\":\"SQL_TOKEN\",\"success\":false}", GETERROR());
					return HTTP_OK;
				} else if (IFERROR(ESQL_PARSE_END)) {
					snprintf(*response, RESPONSE_SIZE, "{\"error_code\":%d,\"description\":\"Unexpected end of query\",\"status\":\"SQL_PARSE_END\",\"success\":false}", GETERROR());
					return HTTP_OK;
				} else if (IFERROR(ESQL_PARSE_VAL)) {
					snprintf(*response, RESPONSE_SIZE, "{\"error_code\":%d,\"description\":\"Expecting value\",\"status\":\"SQL_PARSE_VAL\",\"success\":false}", GETERROR());
					return HTTP_OK;
				} else if (IFERROR(ESQL_PARSE_TOK)) {
					snprintf(*response, RESPONSE_SIZE, "{\"error_code\":%d,\"description\":\"Unexpected token\",\"status\":\"SQL_PARSE_TOK\",\"success\":false}", GETERROR());
					return HTTP_OK;
				} else if (IFERROR(ESQL_PARSE_STCT)) {
					snprintf(*response, RESPONSE_SIZE, "{\"error_code\":%d,\"description\":\"Invalid query construction\",\"status\":\"SQL_PARSE_STCT\",\"success\":false}", GETERROR());
					return HTTP_OK;
				} else {
					snprintf(*response, RESPONSE_SIZE, "{\"error_code\":%d,\"description\":\"Unknown error\",\"status\":\"ERROR_UNKNOWN\",\"success\":false}", GETERROR());
					return HTTP_OK;
				}
			}
			resplen = RESPONSE_SIZE;
			if (len > RESPONSE_SIZE) {
				resplen = RESPONSE_SIZE + len;
				*response = zrealloc(*response, resplen);
			}
			if (data->quid[0] != '\0')
				snprintf(*response, resplen, "{\"quid\":\"%s\",\"items\":%d,\"description\":\"Data stored in record\",\"status\":\"COMMAND_OK\",\"success\":true}", data->quid, data->items);
			else if (data->name) {
				snprintf(*response, resplen, "{\"%s\":\"%s\",\"description\":\"Query executed\",\"status\":\"COMMAND_OK\",\"success\":true}", data->name, (char *)data->data);
				zfree(data->name);
				zfree(data->data);
			} else if (data->data) {
				snprintf(*response, resplen, "{\"data\":%s,\"description\":\"Retrieve record by requested key\",\"status\":\"COMMAND_OK\",\"success\":true}", (char *)data->data);
				zfree(data->data);
			} else
				snprintf(*response, resplen, "{\"description\":\"Query executed\",\"status\":\"COMMAND_OK\",\"success\":true}");
			return HTTP_OK;
		}
		strlcpy(*response, "{\"description\":\"Request expects query\",\"status\":\"EMPTY_DATA\",\"success\":false}", RESPONSE_SIZE);
		return HTTP_OK;
	}
	strlcpy(*response, "{\"description\":\"This call requires POST/PUT requests\",\"status\":\"WRONG_METHOD\",\"success\":false}", RESPONSE_SIZE);
	return HTTP_OK;
}

http_status_t api_vacuum(char **response, http_request_t *req) {
	unused(req);
	if (db_vacuum() < 0) {
		if (IFERROR(EDB_LOCKED)) {
			snprintf(*response, RESPONSE_SIZE, "{\"error_code\":%d,\"description\":\"Database is locked\",\"status\":\"REC_LOCKED\",\"success\":false}", GETERROR());
			return HTTP_OK;
		} else {
			snprintf(*response, RESPONSE_SIZE, "{\"error_code\":%d,\"description\":\"Unknown error\",\"status\":\"ERROR_UNKNOWN\",\"success\":false}", GETERROR());
			return HTTP_OK;
		}
	} else {
		strlcpy(*response, "{\"description\":\"Vacuum succeeded\",\"status\":\"COMMAND_OK\",\"success\":true}", RESPONSE_SIZE);
	}
	return HTTP_OK;
}

http_status_t api_sync(char **response, http_request_t *req) {
	unused(req);
	filesync();
	strlcpy(*response, "{\"description\":\"Block synchronization succeeded\",\"status\":\"COMMAND_OK\",\"success\":true}", RESPONSE_SIZE);
	return HTTP_OK;
}

http_status_t api_gen_quid(char **response, http_request_t *req) {
	unused(req);
	char squid[QUID_LENGTH + 1];
	quid_generate(squid);
	snprintf(*response, RESPONSE_SIZE, "{\"quid\":\"%s\",\"description\":\"New QUID generated\",\"status\":\"COMMAND_OK\",\"success\":true}", squid);
	return HTTP_OK;
}

http_status_t api_shutdown(char **response, http_request_t *req) {
	unused(req);
	run = 0;
	strlcpy(*response, "{\"description\":\"Shutting down database\",\"status\":\"COMMAND_OK\",\"success\":true}", RESPONSE_SIZE);
	return HTTP_OK;
}

http_status_t api_base64_enc(char **response, http_request_t *req) {
	if (req->method == HTTP_POST || req->method == HTTP_PUT) {
		char *param_data = (char *)hashtable_get(req->data, "data");
		if (param_data) {
			char *enc = crypto_base64_enc(param_data);
			snprintf(*response, RESPONSE_SIZE, "{\"encode\":\"%s\",\"description\":\"Data encoded with base64\",\"status\":\"COMMAND_OK\",\"success\":true}", enc);
			zfree(enc);
			return HTTP_OK;
		}
		strlcpy(*response, "{\"description\":\"Request expects data\",\"status\":\"EMPTY_DATA\",\"success\":false}", RESPONSE_SIZE);
		return HTTP_OK;
	}
	strlcpy(*response, "{\"description\":\"This call requires POST/PUT requests\",\"status\":\"WRONG_METHOD\",\"success\":false}", RESPONSE_SIZE);
	return HTTP_OK;
}

http_status_t api_base64_dec(char **response, http_request_t *req) {
	if (req->method == HTTP_POST || req->method == HTTP_PUT) {
		char *param_data = (char *)hashtable_get(req->data, "data");
		if (param_data) {
			char *enc = crypto_base64_dec(param_data);
			snprintf(*response, RESPONSE_SIZE, "{\"encode\":\"%s\",\"description\":\"Data encoded with base64\",\"status\":\"COMMAND_OK\",\"success\":true}", enc);
			zfree(enc);
			return HTTP_OK;
		}
		strlcpy(*response, "{\"description\":\"Request expects data\",\"status\":\"EMPTY_DATA\",\"success\":false}", RESPONSE_SIZE);
		return HTTP_OK;
	}
	strlcpy(*response, "{\"description\":\"This call requires POST/PUT requests\",\"status\":\"WRONG_METHOD\",\"success\":false}", RESPONSE_SIZE);
	return HTTP_OK;
}

http_status_t api_db_put(char **response, http_request_t *req) {
	if (req->method == HTTP_POST || req->method == HTTP_PUT) {
		char *param_data = (char *)hashtable_get(req->data, "data");
		if (param_data) {
			char squid[QUID_LENGTH + 1];
			int items = 0;
			if (db_put(squid, &items, param_data, strlen(param_data)) < 0) {
				snprintf(*response, RESPONSE_SIZE, "{\"error_code\":%d,\"description\":\"Unknown error\",\"status\":\"ERROR_UNKNOWN\",\"success\":false}", GETERROR());
				return HTTP_OK;
			}
			snprintf(*response, RESPONSE_SIZE, "{\"quid\":\"%s\",\"items\":%d,\"description\":\"Data stored in record\",\"status\":\"COMMAND_OK\",\"success\":true}", squid, items);
			return HTTP_OK;
		}
		strlcpy(*response, "{\"description\":\"Request expects data\",\"status\":\"EMPTY_DATA\",\"success\":false}", RESPONSE_SIZE);
		return HTTP_OK;
	}
	strlcpy(*response, "{\"description\":\"This call requires POST/PUT requests\",\"status\":\"WRONG_METHOD\",\"success\":false}", RESPONSE_SIZE);
	return HTTP_OK;
}

http_status_t api_db_get(char **response, http_request_t *req) {
	char *param_quid = (char *)hashtable_get(req->data, "quid");
	char *noresolve = (char *)hashtable_get(req->data, "noresolve");
	if (param_quid) {
		size_t len = 0, resplen;
		bool resolve = TRUE;
		if (noresolve && !strcmp(noresolve, "true")) {
			resolve = FALSE;
		}
		char *data = db_get(param_quid, &len, resolve);
		if (!data) {
			if (IFERROR(EREC_NOTFOUND)) {
				snprintf(*response, RESPONSE_SIZE, "{\"error_code\":%d,\"description\":\"The requested record does not exist\",\"status\":\"REC_NOTFOUND\",\"success\":false}", GETERROR());
				return HTTP_OK;
			} else {
				snprintf(*response, RESPONSE_SIZE, "{\"error_code\":%d,\"description\":\"Unknown error\",\"status\":\"ERROR_UNKNOWN\",\"success\":false}", GETERROR());
				return HTTP_OK;
			}
		}
		len = strlen(data);
		resplen = RESPONSE_SIZE;
		if (len > (RESPONSE_SIZE / 2)) {
			resplen = RESPONSE_SIZE + len;
			*response = zrealloc(*response, resplen);
		}
		snprintf(*response, resplen, "{\"data\":%s,\"description\":\"Retrieve record by requested key\",\"status\":\"COMMAND_OK\",\"success\":true}", data);
		zfree(data);
		return HTTP_OK;
	}
	strlcpy(*response, "{\"description\":\"Request expects data\",\"status\":\"EMPTY_DATA\",\"success\":false}", RESPONSE_SIZE);
	return HTTP_OK;
}

http_status_t api_db_get_type(char **response, http_request_t *req) {
	char *param_quid = (char *)hashtable_get(req->data, "quid");
	if (param_quid) {
		char *type = db_get_type(param_quid);
		if (!type) {
			if (IFERROR(EREC_NOTFOUND)) {
				snprintf(*response, RESPONSE_SIZE, "{\"error_code\":%d,\"description\":\"The requested record does not exist\",\"status\":\"REC_NOTFOUND\",\"success\":false}", GETERROR());
				return HTTP_OK;
			} else {
				snprintf(*response, RESPONSE_SIZE, "{\"error_code\":%d,\"description\":\"Unknown error\",\"status\":\"ERROR_UNKNOWN\",\"success\":false}", GETERROR());
				return HTTP_OK;
			}
		}
		snprintf(*response, RESPONSE_SIZE, "{\"datatype\":\"%s\",\"description\":\"Datatype of record value\",\"status\":\"COMMAND_OK\",\"success\":true}", type);
		return HTTP_OK;
	}
	strlcpy(*response, "{\"description\":\"Request expects data\",\"status\":\"EMPTY_DATA\",\"success\":false}", RESPONSE_SIZE);
	return HTTP_OK;
}

http_status_t api_db_get_schema(char **response, http_request_t *req) {
	char *param_quid = (char *)hashtable_get(req->data, "quid");
	if (param_quid) {
		char *schema = db_get_schema(param_quid);
		if (!schema) {
			if (IFERROR(EREC_NOTFOUND)) {
				snprintf(*response, RESPONSE_SIZE, "{\"error_code\":%d,\"description\":\"The requested record does not exist\",\"status\":\"REC_NOTFOUND\",\"success\":false}", GETERROR());
				return HTTP_OK;
			} else {
				snprintf(*response, RESPONSE_SIZE, "{\"error_code\":%d,\"description\":\"Unknown error\",\"status\":\"ERROR_UNKNOWN\",\"success\":false}", GETERROR());
				return HTTP_OK;
			}
		}
		snprintf(*response, RESPONSE_SIZE, "{\"dataschema\":\"%s\",\"description\":\"Datatype of record value\",\"status\":\"COMMAND_OK\",\"success\":true}", schema);
		return HTTP_OK;
	}
	strlcpy(*response, "{\"description\":\"Request expects data\",\"status\":\"EMPTY_DATA\",\"success\":false}", RESPONSE_SIZE);
	return HTTP_OK;
}

http_status_t api_db_delete(char **response, http_request_t *req) {
	char *param_quid = (char *)hashtable_get(req->data, "quid");
	if (param_quid) {
		if (db_delete(param_quid) < 0) {
			if (IFERROR(EREC_LOCKED)) {
				snprintf(*response, RESPONSE_SIZE, "{\"error_code\":%d,\"description\":\"Record is locked\",\"status\":\"REC_LOCKED\",\"success\":false}", GETERROR());
				return HTTP_OK;
			} else if (IFERROR(EREC_NOTFOUND)) {
				snprintf(*response, RESPONSE_SIZE, "{\"error_code\":%d,\"description\":\"The requested record does not exist\",\"status\":\"REC_NOTFOUND\",\"success\":false}", GETERROR());
				return HTTP_OK;
			} else {
				snprintf(*response, RESPONSE_SIZE, "{\"error_code\":%d,\"description\":\"Unknown error\",\"status\":\"ERROR_UNKNOWN\",\"success\":false}", GETERROR());
				return HTTP_OK;
			}
		}
		snprintf(*response, RESPONSE_SIZE, "{\"description\":\"Record deleted\",\"status\":\"COMMAND_OK\",\"success\":true}");
		return HTTP_OK;
	}
	strlcpy(*response, "{\"description\":\"Request expects data\",\"status\":\"EMPTY_DATA\",\"success\":false}", RESPONSE_SIZE);
	return HTTP_OK;
}

http_status_t api_db_purge(char **response, http_request_t *req) {
	char *param_quid = (char *)hashtable_get(req->data, "quid");
	if (param_quid) {
		if (db_purge(param_quid) < 0) {
			if (IFERROR(EREC_LOCKED)) {
				snprintf(*response, RESPONSE_SIZE, "{\"error_code\":%d,\"description\":\"Record is locked\",\"status\":\"REC_LOCKED\",\"success\":false}", GETERROR());
				return HTTP_OK;
			} else if (IFERROR(EREC_NOTFOUND)) {
				snprintf(*response, RESPONSE_SIZE, "{\"error_code\":%d,\"description\":\"The requested record does not exist\",\"status\":\"REC_NOTFOUND\",\"success\":false}", GETERROR());
				return HTTP_OK;
			} else {
				snprintf(*response, RESPONSE_SIZE, "{\"error_code\":%d,\"description\":\"Unknown error\",\"status\":\"ERROR_UNKNOWN\",\"success\":false}", GETERROR());
				return HTTP_OK;
			}
		}
		snprintf(*response, RESPONSE_SIZE, "{\"description\":\"Record purged\",\"status\":\"COMMAND_OK\",\"success\":true}");
		return HTTP_OK;
	}
	strlcpy(*response, "{\"description\":\"Request expects data\",\"status\":\"EMPTY_DATA\",\"success\":false}", RESPONSE_SIZE);
	return HTTP_OK;
}

http_status_t api_db_update(char **response, http_request_t *req) {
	if (req->method == HTTP_POST || req->method == HTTP_PUT) {
		char *param_data = (char *)hashtable_get(req->data, "data");
		char *param_quid = (char *)hashtable_get(req->data, "quid");
		if (param_data && param_quid) {
			int items = 0;
			if (db_update(param_quid, &items, param_data, strlen(param_data)) < 0) {
				if (IFERROR(EREC_LOCKED)) {
					snprintf(*response, RESPONSE_SIZE, "{\"error_code\":%d,\"description\":\"Record is locked\",\"status\":\"REC_LOCKED\",\"success\":false}", GETERROR());
					return HTTP_OK;
				} else if (IFERROR(EREC_NOTFOUND)) {
					snprintf(*response, RESPONSE_SIZE, "{\"error_code\":%d,\"description\":\"The requested record does not exist\",\"status\":\"REC_NOTFOUND\",\"success\":false}", GETERROR());
					return HTTP_OK;
				} else {
					snprintf(*response, RESPONSE_SIZE, "{\"error_code\":%d,\"description\":\"Unknown error\",\"status\":\"ERROR_UNKNOWN\",\"success\":false}", GETERROR());
					return HTTP_OK;
				}
			}
			snprintf(*response, RESPONSE_SIZE, "{\"description\":\"Record updated\",\"status\":\"COMMAND_OK\",\"success\":true}");
			return HTTP_OK;
		}
		strlcpy(*response, "{\"description\":\"Request expects data\",\"status\":\"EMPTY_DATA\",\"success\":false}", RESPONSE_SIZE);
		return HTTP_OK;
	}
	strlcpy(*response, "{\"description\":\"This call requires POST/PUT requests\",\"status\":\"WRONG_METHOD\",\"success\":false}", RESPONSE_SIZE);
	return HTTP_OK;
}

http_status_t api_db_index(char **response, http_request_t *req) {
	if (req->method == HTTP_POST || req->method == HTTP_PUT) {
		char *param_key = (char *)hashtable_get(req->data, "key");
		char *param_quid = (char *)hashtable_get(req->data, "quid");
		if (param_key && param_quid) {
			if (db_create_index(param_quid, param_key) < 0) {
				snprintf(*response, RESPONSE_SIZE, "{\"error_code\":%d,\"description\":\"Unknown error\",\"status\":\"ERROR_UNKNOWN\",\"success\":false}", GETERROR());
				return HTTP_OK;
			}
			/*int items = 0;
			if (db_update(param_quid, &items, param_data, strlen(param_data))<0) {
				if(IFERROR(EREC_LOCKED)) {
					snprintf(*response, RESPONSE_SIZE, "{\"error_code\":%d,\"description\":\"Record is locked\",\"status\":\"REC_LOCKED\",\"success\":false}", GETERROR());
					return HTTP_OK;
				} else if(IFERROR(EREC_NOTFOUND)) {
					snprintf(*response, RESPONSE_SIZE, "{\"error_code\":%d,\"description\":\"The requested record does not exist\",\"status\":\"REC_NOTFOUND\",\"success\":false}", GETERROR());
					return HTTP_OK;
				} else {
					snprintf(*response, RESPONSE_SIZE, "{\"error_code\":%d,\"description\":\"Unknown error\",\"status\":\"ERROR_UNKNOWN\",\"success\":false}", GETERROR());
					return HTTP_OK;
				}
			}*/
			snprintf(*response, RESPONSE_SIZE, "{\"description\":\"Index created\",\"status\":\"COMMAND_OK\",\"success\":true}");
			return HTTP_OK;
		}
		strlcpy(*response, "{\"description\":\"Request expects key to be given\",\"status\":\"EMPTY_KEY\",\"success\":false}", RESPONSE_SIZE);
		return HTTP_OK;
	}
	strlcpy(*response, "{\"description\":\"This call requires POST/PUT requests\",\"status\":\"WRONG_METHOD\",\"success\":false}", RESPONSE_SIZE);
	return HTTP_OK;
}

http_status_t api_rec_meta(char **response, http_request_t *req) {
	char *param_quid = (char *)hashtable_get(req->data, "quid");
	if (param_quid) {
		struct record_status status;
		if (req->method == HTTP_POST || req->method == HTTP_PUT) {
			if (db_record_get_meta(param_quid, &status) < 0) {
				if (IFERROR(EREC_LOCKED)) {
					snprintf(*response, RESPONSE_SIZE, "{\"error_code\":%d,\"description\":\"Record is locked\",\"status\":\"REC_LOCKED\",\"success\":false}", GETERROR());
					return HTTP_OK;
				} else if (IFERROR(EREC_NOTFOUND)) {
					snprintf(*response, RESPONSE_SIZE, "{\"error_code\":%d,\"description\":\"The requested record does not exist\",\"status\":\"REC_NOTFOUND\",\"success\":false}", GETERROR());
					return HTTP_OK;
				} else {
					snprintf(*response, RESPONSE_SIZE, "{\"error_code\":%d,\"description\":\"Unknown error\",\"status\":\"ERROR_UNKNOWN\",\"success\":false}", GETERROR());
					return HTTP_OK;
				}
			}
			char *param_error = (char *)hashtable_get(req->data, "error");
			if (param_error)
				status.error = atoi(param_error);
			char *param_exec = (char *)hashtable_get(req->data, "executable");
			if (param_exec)
				status.exec = atoi(param_exec);
			char *param_freeze = (char *)hashtable_get(req->data, "freeze");
			if (param_freeze)
				status.freeze = atoi(param_freeze);
			char *param_importance = (char *)hashtable_get(req->data, "importance");
			if (param_importance)
				status.importance = atoi(param_importance);
			char *param_lifecycle = (char *)hashtable_get(req->data, "lifecycle");
			if (param_lifecycle)
				strlcpy(status.lifecycle, param_lifecycle, STATUS_LIFECYCLE_SIZE);
			char *param_lock = (char *)hashtable_get(req->data, "system_lock");
			if (param_lock)
				status.syslock = atoi(param_lock);
			char *param_type = (char *)hashtable_get(req->data, "type");
			if (param_type)
				strlcpy(status.type, param_type, STATUS_TYPE_SIZE);
			if (db_record_set_meta(param_quid, &status) < 0) {
				if (IFERROR(EREC_LOCKED)) {
					snprintf(*response, RESPONSE_SIZE, "{\"error_code\":%d,\"description\":\"Record is locked\",\"status\":\"REC_LOCKED\",\"success\":false}", GETERROR());
					return HTTP_OK;
				} else if (IFERROR(EREC_NOTFOUND)) {
					snprintf(*response, RESPONSE_SIZE, "{\"error_code\":%d,\"description\":\"The requested record does not exist\",\"status\":\"REC_NOTFOUND\",\"success\":false}", GETERROR());
					return HTTP_OK;
				} else {
					snprintf(*response, RESPONSE_SIZE, "{\"error_code\":%d,\"description\":\"Unknown error\",\"status\":\"ERROR_UNKNOWN\",\"success\":false}", GETERROR());
					return HTTP_OK;
				}
			}
			snprintf(*response, RESPONSE_SIZE, "{\"description\":\"Record updated\",\"status\":\"COMMAND_OK\",\"success\":true}");
			return HTTP_OK;
		}
		if (db_record_get_meta(param_quid, &status) < 0) {
			if (IFERROR(EREC_NOTFOUND)) {
				snprintf(*response, RESPONSE_SIZE, "{\"error_code\":%d,\"description\":\"The requested record does not exist\",\"status\":\"REC_NOTFOUND\",\"success\":false}", GETERROR());
				return HTTP_OK;
			} else {
				snprintf(*response, RESPONSE_SIZE, "{\"error_code\":%d,\"description\":\"Unknown error\",\"status\":\"ERROR_UNKNOWN\",\"success\":false}", GETERROR());
			}
		}
		snprintf(*response, RESPONSE_SIZE, "{\"error\":%s,\"freeze\":%s,\"executable\":%s,\"system_lock\":%s,\"lifecycle\":\"%s\",\"importance\":%d,\"type\":\"%s\",\"description\":\"Record metadata queried\",\"status\":\"COMMAND_OK\",\"success\":true}", str_bool(status.error), str_bool(status.freeze), str_bool(status.exec), str_bool(status.syslock), status.lifecycle, status.importance, status.type);
		return HTTP_OK;
	}
	strlcpy(*response, "{\"description\":\"Request expects data\",\"status\":\"EMPTY_DATA\",\"success\":false}", RESPONSE_SIZE);
	return HTTP_OK;
}

http_status_t api_db_listget(char **response, http_request_t *req) {
	char *param_quid = (char *)hashtable_get(req->data, "quid");
	if (param_quid) {
		if (req->method == HTTP_POST || req->method == HTTP_PUT) {
			char *param_name = (char *)hashtable_get(req->data, "name");
			if (param_name && param_quid) {
				if (db_list_update(param_quid, param_name) < 0) {
					snprintf(*response, RESPONSE_SIZE, "{\"error_code\":%d,\"description\":\"The requested record does not exist\",\"status\":\"REC_NOTFOUND\",\"success\":false}", EREC_NOTFOUND);
					return HTTP_OK;
				}
				snprintf(*response, RESPONSE_SIZE, "{\"description\":\"Table renamed\",\"status\":\"COMMAND_OK\",\"success\":true}");
				return HTTP_OK;
			}
			strlcpy(*response, "{\"description\":\"Request expects data\",\"status\":\"EMPTY_DATA\",\"success\":false}", RESPONSE_SIZE);
			return HTTP_OK;
		} else {
			char *name = db_list_get(param_quid);
			if (!name) {
				if (IFERROR(ETBL_NOTFOUND)) {
					snprintf(*response, RESPONSE_SIZE, "{\"error_code\":%d,\"description\":\"The requested record has no tablelist entry\",\"status\":\"TBL_NOTFOUND\",\"success\":false}", GETERROR());
					return HTTP_OK;
				} else {
					snprintf(*response, RESPONSE_SIZE, "{\"error_code\":%d,\"description\":\"Unknown error\",\"status\":\"ERROR_UNKNOWN\",\"success\":false}", GETERROR());
					return HTTP_OK;
				}
			}
			snprintf(*response, RESPONSE_SIZE, "{\"name\":\"%s\",\"description\":\"Get in list\",\"status\":\"COMMAND_OK\",\"success\":true}", name);
			zfree(name);
			return HTTP_OK;
		}
	}
	strlcpy(*response, "{\"description\":\"Request expects data\",\"status\":\"EMPTY_DATA\",\"success\":false}", RESPONSE_SIZE);
	return HTTP_OK;
}

http_status_t api_table_get(char **response, http_request_t *req) {
	size_t len = 0, resplen;
	bool resolve = TRUE;
	if (req->querystring) {
		char *noresolve = (char *)hashtable_get(req->querystring, "noresolve");
		if (noresolve && !strcmp(noresolve, "true")) {
			resolve = FALSE;
		}
	}
	char *data = db_table_get(req->uri, &len, resolve);
	if (!data) {
		if (IFERROR(EREC_NOTFOUND)) {
			snprintf(*response, RESPONSE_SIZE, "{\"error_code\":%d,\"description\":\"The requested record does not exist\",\"status\":\"REC_NOTFOUND\",\"success\":false}", GETERROR());
			return HTTP_OK;
		} else if (IFERROR(ETBL_NOTFOUND)) {
			snprintf(*response, RESPONSE_SIZE, "{\"error_code\":%d,\"description\":\"The requested record has no tablelist entry\",\"status\":\"TBL_NOTFOUND\",\"success\":false}", GETERROR());
			return HTTP_OK;
		} else {
			snprintf(*response, RESPONSE_SIZE, "{\"error_code\":%d,\"description\":\"Unknown error\",\"status\":\"ERROR_UNKNOWN\",\"success\":false}", GETERROR());
			return HTTP_OK;
		}
	}
	resplen = RESPONSE_SIZE;
	if (len > (RESPONSE_SIZE / 2)) {
		resplen = RESPONSE_SIZE + len;
		*response = zrealloc(*response, resplen);
	}
	snprintf(*response, resplen, "{\"data\":%s,\"description\":\"Retrieve record by requested key\",\"status\":\"COMMAND_OK\",\"success\":true}", data);
	zfree(data);
	return HTTP_OK;
}

http_status_t api_listall(char **response, http_request_t *req) {
	unused(req);
	size_t len = 0, resplen;

	char *table = db_list_all();
	if (!table) {
		snprintf(*response, RESPONSE_SIZE, "{\"error_code\":%d,\"description\":\"Server not ready\",\"status\":\"NOT_READY\",\"success\":false}", ENOT_READY);
		return HTTP_OK;
	}

	len = strlen(table);
	resplen = RESPONSE_SIZE;
	if (len > (RESPONSE_SIZE / 2)) {
		resplen = RESPONSE_SIZE + len;
		*response = zrealloc(*response, resplen);
	}
	snprintf(*response, resplen, "{\"table\":%s,\"description\":\"Listening tables\",\"status\":\"COMMAND_OK\",\"success\":true}", table);
	zfree(table);
	return HTTP_OK;
}

static const struct webroute route[] = {
	/* URL				callback			QUID*/
	{"/",				api_root,			FALSE},
#if 0
	{"/help",			api_help,			FALSE},
	{"/api",			api_help,			FALSE},
#endif
	{"/instance",		api_instance,		FALSE},
	{"/sha1",			api_sha1,			FALSE},
	{"/md5",			api_md5,			FALSE},
	{"/sha256",			api_sha256,			FALSE},
	{"/sha512",			api_sha512,			FALSE},
	{"/hmac/sha256",	api_hmac_sha256,	FALSE},
	{"/hmac/sha512",	api_hmac_sha512,	FALSE},
	{"/sql",			api_sqlquery,		FALSE},
	{"/vacuum",			api_vacuum,			FALSE},
	{"/sync",			api_sync,			FALSE},
	{"/vars",			api_variables,		FALSE},
	{"/quid",			api_gen_quid,		FALSE},
	{"/shutdown",		api_shutdown,		FALSE},
	{"/base64/enc",		api_base64_enc,		FALSE},
	{"/base64/dec",		api_base64_dec,		FALSE},
	{"/put",			api_db_put,			FALSE},
	{"/store",			api_db_put,			FALSE},
	{"/insert",			api_db_put,			FALSE},
	{"/table/*",		api_table_get,		FALSE},
	{"/tablelist",		api_listall,		FALSE},
	{"/get",			api_db_get,			TRUE},
	{"/retrieve",		api_db_get,			TRUE},
	{"/type",			api_db_get_type,	TRUE},
	{"/schema",			api_db_get_schema,	TRUE},
	{"/delete",			api_db_delete,		TRUE},
	{"/remove",			api_db_delete,		TRUE},
	{"/purge",			api_db_purge,		TRUE},
	{"/update",			api_db_update,		TRUE},
	{"/index",			api_db_index,		TRUE},
	{"/meta",			api_rec_meta,		TRUE},
	{"/name",			api_db_listget,		TRUE},
};

void handle_request(int sd, fd_set *set) {
	FILE *socket_stream = fdopen(sd, "r+");
	if (!socket_stream) {
		lprint("[erro] Failed to get file descriptor\n");
		goto disconnect;
	};

	struct sockaddr_storage addr;
	char str_addr[INET6_ADDRSTRLEN];
	socklen_t len = sizeof(addr);
	getpeername(sd, (struct sockaddr*)&addr, &len);
	inet_ntop(addr.ss_family, get_in_addr((struct sockaddr *)&addr), str_addr, sizeof(str_addr));

	vector_t *queue = alloc_vector(VECTOR_RHEAD_SIZE);
	vector_t *headers = alloc_vector(VECTOR_SHEAD_SIZE);
	hashtable_t *postdata = NULL;
	hashtable_t *getdata = NULL;
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
		char *str = (char*)(vector_at(queue, i));

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
				fflush(socket_stream);
				vector_free(queue);
				vector_free(headers);
				goto disconnect;
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

	unsigned int keepalive = 0;
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
		vector_free(queue);
		vector_free(headers);
		goto disconnect;
	}

	if (!filename || strstr(filename, "'") || strstr(filename, " ") || (querystring && strstr(querystring, " "))) {
		raw_response(socket_stream, headers, "400 Bad Request");
		fflush(socket_stream);
		vector_free(queue);
		vector_free(headers);
		goto disconnect;
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

		postdata = alloc_hashtable(HASHTABLE_DATA_SIZE);
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
	req.method = request_type;
	while (nsz-- > 0) {
		if (route[nsz].require_quid) {
			char squid[QUID_LENGTH + 1];
			if (fsz < QUID_LENGTH + 1)
				continue;

			char *_pfilename = _filename + QUID_LENGTH + 1;
			strlcpy(squid, _filename + 1, QUID_LENGTH + 1);
			if (_pfilename[0] != '/') {
				_pfilename = _filename + QUID_LENGTH - 1;
				squid[QUID_LENGTH - 2] = '\0';
			}
			if (_pfilename[0] != '/')
				continue;

			if (_pfilename) {
				if (!strcmp(route[nsz].uri, _pfilename)) {
					if (!postdata) {
						postdata = alloc_hashtable(HASHTABLE_DATA_SIZE);
						req.data = postdata;
					}
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
	if (postdata) {
		free_hashtable(postdata);
	}

	if (keepalive)
		return;

disconnect:
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
	lprint("[info] Start database core\n");
	lprintf("[info] " PROGNAME " %s ("__DATE__", "__TIME__")\n", get_version_string());
	lprintf("[info] Current time: %lld\n", get_timestamp());
	lprintf("[info] Storage core %s\n", get_zero_key());
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
