#ifndef MD5_H_INCLUDED
#define MD5_H_INCLUDED

#include <stdint.h>

#define MD5_BLOCK_SIZE		16
#define MD5_SIZE			32

typedef struct {
	uint32_t lo, hi;
	uint32_t a, b, c, d;
	unsigned char buffer[64];
	uint32_t block[MD5_BLOCK_SIZE];
} md5_ctx;

void md5_init(md5_ctx *ctx);
void md5_update(md5_ctx *ctx, const void *data, unsigned long size);
void md5_final(unsigned char *result, md5_ctx *ctx);
void md5_strsum(char *s, unsigned char *digest);

#endif // MD5_H_INCLUDED
