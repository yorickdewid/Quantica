#include <string.h>

#include "test.h"
#include "../src/sha2.h"

void test(const char *vector, unsigned char *digest, unsigned int digest_size) {
	char output[2 * SHA512_DIGEST_SIZE + 1];
	int i;

	output[2 * digest_size] = '\0';
	for (i=0; i<(int)digest_size; ++i)
	   sprintf(output + 2 * i, "%02x", digest[i]);

	if (strcmp(vector, output)) {
		fprintf(stderr, "Test failed.\n");
		exit(EXIT_FAILURE);
	}
}

TEST_IMPL(sha2) {

	TESTCASE("sha2");

	/* Run testcase */

	static const char *vectors[4][3] =
	{   /* SHA-224 */
		{
		"23097d223405d8228642a477bda255b32aadbce4bda0b3f7e36c9da7",
		"75388b16512776cc5dba5da1fd890150b0c6455cb4f58b1952522525",
		"20794655980c91d8bbb4c1ea97618a4bf03f42581948b2ee4ee7ad67",
		},
		/* SHA-256 */
		{
		"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
		"248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
		"cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0",
		},
		/* SHA-384 */
		{
		"cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed"
		"8086072ba1e7cc2358baeca134c825a7",
		"09330c33f71147e83d192fc782cd1b4753111b173b3b05d22fa08086e3b0f712"
		"fcc7c71a557e2db966c3e9fa91746039",
		"9d0e1809716474cb086e834e310a4a1ced149e9c00f248527972cec5704c2a5b"
		"07b8b3dc38ecc4ebae97ddd87f3d8985",
		},
		/* SHA-512 */
		{
		"ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a"
		"2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f",
		"8e959b75dae313da8cf4f72814fc143f8f7779c6eb9f7fa17299aeadb6889018"
		"501d289e4900f7e4331b99dec4b5433ac7d329eeb6dd26545e96e55b874be909",
		"e718483d0ce769644e2e42c7bc15b4638e1f98b13b2044285632a803afa973eb"
		"de0ff244877ea60a4cb0432ce577c31beb009c5c2c49aa2e4eadb217ad8cc09b"
		}
	};

	static const char message1[] = "abc";
	static const char message2a[] = "abcdbcdecdefdefgefghfghighijhi"
									"jkijkljklmklmnlmnomnopnopq";
	static const char message2b[] = "abcdefghbcdefghicdefghijdefghijkefghij"
									"klfghijklmghijklmnhijklmnoijklmnopjklm"
									"nopqklmnopqrlmnopqrsmnopqrstnopqrstu";
	unsigned char *message3;
	unsigned int message3_len = 1000000;
	unsigned char digest[SHA512_DIGEST_SIZE];

	message3 = malloc(message3_len);
	if (message3 == NULL) {
		fprintf(stderr, "Can't allocate memory\n");
		return -1;
	}
	memset(message3, 'a', message3_len);

	sha224((const unsigned char *) message1, strlen(message1), digest);
	test(vectors[0][0], digest, SHA224_DIGEST_SIZE);
	sha224((const unsigned char *) message2a, strlen(message2a), digest);
	test(vectors[0][1], digest, SHA224_DIGEST_SIZE);
	sha224(message3, message3_len, digest);
	test(vectors[0][2], digest, SHA224_DIGEST_SIZE);

	sha256((const unsigned char *) message1, strlen(message1), digest);
	test(vectors[1][0], digest, SHA256_DIGEST_SIZE);
	sha256((const unsigned char *) message2a, strlen(message2a), digest);
	test(vectors[1][1], digest, SHA256_DIGEST_SIZE);
	sha256(message3, message3_len, digest);
	test(vectors[1][2], digest, SHA256_DIGEST_SIZE);

	sha384((const unsigned char *) message1, strlen(message1), digest);
	test(vectors[2][0], digest, SHA384_DIGEST_SIZE);
	sha384((const unsigned char *)message2b, strlen(message2b), digest);
	test(vectors[2][1], digest, SHA384_DIGEST_SIZE);
	sha384(message3, message3_len, digest);
	test(vectors[2][2], digest, SHA384_DIGEST_SIZE);

	sha512((const unsigned char *) message1, strlen(message1), digest);
	test(vectors[3][0], digest, SHA512_DIGEST_SIZE);
	sha512((const unsigned char *) message2b, strlen(message2b), digest);
	test(vectors[3][1], digest, SHA512_DIGEST_SIZE);
	sha512(message3, message3_len, digest);
	test(vectors[3][2], digest, SHA512_DIGEST_SIZE);

	RETURN_OK();
}
