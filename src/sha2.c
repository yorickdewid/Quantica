#include <stdio.h>
#include <string.h>

#include <config.h>
#include "sha2.h"

#define SHFR(x, n)    (x >> n)
#define ROTR(x, n)   ((x >> n) | (x << ((sizeof(x) << 3) - n)))
#define ROTL(x, n)   ((x << n) | (x >> ((sizeof(x) << 3) - n)))
#define CH(x, y, z)  ((x & y) ^ (~x & z))
#define MAJ(x, y, z) ((x & y) ^ (x & z) ^ (y & z))

#define SHA256_F1(x) (ROTR(x,  2) ^ ROTR(x, 13) ^ ROTR(x, 22))
#define SHA256_F2(x) (ROTR(x,  6) ^ ROTR(x, 11) ^ ROTR(x, 25))
#define SHA256_F3(x) (ROTR(x,  7) ^ ROTR(x, 18) ^ SHFR(x,  3))
#define SHA256_F4(x) (ROTR(x, 17) ^ ROTR(x, 19) ^ SHFR(x, 10))

#define SHA512_F1(x) (ROTR(x, 28) ^ ROTR(x, 34) ^ ROTR(x, 39))
#define SHA512_F2(x) (ROTR(x, 14) ^ ROTR(x, 18) ^ ROTR(x, 41))
#define SHA512_F3(x) (ROTR(x,  1) ^ ROTR(x,  8) ^ SHFR(x,  7))
#define SHA512_F4(x) (ROTR(x, 19) ^ ROTR(x, 61) ^ SHFR(x,  6))

#define UNPACK32(x, str)                      \
{                                             \
	*((str) + 3) = (uint8) ((x)      );       \
	*((str) + 2) = (uint8) ((x) >>  8);       \
	*((str) + 1) = (uint8) ((x) >> 16);       \
	*((str) + 0) = (uint8) ((x) >> 24);       \
}

#define PACK32(str, x)                        \
{                                             \
	*(x) =   ((uint32) *((str) + 3)      )    \
		   | ((uint32) *((str) + 2) <<  8)    \
		   | ((uint32) *((str) + 1) << 16)    \
		   | ((uint32) *((str) + 0) << 24);   \
}

#define UNPACK64(x, str)                      \
{                                             \
	*((str) + 7) = (uint8) ((x)      );       \
	*((str) + 6) = (uint8) ((x) >>  8);       \
	*((str) + 5) = (uint8) ((x) >> 16);       \
	*((str) + 4) = (uint8) ((x) >> 24);       \
	*((str) + 3) = (uint8) ((x) >> 32);       \
	*((str) + 2) = (uint8) ((x) >> 40);       \
	*((str) + 1) = (uint8) ((x) >> 48);       \
	*((str) + 0) = (uint8) ((x) >> 56);       \
}

#define PACK64(str, x)                        \
{                                             \
	*(x) =   ((uint64) *((str) + 7)      )    \
		   | ((uint64) *((str) + 6) <<  8)    \
		   | ((uint64) *((str) + 5) << 16)    \
		   | ((uint64) *((str) + 4) << 24)    \
		   | ((uint64) *((str) + 3) << 32)    \
		   | ((uint64) *((str) + 2) << 40)    \
		   | ((uint64) *((str) + 1) << 48)    \
		   | ((uint64) *((str) + 0) << 56);   \
}

#define SHA256_SCR(i)                         \
{                                             \
	w[i] =  SHA256_F4(w[i -  2]) + w[i -  7]  \
		  + SHA256_F3(w[i - 15]) + w[i - 16]; \
}

#define SHA512_SCR(i)                         \
{                                             \
	w[i] =  SHA512_F4(w[i -  2]) + w[i -  7]  \
		  + SHA512_F3(w[i - 15]) + w[i - 16]; \
}

#define SHA256_EXP(a, b, c, d, e, f, g, h, j)               \
{                                                           \
	t1 = wv[h] + SHA256_F2(wv[e]) + CH(wv[e], wv[f], wv[g]) \
		 + sha256_k[j] + w[j];                              \
	t2 = SHA256_F1(wv[a]) + MAJ(wv[a], wv[b], wv[c]);       \
	wv[d] += t1;                                            \
	wv[h] = t1 + t2;                                        \
}

