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

int main(int argc, const char *argv[]) {
	FILE *fd = NULL;

	if (argc < 2) {
		fprintf(stderr, "%s [FILE]\n", argv[0]);
		return 1;
	}
	if ((fd = fopen(argv[1], "r+")) == NULL) {
		perror("Cannot open file\n");
		return 1;
	}

	// obtain file size
	fseek(fd, 0, SEEK_END);
	long int psz = ftell(fd);
	rewind(fd);

	// allocate memory to contain the whole file:
	char *buff = (char*)malloc(sizeof(char) * psz);
	if (!buff) {
		fputs("Memory error", stderr);
		fclose(fd);
		return 1;
	}

	// check LF and add one if needed
	fread(buff, 1, psz, fd);
	if (buff[psz - 1] != '\n') {
		fflush(fd);
		fwrite("\n", 1, 1, fd);
	}

	free(buff);
	fclose(fd);
	return 0;
}
