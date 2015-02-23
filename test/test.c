#include "test.h"
#include "test-list.h"

int main(int argc, char *argv[], char *envp[]) {

	CALL_BENCHMARK(performance);
	CALL_TEST(quid);

	return 0;
}
