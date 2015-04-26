INCLUDE=include
SRCDIR=src
TESTDIR=test
BINDIR=bin
VALGRIND=valgrind
VALFLAGS=--leak-check=full --track-origins=yes --show-reachable=yes
CFLAGS=-c -g -O0 -pedantic -std=c99 -Wall -Werror -Wextra -DDEBUG -DX64 -DTN12
LDFLAGS= -lm
SOURCES=$(SRCDIR)/common.c $(SRCDIR)/time.c $(SRCDIR)/log.c $(SRCDIR)/zmalloc.c $(SRCDIR)/strlcpy.c $(SRCDIR)/strlcat.c $(SRCDIR)/itoa.c $(SRCDIR)/strdup.c $(SRCDIR)/stresc.c $(SRCDIR)/quid.c $(SRCDIR)/sha1.c \
        $(SRCDIR)/aes.c $(SRCDIR)/base64.c $(SRCDIR)/crc32.c $(SRCDIR)/md5.c $(SRCDIR)/sha256.c $(SRCDIR)/json_check.c $(SRCDIR)/dstype.c $(SRCDIR)/slay.c $(SRCDIR)/engine.c $(SRCDIR)/core.c $(SRCDIR)/hashtable.c \
        $(SRCDIR)/bootstrap.c $(SRCDIR)/webapi.c $(TESTDIR)/benchmark-engine.c $(TESTDIR)/test-quid.c $(TESTDIR)/test-aes.c \
        $(TESTDIR)/test-base64.c $(TESTDIR)/test-crc32.c $(TESTDIR)/test-sha1.c $(TESTDIR)/test-md5.c $(TESTDIR)/test-sha256.c $(TESTDIR)/benchmark-quid.c \
        $(TESTDIR)/test-engine.c $(TESTDIR)/test-bootstrap.c $(TESTDIR)/test-json_check.c
OBJECTS=$(SOURCES:.c=.o) $(SRCDIR)/main.o
TESTOBJECTS=$(SOURCES:.c=.o) $(TESTDIR)/runner.o
EXECUTABLE=quantica
EXECUTABLETEST=quantica_test

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
	CFLAGS += -DLINUX
	LDFLAGS += -lrt
endif
ifeq ($(UNAME_S),Darwin)
	CFLAGS += -DOSX
endif
ifeq ($(UNAME_S),OpenBSD)
	CFLAGS += -DOBSD
endif

debug: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $(BINDIR)/$@

test: $(EXECUTABLETEST)

$(EXECUTABLETEST): $(TESTOBJECTS)
	$(CC) $(TESTOBJECTS) $(LDFLAGS) -o $(BINDIR)/$@

.c.o:
	$(CC) -I$(INCLUDE) $(CFLAGS) $< -o $@

memcheck: debug
	cd $(BINDIR) && $(VALGRIND) $(VALFLAGS) ./$(EXECUTABLE) -f

cleanall: clean cleandb cleandebug cleantest

clean:
	@rm -rf $(SRCDIR)/*.o
	@rm -rf $(TESTDIR)/*.o

cleandb:
	@rm -rf $(BINDIR)/*.db* $(BINDIR)/*.idx* $(BINDIR)/*.log*
	@rm -rf $(BINDIR)/*._db $(BINDIR)/*._idx $(BINDIR)/*._log

cleandebug: clean
	@rm -rf $(BINDIR)/$(EXECUTABLE)

cleantest: clean
	@rm -rf $(BINDIR)/$(EXECUTABLETEST)
