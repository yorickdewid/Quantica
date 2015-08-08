#include <stdio.h>
#include <string.h>

#include "sha512.h"

#define ROR64(value, bits) (((value) >> (bits)) | ((value) << (64 - (bits))))
#define MIN(x, y) ( ((x)<(y))?(x):(y) )
#define LOAD64H(x, y) { 													\
	x = (((uint64_t)((y)[0] & 255))<<56)|(((uint64_t)((y)[1] & 255))<<48) |	\
	(((uint64_t)((y)[2] & 255))<<40)|(((uint64_t)((y)[3] & 255))<<32) |		\
	(((uint64_t)((y)[4] & 255))<<24)|(((uint64_t)((y)[5] & 255))<<16) |		\
	(((uint64_t)((y)[6] & 255))<<8)|(((uint64_t)((y)[7] & 255)));			\
}

#define STORE64H(x, y) {													\
	(y)[0] = (uint8_t)(((x)>>56)&255); (y)[1] = (uint8_t)(((x)>>48)&255);	\
	(y)[2] = (uint8_t)(((x)>>40)&255); (y)[3] = (uint8_t)(((x)>>32)&255);	\
	(y)[4] = (uint8_t)(((x)>>24)&255); (y)[5] = (uint8_t)(((x)>>16)&255);	\
	(y)[6] = (uint8_t)(((x)>>8)&255); (y)[7] = (uint8_t)((x)&255);			\
}

#define ch(x, y, z)     (z ^ (x & (y ^ z)))
#define maj(x, y, z)    (((x | y) & z) | (x & y))
#define s(x, n)         ROR64(x, n)
#define r(x, n)         (((x)&0xFFFFFFFFFFFFFFFFULL)>>((uint64_t)n))
#define sigma0(x)       (s(x, 28) ^ s(x, 34) ^ s(x, 39))
#define sigma1(x)       (s(x, 14) ^ s(x, 18) ^ s(x, 41))
#define gamma0(x)       (s(x, 1) ^ s(x, 8) ^ r(x, 7))
#define gamma1(x)       (s(x, 19) ^ s(x, 61) ^ r(x, 6))

#define sha512_round(a, b, c, d, e, f, g, h, i)			\
	t0 = h + sigma1(e) + ch(e, f, g) + k[i] + w[i];		\
	t1 = sigma0(a) + maj(a, b, c);						\
	d += t0;											\
	h = t0 + t1;

static const uint64_t k[80] = {
	0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
	0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
	0xd807aa98a3030242ULL, 0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
	0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
	0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL, 0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
	0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
	0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL,
	0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL, 0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
	0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
	0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
	0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL, 0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL,
	0xd192e819d6ef5218ULL, 0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
	0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL,
	0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL, 0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL,
	0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
	0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
	0xca273eceea26619cULL, 0xd186b8c721c0c207ULL, 0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL,
	0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
	0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL,
	0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL, 0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL
};

static void sha512_transform(sha512_ctx *ctx, uint8_t *buffer) {
	uint64_t s[8];
	uint64_t w[80];
	uint64_t t0;
	uint64_t t1;
	int i;

	for (i=0; i<8; ++i)
		s[i] = ctx->state[i];

	for (i=0; i<16; ++i)
		LOAD64H(w[i], buffer + (8*i));

	for (i=16; i<80; ++i)
		w[i] = gamma1(w[i - 2]) + w[i - 7] + gamma0(w[i - 15]) + w[i - 16];

	for (i=0; i<80; i+=8) {
		 sha512_round(s[0],s[1],s[2],s[3],s[4],s[5],s[6],s[7],i+0);
		 sha512_round(s[7],s[0],s[1],s[2],s[3],s[4],s[5],s[6],i+1);
		 sha512_round(s[6],s[7],s[0],s[1],s[2],s[3],s[4],s[5],i+2);
		 sha512_round(s[5],s[6],s[7],s[0],s[1],s[2],s[3],s[4],i+3);
		 sha512_round(s[4],s[5],s[6],s[7],s[0],s[1],s[2],s[3],i+4);
		 sha512_round(s[3],s[4],s[5],s[6],s[7],s[0],s[1],s[2],i+5);
		 sha512_round(s[2],s[3],s[4],s[5],s[6],s[7],s[0],s[1],i+6);
		 sha512_round(s[1],s[2],s[3],s[4],s[5],s[6],s[7],s[0],i+7);
	 }

	for (i=0; i<8; ++i)
		ctx->state[i] = ctx->state[i] + s[i];
}

void sha512_init(sha512_ctx *ctx) {
	ctx->curlen = 0;
	ctx->length = 0;
	ctx->state[0] = 0x6a09e667f3bcc908ULL;
	ctx->state[1] = 0xbb67ae8584caa73bULL;
	ctx->state[2] = 0x3c6ef372fe94f82bULL;
	ctx->state[3] = 0xa54ff53a5f1d36f1ULL;
	ctx->state[4] = 0x510e527fade682d1ULL;
	ctx->state[5] = 0x9b05688c2b3e6c1fULL;
	ctx->state[6] = 0x1f83d9abfb41bd6bULL;
	ctx->state[7] = 0x5be0cd19137e2179ULL;
}

void sha512_update(sha512_ctx *ctx, void *buffer, uint32_t buffer_size) {
	uint32_t n;

	if (ctx->curlen > sizeof(ctx->buf))
	   return;

	while (buffer_size > 0) {
		if (ctx->curlen == 0 && buffer_size >= SHA512_BLOCK_SIZE) {
		   sha512_transform( ctx, (uint8_t *)buffer );
		   ctx->length += SHA512_BLOCK_SIZE * 8;
		   buffer = (uint8_t*)buffer + SHA512_BLOCK_SIZE;
		   buffer_size -= SHA512_BLOCK_SIZE;
		} else {
		   n = MIN( buffer_size, (SHA512_BLOCK_SIZE - ctx->curlen) );
		   memcpy( ctx->buf + ctx->curlen, buffer, (size_t)n );
		   ctx->curlen += n;
		   buffer = (uint8_t*)buffer + n;
		   buffer_size -= n;
		   if (ctx->curlen == SHA512_BLOCK_SIZE) {
			  sha512_transform (ctx, ctx->buf);
			  ctx->length += 8*SHA512_BLOCK_SIZE;
			  ctx->curlen = 0;
		   }
	   }
	}
}

void sha512_final(sha512_ctx *ctx, SHA512_HASH *Digest) {
	int i;

	if (ctx->curlen >= sizeof(ctx->buf))
	   return;

	ctx->length += ctx->curlen * 8ULL;
	ctx->buf[ctx->curlen++] = (uint8_t)0x80;

	if (ctx->curlen > 112) {
		while (ctx->curlen < 128) {
			ctx->buf[ctx->curlen++] = (uint8_t)0;
		}
		sha512_transform( ctx, ctx->buf );
		ctx->curlen = 0;
	}

	while (ctx->curlen < 120)
		ctx->buf[ctx->curlen++] = (uint8_t)0;

	STORE64H(ctx->length, ctx->buf+120 );
	sha512_transform(ctx, ctx->buf );

	for (i=0; i<8; ++i) 
		STORE64H( ctx->state[i], Digest->bytes+(8*i) );
}

