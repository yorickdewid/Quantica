#ifndef TESTLIST_H_INCLUDED
#define TESTLIST_H_INCLUDED

#include "test.h"

/*
 * List of testunits
 */
TEST_IMPL(engine);
TEST_IMPL(quid);
TEST_IMPL(aes);
TEST_IMPL(base64);
TEST_IMPL(crc32);
TEST_IMPL(sha1);
TEST_IMPL(md5);
TEST_IMPL(bootstrap);
BENCHMARK_IMPL(engine);
BENCHMARK_IMPL(quid);

#endif // TEST-LIST_H_INCLUDED
