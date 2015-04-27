#ifndef SHA256_H_INCLUDED
#define SHA256_H_INCLUDED

#include <stddef.h>

#define SHA256_BLOCK_SIZE	32		// SHA256 outputs a 32 byte digest
#define SHA256_SIZE 		64

typedef struct {
	unsigned char data[64];
	unsigned int datalen;
	unsigned long long bitlen;
	unsigned int state[8];
} sha256_ctx;

void sha256_init(sha256_ctx *ctx);
void sha256_update(sha256_ctx *ctx, const unsigned char data[], size_t len);
void sha256_final(sha256_ctx *ctx, unsigned char hash[]);
void sha256_strsum(char *s, unsigned char *digest);

#endif // SHA256_H_INCLUDED
