#include <string.h>

#include "test.h"
#include "../src/sha1.h"

static void sha1_1(){
    const char input[] = "0H7QXZiydOZqL3OSkw11sT7541OVlnd8Lo0JgrEM9RIqSgUbwPyTNe4PIy0q";
    const char expected[] = "79d97164b41dbe45c2461fd38556d3a82fcff4a9";
    char output[SHA1_LENGTH+1];
    struct sha sha;
    sha1_reset(&sha);
    sha1_input(&sha, (const unsigned char *)input, strlen(input));
    ASSERT(sha1_result(&sha));
    sha1_strsum(output, &sha);
    output[SHA1_LENGTH] = '\0';
	ASSERT(!strcmp(expected, output));
}

static void sha1_2(){
    const char input[] = "ABC@123";
    const char expected[] = "9cfd6e7eb791b8aad23c2a729139bf6ee842991f";
    char output[SHA1_LENGTH+1];
    struct sha sha;
    sha1_reset(&sha);
    sha1_input(&sha, (const unsigned char *)input, strlen(input));
    ASSERT(sha1_result(&sha));
    sha1_strsum(output, &sha);
    output[SHA1_LENGTH] = '\0';
	ASSERT(!strcmp(expected, output));
}

static void sha1_3(){
    const char input[] = "EG4Ie9vppDjvkheAGch69cfd4Cptgs2e0aAzuie67drfi6f6RFI67WFR67IRFI67F6ifi68f7AmBQkVdy6e7eb791b8aad23c2a729139bf6ee842mmToOtquB6EIGG6iwgdi8f5";
    const char expected[] = "93231ee44323dad2046dbb956caf66a8f459e46f";
    char output[SHA1_LENGTH+1];
    struct sha sha;
    sha1_reset(&sha);
    sha1_input(&sha, (const unsigned char *)input, strlen(input));
    ASSERT(sha1_result(&sha));
    sha1_strsum(output, &sha);
    output[SHA1_LENGTH] = '\0';
	ASSERT(!strcmp(expected, output));
}

TEST_IMPL(sha1) {
	/* Run testcase */
	sha1_1();
	sha1_2();
	sha1_3();

	RETURN_OK();
}
