#ifndef SHA512_H_INCLUDED
#define SHA512_H_INCLUDED

#include <stdint.h>
#include <stdio.h>

#define SHA512_BLOCK_SIZE	128
#define SHA512_SIZE			64

typedef struct {
	uint64_t length;
	uint64_t state[8];
	uint32_t curlen;
	uint8_t buf[128];
} sha512_ctx;

typedef struct {
	uint8_t bytes[SHA512_SIZE];
} SHA512_HASH;

void sha512_init(sha512_ctx *ctx);
void sha512_update(sha512_ctx *ctx, void *buffer, uint32_t buffer_size);
void sha512_final(sha512_ctx *ctx, SHA512_HASH *digest);

#endif // SHA256_H_INCLUDED
