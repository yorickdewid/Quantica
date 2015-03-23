#if __STDC_VERSION__ >= 199901L
#define _XOPEN_SOURCE 700
#else
#define _XOPEN_SOURCE 500
#endif /* __STDC_VERSION__ */

#include <time.h>

#include "test.h"
#include "../src/quid.h"

#define NUM			2000000

static struct timespec start;

static void start_timer() {
	clock_gettime(CLOCK_MONOTONIC, &start);
}

static double get_timer() {
	struct timespec end;
	clock_gettime(CLOCK_MONOTONIC, &end);
	long seconds  = end.tv_sec - start.tv_sec;
	long nseconds = end.tv_nsec - start.tv_nsec;
	return seconds + (double)nseconds / 1.0e9;
}

static void print_header() {
	LOGF("Entries:\t%d\n", NUM);
}

static void quid_generate() {
	quid_t quid;
	start_timer();
	int all=0,i;
	for(i=0; i<NUM; ++i) {
		quid_create(&quid);

		if((i%10000)==0)
			LOGF("finished %d ops%30s\r",i,"");
	}
	LINE();
	double cost = get_timer();
	LOGF("|create		(generate:%d): %.6f sec/op; %.1f reads /sec(estimated); cost:%.6f(sec)\n"
	       ,all
	       ,(double)(cost/NUM)
	       ,(double)(NUM/cost)
	       ,cost);
}

BENCHMARK_IMPL(quid) {
	print_header();

	/* Run testcase */
	quid_generate();

	LINE();

	RETURN_OK();
}
