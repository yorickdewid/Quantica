#include "test.h"
#include "test-list.h"

int main(int argc, char *argv[], char *envp[]) {

	char** env;
	for(env = envp; *env != 0; env++){
		char* thisEnv = *env;
		printf("%s\n", thisEnv);
	  }

	CALL_BENCHMARK(performance);

	return 0;
}
