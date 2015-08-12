#ifndef HMAC_H_INCLUDED
#define HMAC_H_INCLUDED

#include "sha2.h"

typedef struct {
	sha256_ctx ctx_inside;
	sha256_ctx ctx_outside;

	/* for hmac_reinit */
	sha256_ctx ctx_inside_reinit;
	sha256_ctx ctx_outside_reinit;

	unsigned char block_ipad[SHA256_BLOCK_SIZE];
	unsigned char block_opad[SHA256_BLOCK_SIZE];
} hmac_sha256_ctx;

void hmac_sha256_init(hmac_sha256_ctx *ctx, const unsigned char *key, unsigned int key_size);
void hmac_sha256_reinit(hmac_sha256_ctx *ctx);
void hmac_sha256_update(hmac_sha256_ctx *ctx, const unsigned char *message, unsigned int message_len);
void hmac_sha256_final(hmac_sha256_ctx *ctx, unsigned char *mac, unsigned int mac_size);

#endif // HMAC_H_INCLUDED