#define SHA512_EXP(a, b, c, d, e, f, g ,h, j)               \
{                                                           \
	t1 = wv[h] + SHA512_F2(wv[e]) + CH(wv[e], wv[f], wv[g]) \
		 + sha512_k[j] + w[j];                              \
	t2 = SHA512_F1(wv[a]) + MAJ(wv[a], wv[b], wv[c]);       \
	wv[d] += t1;                                            \
	wv[h] = t1 + t2;                                        \
}

uint32 sha224_h0[8] = {
			0xc1059ed8, 0x367cd507, 0x3070dd17, 0xf70e5939,
			0xffc00b31, 0x68581511, 0x64f98fa7, 0xbefa4fa4
		};

uint32 sha256_h0[8] = {
			0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
			0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
		};

uint64 sha384_h0[8] = {
			0xcbbb9d5dc1059ed8ULL, 0x629a292a367cd507ULL,
			0x9159015a3070dd17ULL, 0x152fecd8f70e5939ULL,
			0x67332667ffc00b31ULL, 0x8eb44a8768581511ULL,
			0xdb0c2e0d64f98fa7ULL, 0x47b5481dbefa4fa4ULL
		};

uint64 sha512_h0[8] = {
			0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL,
			0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL,
			0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL,
			0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL
		};

uint32 sha256_k[64] = {
			0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
			0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
			0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
			0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
			0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
			0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
			0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
			0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
			0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
			0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
			0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
			0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
			0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
			0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
			0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
			0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
		};

uint64 sha512_k[80] = {
			0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL,
			0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
			0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL,
			0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
			0xd807aa98a3030242ULL, 0x12835b0145706fbeULL,
			0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
			0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL,
			0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
			0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL,
			0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
			0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL,
			0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
			0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL,
			0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL,
			0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL,
			0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
			0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL,
			0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
			0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL,
			0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
			0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL,
			0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL,
			0xd192e819d6ef5218ULL, 0xd69906245565a910ULL,
			0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
			0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL,
			0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL,
			0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL,
			0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL,
			0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL,
			0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
			0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL,
			0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
			0xca273eceea26619cULL, 0xd186b8c721c0c207ULL,
			0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL,
			0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL,
			0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
			0x28db77f523047d84ULL, 0x32caab7b40c72493ULL,
			0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL,
			0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL,
			0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL
		};

static void sha256_transf(sha256_ctx *ctx, const unsigned char *message, unsigned int block_nb) {
	uint32 w[64];
	uint32 wv[8];
	uint32 t1, t2;
	const unsigned char *sub_block;

	int i, j;
	for (i = 0; i < (int) block_nb; i++) {
		sub_block = message + (i << 6);

		for (j = 0; j < 16; j++) {
			PACK32(&sub_block[j << 2], &w[j]);
		}

		for (j = 16; j < 64; j++) {
			SHA256_SCR(j);
		}

		for (j = 0; j < 8; j++) {
			wv[j] = ctx->h[j];
		}

		for (j = 0; j < 64; j++) {
			t1 = wv[7] + SHA256_F2(wv[4]) + CH(wv[4], wv[5], wv[6]) + sha256_k[j] + w[j];
			t2 = SHA256_F1(wv[0]) + MAJ(wv[0], wv[1], wv[2]);
			wv[7] = wv[6];
			wv[6] = wv[5];
			wv[5] = wv[4];
			wv[4] = wv[3] + t1;
			wv[3] = wv[2];
			wv[2] = wv[1];
			wv[1] = wv[0];
			wv[0] = t1 + t2;
		}

		for (j=0; j<8; ++j)
			ctx->h[j] += wv[j];
	}
}

void sha256_init(sha256_ctx *ctx) {
	int i;
	for (i = 0; i < 8; i++)
		ctx->h[i] = sha256_h0[i];

	ctx->len = 0;
	ctx->tot_len = 0;
}

void sha256_update(sha256_ctx *ctx, const unsigned char *message, unsigned int len) {
	unsigned int block_nb;
	unsigned int new_len, rem_len, tmp_len;
	const unsigned char *shifted_message;

	tmp_len = SHA256_BLOCK_SIZE - ctx->len;
	rem_len = len < tmp_len ? len : tmp_len;

	memcpy(&ctx->block[ctx->len], message, rem_len);

	if (ctx->len + len < SHA256_BLOCK_SIZE) {
		ctx->len += len;
		return;
	}

	new_len = len - rem_len;
	block_nb = new_len / SHA256_BLOCK_SIZE;

	shifted_message = message + rem_len;

	sha256_transf(ctx, ctx->block, 1);
	sha256_transf(ctx, shifted_message, block_nb);

	rem_len = new_len % SHA256_BLOCK_SIZE;

	memcpy(ctx->block, &shifted_message[block_nb << 6], rem_len);

	ctx->len = rem_len;
	ctx->tot_len += (block_nb + 1) << 6;
}

