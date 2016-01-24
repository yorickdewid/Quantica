#include <stdio.h>
#include <string.h>

#include "marshall.h"
#include "dict_marshall.h"
#include "base64.h"
#include "hmac.h"
#include "core.h"
#include "time.h"
#include "zmalloc.h"
#include "quid.h"
#include "jwt.h"

static marshall_t *jwt_header() {
	marshall_t *marshall = (marshall_t *)tree_zcalloc(1, sizeof(marshall_t), NULL);
	marshall->child = (marshall_t **)tree_zcalloc(2, sizeof(marshall_t *), marshall);
	marshall->type = MTYPE_OBJECT;
	marshall->size = 2;

	marshall->child[0] = tree_zcalloc(1, sizeof(marshall_t), marshall);
	marshall->child[0]->type = MTYPE_STRING;
	marshall->child[0]->name = tree_zstrdup("alg", marshall);
	marshall->child[0]->name_len = 3;
	marshall->child[0]->data = tree_zstrdup("HS256", marshall);
	marshall->child[0]->data_len = 5;
	marshall->child[0]->size = 1;

	marshall->child[1] = tree_zcalloc(1, sizeof(marshall_t), marshall);
	marshall->child[1]->type = MTYPE_STRING;
	marshall->child[1]->name = tree_zstrdup("typ", marshall);
	marshall->child[1]->name_len = 3;
	marshall->child[1]->data = tree_zstrdup("JWT", marshall);
	marshall->child[1]->data_len = 3;
	marshall->child[1]->size = 1;

	return marshall;
}

char *jwt_encode(marshall_t *data, const unsigned char *key) {
	quid_t jti_key;
	quid_create(&jti_key);
	char squid[QUID_LENGTH + 1];
	quidtostr(squid, &jti_key);

	/* JWT header */
	marshall_t *header = jwt_header();

	marshall_t *body = (marshall_t *)tree_zcalloc(1, sizeof(marshall_t), NULL);
	body->child = (marshall_t **)tree_zcalloc(7 + data->size, sizeof(marshall_t *), body);
	body->type = MTYPE_OBJECT;

	body->child[body->size] = tree_zcalloc(1, sizeof(marshall_t), body);
	body->child[body->size]->type = MTYPE_STRING;
	body->child[body->size]->name = tree_zstrdup("iss", body);
	body->child[body->size]->name_len = 3;
	body->child[body->size]->data = tree_zstrdup(get_instance_name(), body);
	body->child[body->size]->data_len = strlen(get_instance_name());
	body->child[body->size]->size = 1;
	body->size++;

	char *issue_time = itoa(get_timestamp());
	body->child[body->size] = tree_zcalloc(1, sizeof(marshall_t), body);
	body->child[body->size]->type = MTYPE_INT;
	body->child[body->size]->name = tree_zstrdup("iat", body);
	body->child[body->size]->name_len = 3;
	body->child[body->size]->data = tree_zstrdup(issue_time, body);
	body->child[body->size]->data_len = strlen(issue_time);
	body->child[body->size]->size = 1;
	body->size++;

	body->child[body->size] = tree_zcalloc(1, sizeof(marshall_t), body);
	body->child[body->size]->type = MTYPE_INT;
	body->child[body->size]->name = tree_zstrdup("nbf", body);
	body->child[body->size]->name_len = 3;
	body->child[body->size]->data = tree_zstrdup(issue_time, body);
	body->child[body->size]->data_len = strlen(issue_time);
	body->child[body->size]->size = 1;
	body->size++;

	char *expiration_time = itoa(get_timestamp() + JWT_TOKEN_VALID);
	body->child[body->size] = tree_zcalloc(1, sizeof(marshall_t), body);
	body->child[body->size]->type = MTYPE_INT;
	body->child[body->size]->name = tree_zstrdup("exp", body);
	body->child[body->size]->name_len = 3;
	body->child[body->size]->data = tree_zstrdup(expiration_time, body);
	body->child[body->size]->data_len = strlen(expiration_time);
	body->child[body->size]->size = 1;
	body->size++;

	body->child[body->size] = tree_zcalloc(1, sizeof(marshall_t), body);
	body->child[body->size]->type = MTYPE_STRING;
	body->child[body->size]->name = tree_zstrdup("aud", body);
	body->child[body->size]->name_len = 3;
	body->child[body->size]->data = tree_zstrdup("CLUSTER", body);
	body->child[body->size]->data_len = 8;
	body->child[body->size]->size = 1;
	body->size++;

	body->child[body->size] = tree_zcalloc(1, sizeof(marshall_t), body);
	body->child[body->size]->type = MTYPE_STRING;
	body->child[body->size]->name = tree_zstrdup("sub", body);
	body->child[body->size]->name_len = 3;
	body->child[body->size]->data = tree_zstrdup("principal@CLUSTER", body);
	body->child[body->size]->data_len = 17;
	body->child[body->size]->size = 1;
	body->size++;

	body->child[body->size] = tree_zcalloc(1, sizeof(marshall_t), body);
	body->child[body->size]->type = MTYPE_QUID;
	body->child[body->size]->name = tree_zstrdup("jti", body);
	body->child[body->size]->name_len = 3;
	body->child[body->size]->data = tree_zstrdup(squid, body);
	body->child[body->size]->data_len = QUID_LENGTH;
	body->child[body->size]->size = 1;
	body->size++;

	marshall_t *payload = marshall_merge(data, body);

	char *buf_header = marshall_serialize(header);
	char *buf_payload = marshall_serialize(payload);
	marshall_free(payload);
	marshall_free(header);
	marshall_free(data);

	/* Encode header */
	size_t enheadsz = base64_encode_len(strlen(buf_header));
	char *enhead = (char *)zmalloc(enheadsz + 1);
	base64_encode(enhead, buf_header, strlen(buf_header));
	enhead[enheadsz] = '\0';
	zfree(buf_header);

	base64url_encode(enhead, enheadsz);

	/* Encode payload */
	size_t enpayldsz = base64_encode_len(strlen(buf_payload));
	char *enpayld = (char *)zmalloc(enpayldsz + 1);
	base64_encode(enpayld, buf_payload, strlen(buf_payload));
	enpayld[enpayldsz] = '\0';
	zfree(buf_payload);

	base64url_encode(enpayld, enpayldsz);

	/* Concatenate parts */
	char *unsigned_parts = (char *)zcalloc(enheadsz + enpayldsz, sizeof(char));
	strncpy(unsigned_parts, enhead, enheadsz);
	strcat(unsigned_parts, ".");
	strcat(unsigned_parts, enpayld);

	zfree(enhead);
	zfree(enpayld);

	/* Sign parts */
	unsigned char mac[SHA256_DIGEST_SIZE];
	hmac_sha256(key, strlen((const char *)key), (unsigned char *)unsigned_parts, strlen(unsigned_parts), mac, SHA256_DIGEST_SIZE);

	size_t ensigsz = base64_encode_len(SHA256_DIGEST_SIZE);
	char *ensig = (char *)zmalloc(ensigsz + 1);
	base64_encode(ensig, (const char*)mac, SHA256_DIGEST_SIZE);
	ensig[ensigsz] = '\0';

	base64url_encode(ensig, ensigsz);

	/* Append signature */
	unsigned_parts = zrealloc(unsigned_parts, enheadsz + enpayldsz + ensigsz);
	strcat(unsigned_parts, ".");
	strcat(unsigned_parts, ensig);

	zfree(ensig);
	return unsigned_parts;
}

