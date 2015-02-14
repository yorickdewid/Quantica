#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "test.h"
#include "quid.h"
#include "engine.h"

#define NUM			200000
#define R_NUM		(NUM/200)
#define DBNAME		"test_benchmark"
#define KEYSIZE		16
#define VALSIZE		100

static struct timespec start;
static struct btree btree;
static char val[VALSIZE+1] = {'\0'};
struct quid quidr[NUM];

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

void random_value() {
	char salt[10] = {'1','2','3','4','5','6','7','8','a','b'};
	int i;
	for(i=0; i<VALSIZE; ++i) {
		val[i] = salt[rand()%10];
	}
}

void print_header() {
	LOGF("Keys:		%d bytes each\n", KEYSIZE);
	LOGF("Values:		%d bytes each\n", VALSIZE);
	LOGF("Entries:	%d\n", NUM);
}

void print_environment() {
	LOGF("Quantica:	idx version %s\n", IDXVERSION);
	LOGF("Quantica:	db version %s\n", DBVERSION);
	time_t now = time(NULL);
	LOGF("Date:		%s", (char*)ctime(&now));
}

void db_write_test() {
	struct quid key;
	int v_len = strlen(val);
	start_timer();
	int i;
	for(i=0; i<NUM; ++i) {
		memset(&key, 0, sizeof(struct quid));
		quid_create(&key);
		if (btree_insert(&btree, &key, val, v_len)<0)
			FATAL("btree_insert");
		memcpy(&quidr[i], &key, sizeof(struct quid));
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

void db_read_seq_test() {
	struct quid key;
	int all = 0, i;
	int start = NUM/2;
	int end = start+R_NUM;
	char squid[35] = {'\0'};
	start_timer();
	for(i=start; i<end; ++i) {
		memcpy(&key, &quidr[i], sizeof(struct quid));

		size_t len;
		void *data = btree_get(&btree, &key, &len);
		if(data!=NULL) {
			all++;
		} else {
			quidtostr(squid, &key);
			FATAL("Key not found");
		}

		free(data);

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

void db_read_random_test() {
	struct quid key;
	int all=0,i;
	int start=NUM/2;
	int end=start+R_NUM;
	char squid[35] = {'\0'};
	start_timer();
	for(i=start; i<end; ++i) {
		memcpy(&key, &quidr[i], sizeof(struct quid));

		size_t len;
		void *data = btree_get(&btree, &key, &len);
		if(data!=NULL) {
			all++;
		} else {
			quidtostr(squid, &key);
			FATAL("Key not found");
		}

		free(data);

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

void db_read_bounds_test() {
	struct quid key;
	int all=0,i;
	int end=NUM/2000;
	char squid[35] = {'\0'};
	start_timer();
	for(i=0; i<end; ++i) {
		memcpy(&key, &quidr[i], sizeof(struct quid));

		size_t len;
		void *data = btree_get(&btree, &key, &len);
		if(data!=NULL) {
			all++;
		} else {
			quidtostr(squid, &key);
			FATAL("Key not found");
		}

		free(data);

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

void db_delete_random_test() {
	struct quid key;
	int all=0,i;
	int start=NUM/2;
	int end=start+R_NUM;
	char squid[35] = {'\0'};
	start_timer();
	for(i=start; i<end; ++i) {
		memset(&key, 0, sizeof(struct quid));
		memcpy(&key, &quidr[i], sizeof(struct quid));

		size_t len;
		if(btree_delete(&btree, &key)<0)
			FATAL("btree_delete");
		void *data = btree_get(&btree, &key, &len);
		if(data==NULL) {
			all++;
		} else {
			quidtostr(squid, &key);
			FATAL("Key found");
		}

		free(data);

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

void db_read_test() {
	struct quid key;
	start_timer();
	int all=0,i;
	for(i=0; i<NUM; ++i) {
		memcpy(&key, &quidr[i], sizeof(struct quid));

		size_t len;
		void *data = btree_get(&btree, &key, &len);
		if(data!=NULL) {
			all++;
		}

		free(data);

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

BENCHMARK_IMPL(performance) {
	srand(time(NULL));
	print_header();
	print_environment();
	random_value();

	/* Create new database */
	btree_init(&btree, DBNAME);

	/* Run testcase */
	db_write_test();
	db_read_seq_test();
	db_read_random_test();
	db_read_bounds_test();
	db_delete_random_test();
	db_read_test();

	LINE();

	/* Close and delete database */
	btree_close(&btree);
	btree_purge(DBNAME);

	RETURN_OK();
}
