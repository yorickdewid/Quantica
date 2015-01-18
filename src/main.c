#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "engine.h"

#define UNUSED(x) (void)(x)

#define R_NUM	10000
#define NUM		2000000

#define DBNAME		"_bootstrap"
#define V			"1.1"
#define KEYSIZE		16
#define VALSIZE		100
#define LINE		"+-----------------------+---------------------------+----------------------------------+---------------------+\n"
#define LINE1		"--------------------------------------------------------------------------------------------------------------\n"

static struct timespec start;
static struct btree btree;
static void start_timer(void)
{
	clock_gettime(CLOCK_MONOTONIC, &start);
}

static double get_timer(void)
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
	printf("BTreeDB:	version %s\n", V);
	time_t now=time(NULL);
	printf("Date:		%s", (char*)ctime(&now));

	/*int num_cpus=0;
	char cpu_type[256]= {0};
	char cache_size[256]= {0};

	FILE* cpuinfo=fopen("/proc/cpuinfo","r");
	if(cpuinfo) {
	     char line[1024]= {0};
	     while(fgets(line,sizeof(line),cpuinfo)!=NULL) {
	          const char* sep=strchr(line,':');
	          if(sep==NULL||strlen(sep)<10)
	               continue;

	          char key[1024]= {0};
	          char val[1024]= {0};
	          strncpy(key,line,sep-1-line);
	          strncpy(val,sep+1,strlen(sep)-1);
	          if(strcmp("model name",key)==0) {
	               num_cpus++;
	               strcpy(cpu_type,val);
	          } else if(strcmp("cache size",key)==0) {
	               strncpy(cache_size,val+1,strlen(val)-1);
	          }
	     }

	     fclose(cpuinfo);
	     printf("CPU:		%d * %s",num_cpus,cpu_type);
	     printf("CPUCache:	%s\n",cache_size);
	}*/
}

void db_write_test()
{
	uint8_t key[KEYSIZE];
	int v_len = strlen(val);
	start_timer();
	int i;
	for(i=0; i<NUM; ++i) {
		memset(key, 0, sizeof(key));
		sprintf((char *)key, "%dkey", i);
		btree_insert(&btree, key, val, v_len);
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
	uint8_t key[KEYSIZE];
	int all = 0, i;
	int start = NUM/2;
	int end = start+R_NUM;
	start_timer();
	for(i=start; i<end; ++i) {
		memset(key, 0, sizeof(key));
		sprintf((char *)key, "%dkey", i);

		size_t len;
		void *data = btree_get(&btree, key, &len);
		if(data!=NULL) {
			all++;
		} else {
			printf("not found:%s\n", key);
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
	       ,R_NUM
	       ,(double)(cost/R_NUM)
	       ,(double)(R_NUM/cost)
	       ,(_query_size/cost)
	       ,cost);
}

void db_read_random_test()
{
	uint8_t key[KEYSIZE];
	int all=0,i;
	int start=NUM/2;
	int end=start+R_NUM;
	start_timer();
	for (i = start; i <end ; ++i) {
		memset(key,0,sizeof(key));
		sprintf((char *)key, "%dkey", rand()%(i+1));

		size_t len;
		void *data = btree_get(&btree, key, &len);
		if(data!=NULL) {
			all++;
		} else {
			printf("not found:%s\n", key);
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
	       ,R_NUM
	       ,(double)(cost/R_NUM)
	       ,(double)(R_NUM/cost)
	       ,(_query_size/cost)
	       ,cost);
}

void db_delete_test()
{
	uint8_t key[KEYSIZE];
	int i;
	for(i=0; i<10; ++i) {
		sprintf((char *)key, "%dkey", i);
		btree_delete(&btree, key);
	}

	for(i=0; i<10; ++i) {
		sprintf((char *)key, "%dkey", i);

		size_t len;
		void *data = btree_get(&btree, key, &len);
		if(data!=NULL) {
			printf("Found:%s\n", key);
		} else {
			printf("not found:%s\n", key);
		}

		free(data);
	}
}

void db_tests()
{
	db_write_test();
	db_read_seq_test();
	db_read_random_test();
	db_delete_test();
	printf(LINE);
}


int main(int argc, char **argv)
{
	UNUSED(argc);
	UNUSED(argv);

	srand(time(NULL));
	print_header();
	print_environment();
	random_value();

	btree_init(&btree, DBNAME);

	db_tests();

	btree_close(&btree);
	//btree_purge(DBNAME);

	return 0;
}