marshall_t *jwt_decode(char *token, const unsigned char *key) {
	int dots = strccnt(token, '.');
	if (dots != 2) {
		//TODO throw invalid
		return NULL;
	}

	char *header = strtok(token, ".");
	char *payload = strtok(NULL, ".");
	char *signature = strtok(NULL, ".");

	char *signed_parts = (char *)zcalloc(strlen(header) + strlen(payload) + 2, sizeof(char));
	strncpy(signed_parts, header, strlen(header));
	strcat(signed_parts, ".");
	strcat(signed_parts, payload);

	/* Sign parts */
	unsigned char mac[SHA256_DIGEST_SIZE];
	hmac_sha256(key, strlen((const char *)key), (unsigned char *)signed_parts, strlen(signed_parts), mac, SHA256_DIGEST_SIZE);

	size_t ensigsz = base64_encode_len(SHA256_DIGEST_SIZE);
	char *ensig = (char *)zmalloc(ensigsz + 1);
	base64_encode(ensig, (const char*)mac, SHA256_DIGEST_SIZE);
	ensig[ensigsz] = '\0';

	base64url_encode(ensig, ensigsz);
	if (strcmp(ensig, signature)) {
		//TODO throw invalid
		return NULL;
	}

	base64url_decode(payload, strlen(payload));
	size_t enpayldsz = base64_decode_len(payload);
	char *enpayld = (char *)zmalloc(enpayldsz + 1);
	base64_decode(enpayld, payload);

	puts(enpayld);
	marshall_t *data = marshall_convert(enpayld, enpayldsz);
	for (unsigned int i = 0; i < data->size; ++i) {
		if (!strcmp(data->child[i]->name, "exp")) {
			long long expiration_time = atoll((char *)data->child[i]->data);
			if (get_timestamp() > expiration_time) {
				//TODO throw expired
				return NULL;
			}
		}
	}

	return data;
}
