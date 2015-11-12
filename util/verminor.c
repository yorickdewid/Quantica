/*
 * Copyright (c) 2015 Quantica, Quenza
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of Quenza nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define VERSION_DEFINITION	"VERSION_PATCH"
#define VERSION_INCREMENT	1

int main(int argc, const char *argv[]) {
	char buf[256];
	char _tmpfile[1024];
	FILE *fd = NULL;
	FILE *fd2 = NULL;
	int line_found = 0;
	char *vstr = NULL;

	if (argc < 2) {
		fprintf(stderr, "%s [FILE]\n", argv[0]);
		return 1;
	}
	if ((fd = fopen(argv[1], "r+")) == NULL) {
		perror("Cannot open file\n");
		return 1;
	}

	// create temp file
	strcpy(_tmpfile, argv[1]);
	strcat(_tmpfile, ".tmp");

	if ((fd2 = fopen(_tmpfile, "w+")) == NULL) {
		perror("Cannot open file\n");
		fclose(fd);
		return 1;
	}

	// locate version definition
	int found = 0;
	while (fgets(buf, sizeof(buf) - 10, fd)) {
		++line_found;
		char *pch = strtok(buf, " \t");
		while (pch != NULL) {
			if (found) {
				vstr = malloc(sizeof(pch) + 1);
				strcpy(vstr, pch);
				goto replace;
			}
			if (!strcmp(pch, VERSION_DEFINITION)) {
				found = 1;
			}
			pch = strtok(NULL, " \t");
		}
	}

replace:
	rewind(fd);

	// increase version number
	int ver = atoi(vstr);
	ver += VERSION_INCREMENT;
	fprintf(stdout, VERSION_DEFINITION " %d => %d\n", ver - VERSION_INCREMENT, ver);

	// write version to temp
	int lcnt = 0;
	while (fgets(buf, sizeof(buf) - 10, fd)) {
		if (++lcnt == line_found) {
			char *pch = strstr(buf, vstr);
			sprintf(pch, "%d\n", ver);
		}
		fputs(buf, fd2);
	}

	free(vstr);
	fclose(fd);
	fclose(fd2);

	// switch temp for original
	unlink(argv[1]);
	rename(_tmpfile, argv[1]);

	return 0;
}
