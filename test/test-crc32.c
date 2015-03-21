#include <string.h>

#include "test.h"
#include "../src/crc32.h"

static void crc32_1(){
    char input[] = "ygd5i7f%Fd&weDif8i^fikf6d6ikf6ifikUFf57r&DFC%&F%ydyt";
    unsigned long output = 0;
    unsigned long expected = 3558500220;
    output = crc32_calculate(output, input, strlen(input));
	ASSERT(expected==output);
}

static void crc32_2(){
    char input[] = "3a5";
    unsigned long output = 0;
    unsigned long expected = 4080606238;
    output = crc32_calculate(output, input, strlen(input));
	ASSERT(expected==output);
}

static void crc32_3(){
    char input[] = "3a57ERTFGO68G68T6o8etor86eliwfgefd78342fgl4ifgeig6irt%u&rq&iRE8O23T9312T487GIYGI\":]WDWE]'A;S'Q[;S;'QD;WE[F;WE;";
    unsigned long output = 0;
    unsigned long expected = 3492629019;
    output = crc32_calculate(output, input, strlen(input));
	ASSERT(expected==output);
}

TEST_IMPL(crc32) {
	/* Run testcase */
	crc32_1();
	crc32_2();
	crc32_3();

	RETURN_OK();
}
