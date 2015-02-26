#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "core.h"
#include "engine.h"

#define BUFFER_SIZE 1<<16
#define ARR_SIZE 1<<16

void usage() {
	puts("help\t\t\tthis help menu");
	puts("license\t\t\tshow software license");
	puts("store <data>\t\tstore data in the database");
	puts("request <quid>\t\tretrieve data by key");
	puts("meta <quid>\t\tshow metadata from key");
	puts("update <quid>\t\tupdate metadata from key");
	puts("stats\t\t\tshow database statistics");
	puts("vacuum\t\t\tclean the database");
	puts("exit\t\t\texit shell");
}

void parse_args(char *buffer, char** args,
                size_t args_size, size_t *nargs)
{
    char *buf_args[args_size];
    char **cp;
    char *wbuf;
    size_t i, j;

    wbuf = buffer;
    buf_args[0] = buffer;
    args[0] = buffer;

    for(cp=buf_args; (*cp=strsep(&wbuf, " \n\t")) != NULL ;){
        if ((*cp != '\0') && (++cp >= &buf_args[args_size]))
            break;
    }

    for (j=i=0; buf_args[i]!=NULL; i++){
        if(strlen(buf_args[i])>0)
            args[j++]=buf_args[i];
    }

    *nargs=j;
    args[j]=NULL;
}

int main(int argc, char *argv[]) {
    char buffer[BUFFER_SIZE];
    char *args[ARR_SIZE];

	(void)(argc);
	(void)(argv);
	start_core();

    size_t nargs;
    printf("Quantica %s (%s, %s)\nType \"help\" or \"license\" for more information.\n", IDXVERSION, __DATE__, __TIME__);
    while(1){
        printf(">>> ");
        fgets(buffer, BUFFER_SIZE, stdin);
        parse_args(buffer, args, ARR_SIZE, &nargs);

        if (nargs == 0)
			continue;

        if ((!strcmp(args[0], "exit")) || (!strcmp(args[0], "quit"))) {
			detach_core();
			return 0;
        } else if (!strcmp(args[0], "help")) usage();
        else if (!strcmp(args[0], "license")) {
			int c;
			FILE *file;
			file = fopen("LICENSE", "r");
			if (file) {
				while ((c = getc(file)) != EOF)
					putchar(c);
				fclose(file);
			}
		} else if (!strcmp(args[0], "test")) {
			printf("Command '%s' args %ld\n", args[0], nargs);
			test(args);
		} else if (!strcmp(args[0], "meta")) {
			if (nargs<2) {
				printf("To few parameters for '%s'\n", args[0]);
				continue;
			}
			int rtn = debugkey(args[1]);
			if (rtn<0) {
				puts("No QUID/data found");
			}
		} else if (!strcmp(args[0], "store")) {
			if (nargs<2) {
				printf("To few parameters for '%s'\n", args[0]);
				continue;
			}
			char squid[39] = {'\0'};
			int rtn = store(squid, args[1], strlen(args[1]));
			if (rtn<0) {
				puts("Error: store failed");
			} else {
				puts(squid);
			}
		} else if (!strcmp(args[0], "request")) {
			if (nargs<2) {
				printf("To few parameters for '%s'\n", args[0]);
				continue;
			}
			size_t len;
			char *data = request(args[1], &len);
			if (data==NULL) {
				puts("No QUID/data found");
			} else{
				data[len] = '\0';
				puts(data);
				free(data);
			}
		} else if (!strcmp(args[0], "delete")) {
			if (nargs<2) {
				printf("To few parameters for '%s'\n", args[0]);
				continue;
			}
			int rtn = delete(args[1]);
			if (rtn<0) {
				puts("Error: delete failed");
			}
		} else if (!strcmp(args[0], "stats")) {
			debugstats();
		} else if (!strcmp(args[0], "vacuum")) {
			if(vacuum()<0)
				printf("Vacuum failed");
		} else if (!strcmp(args[0], "update")) {
			if (nargs<2) {
				printf("To few parameters for '%s'\n", args[0]);
				continue;
			}
			int i;
			struct microdata md;
			printf("lifecycle (0-31):");
			scanf ("%d", &i);
			md.lifecycle = i;
			printf("importance (0-10):");
			scanf ("%d", &i);
			md.importance = i;
			printf("syslock (0-1):");
			scanf ("%d", &i);
			md.syslock = i;
			printf("exec (0-1):");
			scanf ("%d", &i);
			md.exec = i;
			printf("freeze (0-1):");
			scanf ("%d", &i);
			md.freeze = i;
			printf("error (0-1):");
			scanf ("%d", &i);
			md.error = i;
			printf("flag (0-7):");
			scanf ("%d", &i);
			md.flag = i;
			int rtn = update(args[1], &md);
			if (rtn<0) {
				puts("Error: update failed");
			}
        } else printf("Unknown command '%s'\n", args[0]);
    }
    return 0;
}
