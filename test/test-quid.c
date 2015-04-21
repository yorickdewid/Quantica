#include <string.h>

#include "test.h"
#include "../src/quid.h"

static void quid_compare(){
	quid_t quid1;
	quid_t quid2;
	quid_create(&quid1);
	memcpy(&quid2, &quid1, sizeof(quid_t));
	ASSERT(!quidcmp(&quid1, &quid2));
}

static void quid_generate(){
	quid_t rquid[5];
	quid_create(&rquid[0]);
	quid_create(&rquid[1]);
	quid_create(&rquid[2]);
	quid_create(&rquid[3]);
	quid_create(&rquid[4]);

	int i, j;
	for(i=0; i<5; ++i){
		for(j=0; j<5; ++j){
			if(j==i)
				continue;
			ASSERT(quidcmp(&rquid[i], &rquid[j]));
		}
	}
}

static void quid_sformat(){
	char *pch;
	int phyp, nhyp = 0;
	quid_t quid;
	char squid[QUID_LENGTH+1] = {'\0'};
	quid_create(&quid);
	quidtostr(squid, &quid);

	ASSERT(strlen(squid)==QUID_LENGTH);
	ASSERT(squid[0]=='{'&&squid[QUID_LENGTH-1]=='}');
	ASSERT(squid[strspn(squid, "{}-0123456789abcdefABCDEF")]==0);

	pch = strchr(squid, '-');
	while(pch != NULL){
		nhyp++;
		phyp = pch-squid;
		ASSERT(phyp==9||phyp==14||phyp==19||phyp==24);
		pch = strchr(pch+1, '-');
	}
	ASSERT(nhyp==4);
}

static void quid_convertoi(){
	quid_t quido;
	quid_t quidi;
	char squid[QUID_LENGTH+1] = {'\0'};
	quid_create(&quido);
	quidtostr(squid, &quido);
	strtoquid(squid, &quidi);
	ASSERT(!quidcmp(&quido, &quidi));
}

static void quid_convertio(){
	quid_t quid;
	char squidi[] = "{00000000-0000-a150-8345-c649140dc096}";
	char squido[QUID_LENGTH+1] = {'\0'};
	strtoquid(squidi, &quid);
	quidtostr(squido, &quid);
	ASSERT(!strcmp(squidi, squido));
}

TEST_IMPL(quid) {
	/* Run testcase */
	quid_compare();
	quid_generate();
	quid_sformat();
	quid_convertoi();
	quid_convertio();

	RETURN_OK();
}
