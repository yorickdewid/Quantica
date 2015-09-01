#ifdef LINUX
#if __STDC_VERSION__ >= 199901L
#define _XOPEN_SOURCE 700
#else
#define _XOPEN_SOURCE 500
#endif /* __STDC_VERSION__ */
#endif // LINUX

#include <time.h>

#ifdef __MACH__
#include <mach/clock.h>
#include <mach/mach.h>
#endif

#include "test.h"
#include "../src/quid.h"

#define NUM			2000000

static struct timespec start;

static void start_timer() {
#ifdef __MACH__
	clock_serv_t cclock;
	mach_timespec_t mts;
	host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
	clock_get_time(cclock, &mts);
	mach_port_deallocate(mach_task_self(), cclock);
	start.tv_sec = mts.tv_sec;
	start.tv_nsec = mts.tv_nsec;
#else
	clock_gettime(CLOCK_MONOTONIC, &start);
#endif
}

static double get_timer() {
	struct timespec end;
#ifdef __MACH__
	clock_serv_t cclock;
	mach_timespec_t mts;
	host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
	clock_get_time(cclock, &mts);
	mach_port_deallocate(mach_task_self(), cclock);
	end.tv_sec = mts.tv_sec;
	end.tv_nsec = mts.tv_nsec;
#else
	clock_gettime(CLOCK_MONOTONIC, &end);
#endif
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
