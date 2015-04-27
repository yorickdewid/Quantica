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

#include "test.h"
#include "../src/zmalloc.h"
#include "../src/arc4random.h"
#include "../src/quid.h"
#include "../src/engine.h"

#define NUM		200000
#define R_NUM		(NUM/200)
#define DBNAME		"bmark_engine"
#define KEYSIZE		16
#define VALSIZE		100

static struct timespec start;
static struct engine e;
static char val[VALSIZE+1] = {'\0'};
quid_t quidr[NUM];

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
	start_timer();
	int i;
	for(i=0; i<NUM; ++i) {
		memset(&key, 0, sizeof(quid_t));
		quid_create(&key);
		if (engine_insert(&e, &key, val, v_len)<0)
			FATAL("engine_insert");
		memcpy(&quidr[i], &key, sizeof(quid_t));
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
		void *data = engine_get(&e, &key, &len);
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
		void *data = engine_get(&e, &key, &len);
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
		void *data = engine_get(&e, &key, &len);
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
		if(engine_delete(&e, &key)<0)
			FATAL("engine_delete");
		void *data = engine_get(&e, &key, &len);
		if(data==NULL) {
			all++;
		} else {
			quidtostr(squid, &key);
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
		void *data = engine_get(&e, &key, &len);
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
	engine_init(&e, DBNAME);

	/* Run testcase */
	db_write_test();
	db_read_seq_test();
	db_read_random_test();
	db_read_bounds_test();
	db_delete_random_test();
	db_read_test();

	LINE();

	/* Close and delete database */
	engine_close(&e);
	engine_unlink(DBNAME);

	RETURN_OK();
}
