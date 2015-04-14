#include <string.h>

#include "test.h"
#include "../src/md5.h"

static void md5_1(){
	unsigned char digest[16];
	const char input[] = "ABC@123";
	const char expected[] = "28c15c0b405c1f7a107133edf5504367";
    char output[MD5_SIZE+1];
	md5_ctx md5;
	md5_init(&md5);
	md5_update(&md5, input, strlen(input));
	md5_final(digest, &md5);
	md5_strsum(output, digest);
	output[MD5_SIZE] = '\0';
	ASSERT(!strcmp(expected, output));
}

static void md5_2(){
	unsigned char digest[16];
	const char input[] = "1f7a107133edf5504";
	const char expected[] = "22340cdd11ef8447fe076ef5ac65cf91";
	char output[MD5_SIZE+1];
	md5_ctx md5;
	md5_init(&md5);
	md5_update(&md5, input, strlen(input));
	md5_final(digest, &md5);
	md5_strsum(output, digest);
	output[MD5_SIZE] = '\0';
	ASSERT(!strcmp(expected, output));
}

static void md5_3(){
	unsigned char digest[16];
	const char input[] = "hqaP5yKHGOJjDsoZaMNEQcO7y7kfwRA2JlbujPFaSiZz5QPaqbKV44rvVA37";
	const char expected[] = "0ffb94a1468813d097c28bab1127b3d5";
	char output[MD5_SIZE+1];
	md5_ctx md5;
	md5_init(&md5);
	md5_update(&md5, input, strlen(input));
	md5_final(digest, &md5);
	md5_strsum(output, digest);
	output[MD5_SIZE] = '\0';
	ASSERT(!strcmp(expected, output));
}

TEST_IMPL(md5) {
	/* Run testcase */
	md5_1();
	md5_2();
	md5_3();

	RETURN_OK();
}
