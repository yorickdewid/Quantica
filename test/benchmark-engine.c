#ifdef LINUX
#if __STDC_VERSION__ >= 199901L
#define _XOPEN_SOURCE 700
#else
#define _XOPEN_SOURCE 500
#endif /* __STDC_VERSION__ */
#endif // LINUX

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef __MACH__
#include <mach/clock.h>
#include <mach/mach.h>
#endif

#include "test.h"
#include "../src/zmalloc.h"
#include "../src/arc4random.h"
#include "../src/quid.h"
#include "../src/engine.h"

#define NUM			200000
#define R_NUM		(NUM/200)
#define IDXNAME		"bmark_engine.idx"
#define DBNAME		"bmark_engine.db"
#define KEYSIZE		16
#define VALSIZE		100

static struct timespec timer_start;
static struct engine e;
static char val[VALSIZE+1] = {'\0'};
quid_t quidr[NUM];

static void start_timer() {
#ifdef __MACH__
	clock_serv_t cclock;
	mach_timespec_t mts;
	host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
	clock_get_time(cclock, &mts);
	mach_port_deallocate(mach_task_self(), cclock);
	timer_start.tv_sec = mts.tv_sec;
	timer_start.tv_nsec = mts.tv_nsec;
#else
	clock_gettime(CLOCK_MONOTONIC, &timer_start);
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
	long seconds  = end.tv_sec - timer_start.tv_sec;
	long nseconds = end.tv_nsec - timer_start.tv_nsec;
	return seconds + (double)nseconds / 1.0e9;
}

static void random_value() {
	char salt[10] = {'1','2','3','4','5','6','7','8','a','b'};
	int i;
	for(i=0; i<VALSIZE; ++i) {
		val[i] = salt[arc4random()%10];
	}
}

static void print_header() {
	LOGF("Keys:\t\t%d bytes each\n", KEYSIZE);
	LOGF("Values:\t\t%d bytes each\n", VALSIZE);
	LOGF("Entries:\t%d\n", NUM);
}

static void db_write_test() {
	quid_t key;
	int v_len = strlen(val);
	char squid[35] = {'\0'};
	start_timer();
	int i;
	for(i=0; i<NUM; ++i) {
		memset(&key, 0, sizeof(quid_t));
		quid_create(&key);
		quidtostr(squid, &key);
		memcpy(&quidr[i], &key, sizeof(quid_t));
		if (engine_insert_data(&e, &key, val, v_len)<0)
			FATAL("engine_insert");

		if(!(i%10000))
			LOGF("finished %d ops%30s\r", i, "");
	}
	LINE();
	double cost = get_timer();
	LOGF("|write		(succ:%d): %.6f sec/op; %.1f writes/sec(estimated); cost:%.6f(sec)\n"
	       ,NUM
	       ,(double)(cost/NUM)
	       ,(double)(NUM/cost)
	       ,(double)cost);
}

static void db_read_seq_test() {
	quid_t key;
	int all = 0, i;
	int start = NUM/2;
	int end = start+R_NUM;
	char squid[35] = {'\0'};
	start_timer();
	for(i=start; i<end; ++i) {
		memcpy(&key, &quidr[i], sizeof(quid_t));

		size_t len;
		uint64_t offset = engine_get(&e, &key);
		void *data = get_data(&e, offset, &len);
		if(data!=NULL) {
			all++;
		} else {
			quidtostr(squid, &key);
			FATAL("Key not found");
		}

		zfree(data);

		if(!(i%10000))
			LOGF("finished %d ops%30s\r",i,"");
	}
	LINE();
	double cost=get_timer();
	LOGF("|readseq	(found:%d): %.6f sec/op; %.1f reads /sec(estimated); cost:%.6f(sec)\n"
	       ,all
	       ,(double)(cost/R_NUM)
	       ,(double)(R_NUM/cost)
	       ,cost);
}

static void db_read_random_test() {
	quid_t key;
	int all=0,i;
	int start=NUM/2;
	int end=start+R_NUM;
	char squid[35] = {'\0'};
	start_timer();
	for(i=start; i<end; ++i) {
		memcpy(&key, &quidr[i], sizeof(quid_t));

		size_t len;
		uint64_t offset = engine_get(&e, &key);
		void *data = get_data(&e, offset, &len);
		if(data!=NULL) {
			all++;
		} else {
			quidtostr(squid, &key);
			FATAL("Key not found");
		}

		zfree(data);

		if((i%10000)==0)
			LOGF("finished %d ops%30s\r",i,"");
	}
	LINE();
	double cost = get_timer();
	LOGF("|readrandom	(found:%d): %.6f sec/op; %.1f reads /sec(estimated); cost:%.6f(sec)\n"
	       ,all
	       ,(double)(cost/R_NUM)
	       ,(double)(R_NUM/cost)
	       ,cost);
}

