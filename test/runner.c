#include <time.h>

#include "test.h"
#include "test-list.h"

void print_environment() {
	LOG("Start unittests\n");
	LOGF("Quantica: idx version %s\n", IDXVERSION);
	LOGF("Quantica: db version %s\n", DBVERSION);
	time_t now = time(NULL);
	LOGF("Date:\t\t%s", (char*)ctime(&now));
}

int main(int argc, char *argv[], char *envp[]) {
	print_environment();

	CALL_TEST(engine);
	CALL_TEST(quid);
	CALL_TEST(bootstrap);
	LOG("All tests passed\n");
	CALL_BENCHMARK(engine);
	CALL_BENCHMARK(quid);
	LOG("Benchmarks finished\n");

	return 0;
}
