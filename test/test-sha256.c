#include <string.h>

#include "test.h"
#include "../src/sha256.h"

static void sha256_1(){
	unsigned char digest[SHA256_BLOCK_SIZE];
    const char input[] = "ABC@123";
    const char expected[] = "33f9a66e61b1e5e76cd324dfdacb7c973adcc45b410ddb4c6a21b41c20699c36";
    char output[SHA256_SIZE+1];
	sha256_ctx ctx;
	sha256_init(&ctx);
	sha256_update(&ctx, (const unsigned char *)input, strlen(input));
	sha256_final(&ctx, digest);
	sha256_strsum(output, digest);
    output[SHA256_SIZE] = '\0';
	ASSERT(!strcmp(expected, output));
}

static void sha256_2(){
	unsigned char digest[SHA256_BLOCK_SIZE];
    const char input[] = "0H7QXZiydOZqL3OSkw11sT7541OVlnd8Lo0JgrEM9RIqSgUbwPyTNe4PIy0q";
    const char expected[] = "0329fbbe5838752949c041e5215742c4c377d6f218c31b18c6432eabc72a7665";
    char output[SHA256_SIZE+1];
	sha256_ctx ctx;
	sha256_init(&ctx);
	sha256_update(&ctx, (const unsigned char *)input, strlen(input));
	sha256_final(&ctx, digest);
	sha256_strsum(output, digest);
    output[SHA256_SIZE] = '\0';
	ASSERT(!strcmp(expected, output));
}

static void sha256_3(){
	unsigned char digest[SHA256_BLOCK_SIZE];
    const char input[] = "EG4Ie9vppDjvkheAGch69cfd4Cptgs2e0aAzuie67drfi6f6RFI67WFR67IRFI67F6ifi68f7AmBQkVdy6e7eb791b8aad23c2a729139bf6ee842mmToOtquB6EIGG6iwgdi8f5";
    const char expected[] = "3dfdf014e4647cb11368d3f30fe2f0064c3124f226a7de874d900cdc269950af";
    char output[SHA256_SIZE+1];
	sha256_ctx ctx;
	sha256_init(&ctx);
	sha256_update(&ctx, (const unsigned char *)input, strlen(input));
	sha256_final(&ctx, digest);
	sha256_strsum(output, digest);
    output[SHA256_SIZE] = '\0';
	ASSERT(!strcmp(expected, output));
}

TEST_IMPL(sha256) {
	/* Run testcase */
	sha256_1();
	sha256_2();
	sha256_3();

	RETURN_OK();
}