void sha256_final(sha256_ctx *ctx, unsigned char *digest) {
	unsigned int block_nb;
	unsigned int pm_len;
	unsigned int len_b;

	int i;
	block_nb = (1 + ((SHA256_BLOCK_SIZE - 9) < (ctx->len % SHA256_BLOCK_SIZE)));

	len_b = (ctx->tot_len + ctx->len) << 3;
	pm_len = block_nb << 6;

	memset(ctx->block + ctx->len, 0, pm_len - ctx->len);
	ctx->block[ctx->len] = 0x80;
	UNPACK32(len_b, ctx->block + pm_len - 4);

	sha256_transf(ctx, ctx->block, block_nb);

	for (i=0; i<8; ++i)
		UNPACK32(ctx->h[i], &digest[i << 2]);
}

static void sha512_transf(sha512_ctx *ctx, const unsigned char *message, unsigned int block_nb) {
	uint64 w[80];
	uint64 wv[8];
	uint64 t1, t2;
	const unsigned char *sub_block;

	int i, j;
	for (i=0; i<(int)block_nb; ++i) {
		sub_block = message + (i << 7);

		for (j = 0; j < 16; j++) {
			PACK64(&sub_block[j << 3], &w[j]);
		}

		for (j = 16; j < 80; j++) {
			SHA512_SCR(j);
		}

		for (j = 0; j < 8; j++) {
			wv[j] = ctx->h[j];
		}

		for (j = 0; j < 80; j++) {
			t1 = wv[7] + SHA512_F2(wv[4]) + CH(wv[4], wv[5], wv[6]) + sha512_k[j] + w[j];
			t2 = SHA512_F1(wv[0]) + MAJ(wv[0], wv[1], wv[2]);
			wv[7] = wv[6];
			wv[6] = wv[5];
			wv[5] = wv[4];
			wv[4] = wv[3] + t1;
			wv[3] = wv[2];
			wv[2] = wv[1];
			wv[1] = wv[0];
			wv[0] = t1 + t2;
		}

		for (j=0; j<8; ++j)
			ctx->h[j] += wv[j];
	}
}

void sha512_init(sha512_ctx *ctx) {
	int i;
	for (i=0; i<8; ++i)
		ctx->h[i] = sha512_h0[i];

	ctx->len = 0;
	ctx->tot_len = 0;
}

void sha512_update(sha512_ctx *ctx, const unsigned char *message, unsigned int len) {
	unsigned int block_nb;
	unsigned int new_len, rem_len, tmp_len;
	const unsigned char *shifted_message;

	tmp_len = SHA512_BLOCK_SIZE - ctx->len;
	rem_len = len < tmp_len ? len : tmp_len;

	memcpy(&ctx->block[ctx->len], message, rem_len);

	if (ctx->len + len < SHA512_BLOCK_SIZE) {
		ctx->len += len;
		return;
	}

	new_len = len - rem_len;
	block_nb = new_len / SHA512_BLOCK_SIZE;

	shifted_message = message + rem_len;

	sha512_transf(ctx, ctx->block, 1);
	sha512_transf(ctx, shifted_message, block_nb);

	rem_len = new_len % SHA512_BLOCK_SIZE;

	memcpy(ctx->block, &shifted_message[block_nb << 7], rem_len);

	ctx->len = rem_len;
	ctx->tot_len += (block_nb + 1) << 7;
}

void sha512_final(sha512_ctx *ctx, unsigned char *digest) {
	unsigned int block_nb;
	unsigned int pm_len;
	unsigned int len_b;

	int i;
	block_nb = 1 + ((SHA512_BLOCK_SIZE - 17) < (ctx->len % SHA512_BLOCK_SIZE));

	len_b = (ctx->tot_len + ctx->len) << 3;
	pm_len = block_nb << 7;

	memset(ctx->block + ctx->len, 0, pm_len - ctx->len);
	ctx->block[ctx->len] = 0x80;
	UNPACK32(len_b, ctx->block + pm_len - 4);

	sha512_transf(ctx, ctx->block, block_nb);

	for (i=0; i<8; ++i)
		UNPACK64(ctx->h[i], &digest[i << 3]);
}

