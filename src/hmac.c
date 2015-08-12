#include <string.h>

#include "hmac.h"

void hmac_sha256_init(hmac_sha256_ctx *ctx, const unsigned char *key, unsigned int key_size) {
	unsigned int fill;
	unsigned int num;

	sha256_ctx _ctx;
	const unsigned char *key_used;
	unsigned char key_temp[SHA256_SIZE];
	int i;

	if (key_size == SHA256_BLOCK_SIZE) {
		key_used = key;
		num = SHA256_BLOCK_SIZE;
	} else {
		if (key_size > SHA256_BLOCK_SIZE){
			num = SHA256_SIZE;
			sha256_init(&_ctx);
			sha256_update(&_ctx, key, key_size);
			sha256_final(&_ctx, key_temp);
			key_used = key_temp;
		} else { /* key_size > SHA256_BLOCK_SIZE */
			key_used = key;
			num = key_size;
		}
		fill = SHA256_BLOCK_SIZE - num;

		memset(ctx->block_ipad + num, 0x36, fill);
		memset(ctx->block_opad + num, 0x5c, fill);
	}

	for (i=0; i<(int)num; ++i) {
		ctx->block_ipad[i] = key_used[i] ^ 0x36;
		ctx->block_opad[i] = key_used[i] ^ 0x5c;
	}

	sha256_init(&ctx->ctx_inside);
	sha256_update(&ctx->ctx_inside, ctx->block_ipad, SHA256_BLOCK_SIZE);

	sha256_init(&ctx->ctx_outside);
	sha256_update(&ctx->ctx_outside, ctx->block_opad, SHA256_BLOCK_SIZE);

	/* for hmac_reinit */
	memcpy(&ctx->ctx_inside_reinit, &ctx->ctx_inside, sizeof(sha256_ctx));
	memcpy(&ctx->ctx_outside_reinit, &ctx->ctx_outside, sizeof(sha256_ctx));
}

void hmac_sha256_reinit(hmac_sha256_ctx *ctx) {
	memcpy(&ctx->ctx_inside, &ctx->ctx_inside_reinit, sizeof(sha256_ctx));
	memcpy(&ctx->ctx_outside, &ctx->ctx_outside_reinit, sizeof(sha256_ctx));
}

void hmac_sha256_update(hmac_sha256_ctx *ctx, const unsigned char *message, unsigned int message_len) {
	sha256_update(&ctx->ctx_inside, message, message_len);
}

void hmac_sha256_final(hmac_sha256_ctx *ctx, unsigned char *mac, unsigned int mac_size) {
	unsigned char digest_inside[SHA256_SIZE];
	unsigned char mac_temp[SHA256_SIZE];

	sha256_final(&ctx->ctx_inside, digest_inside);
	sha256_update(&ctx->ctx_outside, digest_inside, SHA256_SIZE);
	sha256_final(&ctx->ctx_outside, mac_temp);
	memcpy(mac, mac_temp, mac_size);
}

#if 0
void hmac_sha256(const unsigned char *key, unsigned int key_size, const unsigned char *message, unsigned int message_len, unsigned char *mac, unsigned mac_size) {
	hmac_sha256_ctx ctx;

	hmac_sha256_init(&ctx, key, key_size);
	hmac_sha256_update(&ctx, message, message_len);
	hmac_sha256_final(&ctx, mac, mac_size);
}
#endif
