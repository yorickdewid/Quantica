#include <string.h>

#include "test.h"
#include "../src/quid.h"

static void quid_compare(){
	struct quid quid1;
	struct quid quid2;
	quid_create(&quid1);
	memcpy(&quid2, &quid1, sizeof(struct quid));
	ASSERT(!quidcmp(&quid1, &quid2));
}

static void quid_generate(){
	struct quid rquid[5];
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
	struct quid quid;
	char squid[39] = {'\0'};
	quid_create(&quid);
	quidtostr(squid, &quid);

	ASSERT(strlen(squid)==38);
	ASSERT(squid[0]=='{'&&squid[37]=='}');
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
	struct quid quido;
	struct quid quidi;
	char squid[39] = {'\0'};
	quid_create(&quido);
	quidtostr(squid, &quido);
	strtoquid(squid, &quidi);
	ASSERT(!quidcmp(&quido, &quidi));
}

static void quid_convertio(){
	struct quid quid;
	char squidi[] = "{00000000-0000-a150-8345-c649140dc096}";
	char squido[39] = {'\0'};
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