void sha384_init(sha384_ctx *ctx) {
	int i;
	for (i=0; i<8; ++i)
		ctx->h[i] = sha384_h0[i];

	ctx->len = 0;
	ctx->tot_len = 0;
}

void sha384_update(sha384_ctx *ctx, const unsigned char *message, unsigned int len) {
	unsigned int block_nb;
	unsigned int new_len, rem_len, tmp_len;
	const unsigned char *shifted_message;

	tmp_len = SHA384_BLOCK_SIZE - ctx->len;
	rem_len = len < tmp_len ? len : tmp_len;

	memcpy(&ctx->block[ctx->len], message, rem_len);

	if (ctx->len + len < SHA384_BLOCK_SIZE) {
		ctx->len += len;
		return;
	}

	new_len = len - rem_len;
	block_nb = new_len / SHA384_BLOCK_SIZE;

	shifted_message = message + rem_len;

	sha512_transf(ctx, ctx->block, 1);
	sha512_transf(ctx, shifted_message, block_nb);

	rem_len = new_len % SHA384_BLOCK_SIZE;

	memcpy(ctx->block, &shifted_message[block_nb << 7], rem_len);

	ctx->len = rem_len;
	ctx->tot_len += (block_nb + 1) << 7;
}

void sha384_final(sha384_ctx *ctx, unsigned char *digest) {
	unsigned int block_nb;
	unsigned int pm_len;
	unsigned int len_b;

	int i;
	block_nb = (1 + ((SHA384_BLOCK_SIZE - 17) < (ctx->len % SHA384_BLOCK_SIZE)));

	len_b = (ctx->tot_len + ctx->len) << 3;
	pm_len = block_nb << 7;

	memset(ctx->block + ctx->len, 0, pm_len - ctx->len);
	ctx->block[ctx->len] = 0x80;
	UNPACK32(len_b, ctx->block + pm_len - 4);

	sha512_transf(ctx, ctx->block, block_nb);

	for (i=0; i<6; ++i)
		UNPACK64(ctx->h[i], &digest[i << 3]);
}

void sha224_init(sha224_ctx *ctx) {
	int i;
	for (i = 0; i < 8; i++) {
		ctx->h[i] = sha224_h0[i];
	}

	ctx->len = 0;
	ctx->tot_len = 0;
}

void sha224_update(sha224_ctx *ctx, const unsigned char *message, unsigned int len){
	unsigned int block_nb;
	unsigned int new_len, rem_len, tmp_len;
	const unsigned char *shifted_message;

	tmp_len = SHA224_BLOCK_SIZE - ctx->len;
	rem_len = len < tmp_len ? len : tmp_len;

	memcpy(&ctx->block[ctx->len], message, rem_len);

	if (ctx->len + len < SHA224_BLOCK_SIZE) {
		ctx->len += len;
		return;
	}

	new_len = len - rem_len;
	block_nb = new_len / SHA224_BLOCK_SIZE;

	shifted_message = message + rem_len;

	sha256_transf(ctx, ctx->block, 1);
	sha256_transf(ctx, shifted_message, block_nb);

	rem_len = new_len % SHA224_BLOCK_SIZE;

	memcpy(ctx->block, &shifted_message[block_nb << 6], rem_len);

	ctx->len = rem_len;
	ctx->tot_len += (block_nb + 1) << 6;
}

void sha224_final(sha224_ctx *ctx, unsigned char *digest) {
	unsigned int block_nb;
	unsigned int pm_len;
	unsigned int len_b;

	int i;
	block_nb = (1 + ((SHA224_BLOCK_SIZE - 9) < (ctx->len % SHA224_BLOCK_SIZE)));

	len_b = (ctx->tot_len + ctx->len) << 3;
	pm_len = block_nb << 6;

	memset(ctx->block + ctx->len, 0, pm_len - ctx->len);
	ctx->block[ctx->len] = 0x80;
	UNPACK32(len_b, ctx->block + pm_len - 4);

	sha256_transf(ctx, ctx->block, block_nb);

	for (i=0; i<7; ++i)
		UNPACK32(ctx->h[i], &digest[i << 2]);
}

void sha2_strsum(char *s, unsigned char *digest, unsigned int digest_size) {
	int i;

	for (i=0; i<(int)digest_size; ++i)
		sprintf(s + 2 * i, "%02x", digest[i]);
}
