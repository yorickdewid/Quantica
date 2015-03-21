#include <string.h>

#include "test.h"
#include "../src/aes.h"

static void aes_encrypt_ecb(){
    uint8_t input[] = "sometestval%285!";
    uint8_t key[] = "&2jUe*e7DE2o#ab%";
    uint8_t expected[] = {'\x5c', '\x8d', '\xab', '\xbc', '\x8e', '\x44', '\xd4', '\x79', '\x01', '\x27', '\xf9', '\xb9', '\xa1', '\x65', '\xcf', '\xa4'};
    uint8_t output[16];
    aes128_ecb_encrypt(input, key, output);
	ASSERT(!memcmp(output, expected, 16));
}

static void aes_decrypt_ecb(){
    uint8_t input[] = {'\xbd', '\xf7', '\x1b', '\xbc', '\xc4', '\xbb', '\xd8', '\x49', '\x3f', '\x5b', '\x52', '\x17', '\x5e', '\x3a', '\x79', '\x45'};
    uint8_t key[] = "8irI&rsI&YI&%57e";
    uint8_t expected[] = "ferfgrgFGFG6iofd";
    uint8_t output[16];
    aes128_ecb_decrypt(input, key, output);
	ASSERT(!memcmp(output, expected, 16));
}

static void aes_crypt_ecb(){
    #define ECB_INPUT "6H7Btp3Bxm4d6ILl"
    #define ECB_KEY "66h5C030jZCJomHZ"
    uint8_t input[] = ECB_INPUT;
    uint8_t key[] = ECB_KEY;
    uint8_t output[16];
    uint8_t result[16];
    aes128_ecb_encrypt(input, key, output);
    aes128_ecb_decrypt(output, key, result);
	ASSERT(!memcmp(ECB_INPUT, result, 16));
	#undef ECB_INPUT
	#undef ECB_KEY
}

static void aes_encrypt_cbc(){
    uint8_t input[] = "B5SuLr8K55xcnl2M";
    uint8_t key[] = "8oJjGXXUR5Xxq701";
    uint8_t iv[16] = {'\x28', '\x27', '\xf5', '\xcb', '\x06', '\x0a', '\xcb', '\xa7', '\x89', '\x00', '\x72', '\xbc', '\x88', '\x29', '\xfb', '\x4c'};
    uint8_t expected[] = {'\xde', '\xbe', '\x38', '\x19', '\x69', '\xc4', '\x32', '\x11', '\xde', '\xf5', '\x58', '\x64', '\x2d', '\x58', '\x73', '\x8c'};
    uint8_t output[16];
    aes128_cbc_encrypt_buffer(output, input, 16, key, iv);
	ASSERT(!memcmp(output, expected, 16));
}

static void aes_decrypt_cbc(){
    uint8_t input[] = {'\xa0', '\x1e', '\xd3', '\xfa', '\x4c', '\x6a', '\xfc', '\xfa', '\xc3', '\x31', '\x1b', '\xaf', '\x5f', '\xff', '\x7a', '\xbc'};
    uint8_t iv[16] = {'\xf9', '\x2d', '\xc6', '\x33', '\xc0', '\x52', '\x47', '\xe1', '\x86', '\xee', '\xd4', '\x7c', '\x14', '\xf1', '\xe8', '\x9a'};
    uint8_t key[] = "sTRdRqC7J6iwhUAh";
    uint8_t expected[] = "0Ni7KgCuTgNiFHcR";
    uint8_t output[16];
    aes128_cbc_decrypt_buffer(output, input, 16, key, iv);
	ASSERT(!memcmp(output, expected, 16));
}

static void aes_crypt_cbc(){
    #define CBC_INPUT "JLKenVFeG9wJLiY2"
    #define CBC_KEY "ZFNcJ9EhwbtgCM0C"
    uint8_t input[] = CBC_INPUT;
    uint8_t key[] = CBC_KEY;
    uint8_t iv[16] = {'\x30', '\x6a', '\x03', '\x77', '\x43', '\x4e', '\x84', '\x9a', '\xb6', '\xf2', '\xf2', '\xbf', '\x7b', '\xbb', '\x43', '\x4b'};
    uint8_t output[16];
    uint8_t result[16];
    aes128_cbc_encrypt_buffer(output, input, 16, key, iv);
    aes128_cbc_decrypt_buffer(result, output, 16, key, iv);
	ASSERT(!memcmp(CBC_INPUT, result, 16));
	#undef CBC_INPUT
	#undef CBC_KEY
}

TEST_IMPL(aes) {
	/* Run testcase */
	aes_encrypt_ecb();
	aes_decrypt_ecb();
	aes_crypt_ecb();
	aes_encrypt_cbc();
	aes_decrypt_cbc();
	aes_crypt_cbc();

	RETURN_OK();
}
