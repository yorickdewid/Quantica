#include <string.h>

#include "test.h"
#include "../src/base64.h"

static void base64_enc() {
	char input[] = "EzGE8rMvymf2eaNqAkfT";
	size_t encsz = base64_encode_len(strlen(input));
	char output[encsz];
	char expected[] = "RXpHRThyTXZ5bWYyZWFOcUFrZlQ=";
	base64_encode(output, input, strlen(input));
	ASSERT(!strcmp(expected, output));
}

static void base64_dec() {
	char input[] = "dzVmbU1SM2hkQmo0Y0RZTWpKakc=";
	size_t decsz = base64_decode_len(input);
	char output[decsz];
	char expected[] = "w5fmMR3hdBj4cDYMjJjG";
	base64_decode(output, input);
	ASSERT(!strcmp(expected, output));
}

static void base64() {
	char input[] = "AC8eChnS%R7udfws/sd#W5237bT6ZEnCDk76kP";
	char result[strlen(input)];
	size_t encsz = base64_encode_len(strlen(input));
	char output[encsz];
	base64_encode(output, input, strlen(input));
	base64_decode(result, output);
	ASSERT(!strcmp(input, result));
}

TEST_IMPL(base64) {

	TESTCASE("base64");

	/* Run testcase */
	base64_enc();
	base64_dec();
	base64();

	RETURN_OK();
}
