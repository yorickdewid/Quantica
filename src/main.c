#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "core.h"

#define BUFFER_SIZE 1<<16
#define ARR_SIZE 1<<16

void usage() {
	puts("help\t\t\tthis help menu");
	puts("license\t\t\tshow software license");
	puts("store <data>\t\tstore data in the database");
	puts("request <quid>\t\tretrieve data by key");
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
			while(nargs--)
				puts(args[nargs]);
		} else if (!strcmp(args[0], "store")) {
			if (nargs<2) {
				printf("To few parameters for '%s'\n", args[0]);
				continue;
			}
			char squid[35] = {'\0'};
			store(squid, args[1], strlen(args[1]));
			puts(squid);
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
        } else printf("Unknown command '%s'\n", args[0]);
    }
    return 0;
}
