#ifndef TESTLIST_H_INCLUDED
#define TESTLIST_H_INCLUDED

#include "test.h"

/*
 * List of testunits
 */
TEST_IMPL(engine);
TEST_IMPL(quid);
BENCHMARK_IMPL(engine);
BENCHMARK_IMPL(quid);

#endif // TEST-LIST_H_INCLUDED
