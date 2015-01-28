#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "config.h"
#include "quid.h"
#include "engine.h"

#define R_NUM	1000
#define NUM		200000

#define DBNAME		"_bootstrap"
#define KEYSIZE		16
#define VALSIZE		100
#define LINE		"+-----------------------+---------------------------+----------------------------------+---------------------+\n"
#define LINE1		"--------------------------------------------------------------------------------------------------------------\n"

static struct timespec start;
static struct btree btree;
static void start_timer()
{
	clock_gettime(CLOCK_MONOTONIC, &start);
}

static double get_timer()
{
	struct timespec end;
	clock_gettime(CLOCK_MONOTONIC, &end);
	long seconds  = end.tv_sec  - start.tv_sec;
	long nseconds = end.tv_nsec - start.tv_nsec;
	return seconds + (double) nseconds / 1.0e9;
}

static char val[VALSIZE+1] = {'\0'};
double _file_size=((double)(KEYSIZE+8*6)*NUM)/1048576.0+((double)(VALSIZE+8*3)*NUM)/1048576.0;
double _query_size=(double)((double)(KEYSIZE+VALSIZE+8*4)*R_NUM)/1048576.0;
struct quid quidr[NUM];

void random_value()
{
	char salt[10] = {'1','2','3','4','5','6','7','8','a','b'};
	int i;
	for(i=0; i<VALSIZE; ++i) {
		val[i] = salt[rand()%10];
	}
}

void print_header()
{
	printf("Keys:		%d bytes each\n",KEYSIZE);
	printf("Values:		%d bytes each\n",VALSIZE);
	printf("Entries:	%d\n",NUM);
	printf("IndexSize:	%.1f MB (estimated)\n",(double)((double)(KEYSIZE+8*6)*NUM)/1048576.0);
	printf("DBSize:		%.1f MB (estimated)\n",(double)((double)(VALSIZE+8*3)*NUM)/1048576.0);
}

void print_environment()
{
	printf("Quantica:	idx version %s\n", IDXVERSION);
	printf("Quantica:	db version %s\n", DBVERSION);
	time_t now=time(NULL);
	printf("Date:		%s", (char*)ctime(&now));
}

void quidtostr(char *s, struct quid *u)
{
	sprintf(s, "{%.8x-%.4x-%.4x%x%.2x-%.2x%.2x%.2x%.2x%.2x%.2x}"
			, (unsigned int)u->time_low
			, u->time_mid
			, u->time_hi_and_version
			, u->clock_seq_hi_and_reserved
			, u->clock_seq_low
			, u->node[0]
			, u->node[1]
			, u->node[2]
			, u->node[3]
			, u->node[4]
			, u->node[5]);
}

void db_write_test()
{
	struct quid key;
	int v_len = strlen(val);
	start_timer();
	int i;
	for(i=0; i<NUM; ++i) {
		memset(&key, 0, sizeof(struct quid));
		quid_create(&key);
		btree_insert(&btree, &key, val, v_len);
		memcpy(&quidr[i], &key, sizeof(struct quid));
		if(!(i%10000)) {
			fprintf(stderr,"finished %d ops%30s\r", i, "");
			fflush(stderr);
		}
	}
	printf(LINE);
	double cost = get_timer();
	printf("|write		(succ:%d): %.6f sec/op; %.1f writes/sec(estimated); %.1f MB/sec; cost:%.6f(sec)\n"
	       ,NUM
	       ,(double)(cost/NUM)
	       ,(double)(NUM/cost)
	       ,(_file_size/cost)
	       ,(double)cost);
}

void db_read_seq_test()
{
	struct quid key;
	int all = 0, i;
	int start = NUM/2;
	int end = start+R_NUM;
	char squid[35] = {'\0'};
	start_timer();
	for(i=start; i<end; ++i) {
		memset(&key, 0, sizeof(struct quid));
		memcpy(&key, &quidr[i], sizeof(struct quid));

		size_t len;
		void *data = btree_get(&btree, &key, &len);
		if(data!=NULL) {
			all++;
		} else {
			quidtostr(squid, &key);
			printf("not found: %s\n", squid);
		}

		free(data);

		if(!(i%10000)) {
			fprintf(stderr,"finished %d ops%30s\r",i,"");
			fflush(stderr);
		}
	}
	printf(LINE);
	double cost=get_timer();
	printf("|readseq	(found:%d): %.6f sec/op; %.1f reads /sec(estimated); %.1f MB/sec; cost:%.6f(sec)\n"
	       ,all
	       ,(double)(cost/R_NUM)
	       ,(double)(R_NUM/cost)
	       ,(_query_size/cost)
	       ,cost);
}

void db_read_random_test()
{
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
		void *data = btree_get(&btree, &key, &len);
		if(data!=NULL) {
			all++;
		} else {
			quidtostr(squid, &key);
			printf("not found: %s\n", squid);
		}

		free(data);

		if((i%10000)==0) {
			fprintf(stderr,"finished %d ops%30s\r",i,"");
			fflush(stderr);
		}
	}
	printf(LINE);
	double cost = get_timer();
	printf("|readrandom	(found:%d): %.6f sec/op; %.1f reads /sec(estimated); %.1f MB/sec; cost:%.6f(sec)\n"
	       ,all
	       ,(double)(cost/R_NUM)
	       ,(double)(R_NUM/cost)
	       ,(_query_size/cost)
	       ,cost);
}

void db_delete_random_test()
{
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
		btree_delete(&btree, &key);
		void *data = btree_get(&btree, &key, &len);
		if(data==NULL) {
			all++;
		} else {
			quidtostr(squid, &key);
			printf("found: %s\n", squid);
		}

		free(data);

		if((i%10000)==0) {
			fprintf(stderr,"finished %d ops%30s\r",i,"");
			fflush(stderr);
		}
	}
	printf(LINE);
	double cost = get_timer();
	printf("|deleterandom	(delete:%d): %.6f sec/op; %.1f reads /sec(estimated); %.1f MB/sec; cost:%.6f(sec)\n"
	       ,all
	       ,(double)(cost/R_NUM)
	       ,(double)(R_NUM/cost)
	       ,(_query_size/cost)
	       ,cost);
}

void db_tests()
{
	db_write_test();
	db_read_seq_test();
	db_read_random_test();
	db_delete_random_test();
	printf(LINE);
}


int main(__attribute__((unused)) int argc, __attribute__((unused)) char **argv)
{
	srand(time(NULL));
	print_header();
	print_environment();
	random_value();

	btree_init(&btree, DBNAME);

	db_tests();

	btree_close(&btree);
	btree_purge(DBNAME);

	return 0;
}
