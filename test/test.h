#ifndef TEST_H_INCLUDED
#define TEST_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>

#include "config.h"

#pragma GCC diagnostic ignored "-Wunused-parameter"
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#define container_of(ptr, type, member) \
  ((type *) ((char *) (ptr) - offsetof(type, member)))

/* Draw gridline */
#define LINE()                          \
  do {                                  \
    fprintf(stderr, "+-----------------------+---------------------------+----------------------------------+---------------------+\n"); \
    fflush(stderr);                     \
  } while (0)

/* Log to stderr. */
#define LOG(...)                        \
  do {                                  \
    fprintf(stderr, "%s", __VA_ARGS__); \
    fflush(stderr);                     \
  } while (0)

/* Log format */
#define LOGF(...)                       \
  do {                                  \
    fprintf(stderr, __VA_ARGS__);       \
    fflush(stderr);                     \
  } while (0)

/* Die with fatal error. */
#define FATAL(msg)                                        \
  do {                                                    \
    fprintf(stderr,                                       \
            "Fatal error in %s on line %d: %s\n",         \
            __FILE__,                                     \
            __LINE__,                                     \
            msg);                                         \
    fflush(stderr);                                       \
    abort();                                              \
  } while (0)

/* Have our own assert, so we are sure it does not get optimized away in
 * a release build.
 */
#define ASSERT(expr)                                      \
 do {                                                     \
  if (!(expr)) {                                          \
    fprintf(stderr,                                       \
            "Assertion failed in %s on line %d: %s\n",    \
            __FILE__,                                     \
            __LINE__,                                     \
            #expr);                                       \
    abort();                                              \
  }                                                       \
 } while (0)

/* Just sugar for wrapping the main() for a task or helper. */
#define TEST_IMPL(name)                                                       \
  int run_test_##name();                                                      \
  int run_test_##name()

#define BENCHMARK_IMPL(name)                                                  \
  int run_benchmark_##name();                                                 \
  int run_benchmark_##name()

#define HELPER_IMPL(name)                                                     \
  int run_helper_##name();                                                    \
  int run_helper_##name()

#define CALL_BENCHMARK(name)                                                  \
  fprintf(stderr, "Call benchmark\n");                                        \
  fflush(stderr);                                                             \
  run_benchmark_##name();                                                     \

#define CALL_TEST(name)                                                       \
  fprintf(stderr, "Call test\n");                                             \
  fflush(stderr);                                                             \
  run_test_##name();                                                          \

/* Reserved test exit codes. */
enum test_status {
  TEST_OK = 0,
  TEST_TODO,
  TEST_SKIP
};

#define RETURN_OK()                                                           \
  do {                                                                        \
    return TEST_OK;                                                           \
  } while (0)

#define RETURN_TODO(explanation)                                              \
  do {                                                                        \
    LOGF("%s\n", explanation);                                                \
    return TEST_TODO;                                                         \
  } while (0)

#define RETURN_SKIP(explanation)                                              \
  do {                                                                        \
    LOGF("%s\n", explanation);                                                \
    return TEST_SKIP;                                                         \
  } while (0)

#if defined(__clang__) ||                                \
    defined(__GNUC__) ||                                 \
    defined(__INTEL_COMPILER) ||                         \
    defined(__SUNPRO_C)
# define UNUSED __attribute__((unused))
#else
# define UNUSED
#endif

#endif // TEST_H_INCLUDED
