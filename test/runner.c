#include <time.h>
#include <common.h>

#include "test.h"
#include "test-list.h"

void print_environment() {
	LOG("Start unittests\n");
	LOGF(PROGNAME " %s ("__DATE__", "__TIME__")\n", get_version_string());
	time_t now = time(NULL);
	LOGF("Date:\t\t%s", (char*)ctime(&now));
}

int main(int argc, char *argv[], char *envp[]) {
	print_environment();

	CALL_TEST(engine);
	CALL_TEST(quid);
	CALL_TEST(aes);
	CALL_TEST(base64);
	CALL_TEST(crc32);
	CALL_TEST(sha1);
	CALL_TEST(md5);
	CALL_TEST(bootstrap);
	LOG("All tests passed\n");
	CALL_BENCHMARK(engine);
	CALL_BENCHMARK(quid);
	LOG("Benchmarks finished\n");

	return 0;
}