static void db_read_bounds_test() {
	quid_t key;
	int all=0,i;
	int end=NUM/2000;
	char squid[35] = {'\0'};
	start_timer();
	for(i=0; i<end; ++i) {
		memcpy(&key, &quidr[i], sizeof(quid_t));

		size_t len;
		uint64_t offset = engine_get(&e, &key);
		void *data = get_data(&e, offset, &len);
		if(data!=NULL) {
			all++;
		} else {
			quidtostr(squid, &key);
			FATAL("Key not found");
		}

		zfree(data);

		if((i%10000)==0)
			LOGF("finished %d ops%30s\r",i,"");
	}
	LINE();
	double cost = get_timer();
	LOGF("|readbounds	(found:%d): %.6f sec/op; %.1f reads /sec(estimated); cost:%.6f(sec)\n"
	       ,all
	       ,(double)(cost/R_NUM)
	       ,(double)(R_NUM/cost)
	       ,cost);
}

#if 0
static void db_delete_test() {
	quid_t key;
	int all=0, i;
	char squid[35] = {'\0'};
	start_timer();
	for(i=0; i<NUM; ++i) {
		memset(&key, 0, sizeof(quid_t));
		memcpy(&key, &quidr[i], sizeof(quid_t));
		quidtostr(squid, &key);

		size_t len;
		if(engine_delete(&e, &key)<0) {
			quidtostr(squid, &key);
			LOGF("Cannot delete key %s[%d]\n", squid, i);
			FATAL("engine_delete");
		}
		uint64_t offset = engine_get(&e, &key);
		void *data = get_data(&e, offset, &len);
		if(data==NULL) {
			all++;
		} else {
			FATAL("Key found");
		}

		zfree(data);

		if((i%10000)==0)
			LOGF("finished %d ops%30s\r",i,"");
	}
	LINE();
	double cost = get_timer();
	LOGF("|deleterandom	(delete:%d): %.6f sec/op; %.1f reads /sec(estimated); cost:%.6f(sec)\n"
	       ,all
	       ,(double)(cost/R_NUM)
	       ,(double)(R_NUM/cost)
	       ,cost);
}
#endif

static void db_delete_random_test() {
	quid_t key;
	int all=0,i;
	int start=NUM/2;
	int end=start+R_NUM;
	char squid[35] = {'\0'};
	start_timer();
	for(i=start; i<end; ++i) {
		memset(&key, 0, sizeof(quid_t));
		memcpy(&key, &quidr[i], sizeof(quid_t));

		size_t len;
		if(engine_delete(&e, &key)<0) {
			quidtostr(squid, &key);
			FATAL("engine_delete");
		}
		uint64_t offset = engine_get(&e, &key);
		void *data = get_data(&e, offset, &len);
		if(data==NULL) {
			all++;
		} else {
			FATAL("Key found");
		}

		zfree(data);

		if((i%10000)==0)
			LOGF("finished %d ops%30s\r",i,"");
	}
	LINE();
	double cost = get_timer();
	LOGF("|deleterandom	(delete:%d): %.6f sec/op; %.1f reads /sec(estimated); cost:%.6f(sec)\n"
	       ,all
	       ,(double)(cost/R_NUM)
	       ,(double)(R_NUM/cost)
	       ,cost);
}

static void db_read_test() {
	quid_t key;
	start_timer();
	int all=0,i;
	for(i=0; i<NUM; ++i) {
		memcpy(&key, &quidr[i], sizeof(quid_t));

		size_t len;
		uint64_t offset = engine_get(&e, &key);
		void *data = get_data(&e, offset, &len);
		if(data!=NULL) {
			all++;
		}

		zfree(data);

		if((i%10000)==0)
			LOGF("finished %d ops%30s\r",i,"");
	}
	LINE();
	double cost = get_timer();
	LOGF("|read		(found:%d): %.6f sec/op; %.1f reads /sec(estimated); cost:%.6f(sec)\n"
	       ,all
	       ,(double)(cost/R_NUM)
	       ,(double)(R_NUM/cost)
	       ,cost);
}

BENCHMARK_IMPL(engine) {
	print_header();
	random_value();

	/* Create new database */
	engine_init(&e, IDXNAME, DBNAME);

	/* Run testcase */
	db_write_test();
	db_read_seq_test();
	db_read_random_test();
	db_read_bounds_test();
#if 0
	db_delete_test();
#endif
	db_delete_random_test();
	db_read_test();

	LINE();

	/* Close and delete database */
	engine_close(&e);
	unlink(IDXNAME);
	unlink(DBNAME);

	RETURN_OK();
}
