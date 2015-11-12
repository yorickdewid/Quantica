#include <stdio.h>

#include <config.h>
#include "sha1.h"

#define SHA1SHIFT(bits,word) \
	((((word) << (bits)) & 0xFFFFFFFF) | \
	((word) >> (32-(bits))))

void sha1_process_message_block(struct sha *ctx);
void sha1_padmessage(struct sha *ctx);

void sha1_reset(struct sha *ctx) {
	ctx->szlow = 0;
	ctx->szhigh = 0;
	ctx->message_block_idx = 0;
	ctx->digest[0] = 0x67452301;
	ctx->digest[1] = 0xEFCDAB89;
	ctx->digest[2] = 0x98BADCFE;
	ctx->digest[3] = 0x10325476;
	ctx->digest[4] = 0xC3D2E1F0;
	ctx->computed = 0;
	ctx->corrupted = 0;
}

int sha1_result(struct sha *ctx) {
	if (ctx->corrupted)
		return 0;

	if (!ctx->computed) {
		sha1_padmessage(ctx);
		ctx->computed = 1;
	}

	return 1;
}

void sha1_input(struct sha *ctx, const unsigned char *message_array, unsigned int length) {
	if (!length)
		return;

	if (ctx->computed || ctx->corrupted) {
		ctx->corrupted = 1;
		return;
	}

	while (length-- && !ctx->corrupted) {
		ctx->message_block[ctx->message_block_idx++] = (*message_array & 0xFF);

		ctx->szlow += 8;
		ctx->szlow &= 0xFFFFFFFF;
		if (ctx->szlow == 0) {
			ctx->szhigh++;
			ctx->szhigh &= 0xFFFFFFFF;
			if (ctx->szhigh == 0)
				ctx->corrupted = 1;
		}

		if (ctx->message_block_idx == 64)
			sha1_process_message_block(ctx);

		message_array++;
	}
}

void sha1_process_message_block(struct sha *ctx) {
	const unsigned k[] = {
		0x5A827999,
		0x6ED9EBA1,
		0x8F1BBCDC,
		0xCA62C1D6
	};

	int t;
	unsigned int temp;
	unsigned int w[80];
	unsigned int a, b, c, d, e;

	for (t = 0; t < 16; ++t) {
		w[t] = ((unsigned) ctx->message_block[t * 4]) << 24;
		w[t] |= ((unsigned) ctx->message_block[t * 4 + 1]) << 16;
		w[t] |= ((unsigned) ctx->message_block[t * 4 + 2]) << 8;
		w[t] |= ((unsigned) ctx->message_block[t * 4 + 3]);
	}

	for (t = 16; t < 80; ++t) {
		w[t] = SHA1SHIFT(1, w[t - 3] ^ w[t - 8] ^ w[t - 14] ^ w[t - 16]);
	}

	a = ctx->digest[0];
	b = ctx->digest[1];
	c = ctx->digest[2];
	d = ctx->digest[3];
	e = ctx->digest[4];

	for (t = 0; t < 20; ++t) {
		temp = SHA1SHIFT(5, a) + ((b & c) | ((~b) & d)) + e + w[t] + k[0];
		temp &= 0xFFFFFFFF;
		e = d;
		d = c;
		c = SHA1SHIFT(30, b);
		b = a;
		a = temp;
	}

	for (t = 20; t < 40; ++t) {
		temp = SHA1SHIFT(5, a) + (b ^ c ^ d) + e + w[t] + k[1];
		temp &= 0xFFFFFFFF;
		e = d;
		d = c;
		c = SHA1SHIFT(30, b);
		b = a;
		a = temp;
	}

	for (t = 40; t < 60; ++t) {
		temp = SHA1SHIFT(5, a) + ((b & c) | (b & d) | (c & d)) + e + w[t] + k[2];
		temp &= 0xFFFFFFFF;
		e = d;
		d = c;
		c = SHA1SHIFT(30, b);
		b = a;
		a = temp;
	}

	for (t = 60; t < 80; ++t) {
		temp = SHA1SHIFT(5, a) + (b ^ c ^ d) + e + w[t] + k[3];
		temp &= 0xFFFFFFFF;
		e = d;
		d = c;
		c = SHA1SHIFT(30, b);
		b = a;
		a = temp;
	}

	ctx->digest[0] = (ctx->digest[0] + a) & 0xFFFFFFFF;
	ctx->digest[1] = (ctx->digest[1] + b) & 0xFFFFFFFF;
	ctx->digest[2] = (ctx->digest[2] + c) & 0xFFFFFFFF;
	ctx->digest[3] = (ctx->digest[3] + d) & 0xFFFFFFFF;
	ctx->digest[4] = (ctx->digest[4] + e) & 0xFFFFFFFF;

	ctx->message_block_idx = 0;
}

void sha1_padmessage(struct sha *ctx) {
	if (ctx->message_block_idx > 55) {
		ctx->message_block[ctx->message_block_idx++] = 0x80;
		while (ctx->message_block_idx < 64) {
			ctx->message_block[ctx->message_block_idx++] = 0;
		}

		sha1_process_message_block(ctx);

		while (ctx->message_block_idx < 56) {
			ctx->message_block[ctx->message_block_idx++] = 0;
		}
	} else {
		ctx->message_block[ctx->message_block_idx++] = 0x80;
		while (ctx->message_block_idx < 56) {
			ctx->message_block[ctx->message_block_idx++] = 0;
		}
	}

	ctx->message_block[56] = (ctx->szhigh >> 24) & 0xFF;
	ctx->message_block[57] = (ctx->szhigh >> 16) & 0xFF;
	ctx->message_block[58] = (ctx->szhigh >> 8) & 0xFF;
	ctx->message_block[59] = (ctx->szhigh) & 0xFF;
	ctx->message_block[60] = (ctx->szlow >> 24) & 0xFF;
	ctx->message_block[61] = (ctx->szlow >> 16) & 0xFF;
	ctx->message_block[62] = (ctx->szlow >> 8) & 0xFF;
	ctx->message_block[63] = (ctx->szlow) & 0xFF;

	sha1_process_message_block(ctx);
}

void sha1_strsum(char *s, struct sha *ctx) {
	snprintf(s, SHA1_LENGTH + 1, "%.8x%.8x%.8x%.8x%.8x"
	         , ctx->digest[0]
	         , ctx->digest[1]
	         , ctx->digest[2]
	         , ctx->digest[3]
	         , ctx->digest[4]);
}
