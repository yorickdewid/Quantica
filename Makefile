INCLUDE=include
SRCDIR=src
TESTDIR=test
BINDIR=bin
VALGRIND=valgrind
VALFLAGS=--leak-check=yes --track-origins=yes
CFLAGS=-c -g -std=c99 -Wall -Werror -Wextra -DDEBUG
LDFLAGS=-lpthread
SOURCES=$(SRCDIR)/common.c $(SRCDIR)/strlcpy.c $(SRCDIR)/strlcat.c $(SRCDIR)/quid.c $(SRCDIR)/sha1.c \
        $(SRCDIR)/aes.c $(SRCDIR)/base64.c $(SRCDIR)/crc32.c $(SRCDIR)/time.c $(SRCDIR)/engine.c $(SRCDIR)/core.c \
        $(SRCDIR)/bootstrap.c $(SRCDIR)/webapi.c $(TESTDIR)/benchmark-engine.c $(TESTDIR)/test-quid.c $(TESTDIR)/test-aes.c \
        $(TESTDIR)/test-base64.c $(TESTDIR)/test-crc32.c $(TESTDIR)/test-sha1.c $(TESTDIR)/benchmark-quid.c \
        $(TESTDIR)/test-engine.c $(TESTDIR)/test-bootstrap.c
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
	$(VALGRIND) $(VALFLAGS) $(BINDIR)/$(EXECUTABLE)

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
