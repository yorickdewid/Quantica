INCLUDE=include
SRCDIR=src
TESTDIR=test
BINDIR=bin
UTILDIR=util
VALGRIND=valgrind
VALFLAGS=--leak-check=full --track-origins=yes --show-reachable=yes
CFLAGS=-c -g -O0 -pedantic -std=c99 -Wall -Werror -Wextra -DDEBUG -DX64 -DTN12
LDFLAGS= -lm
SOURCES=$(SRCDIR)/common.c \
		$(SRCDIR)/time.c \
		$(SRCDIR)/log.c \
		$(SRCDIR)/zmalloc.c \
		$(SRCDIR)/arc4random.c \
		$(SRCDIR)/strlcpy.c \
		$(SRCDIR)/strlcat.c \
		$(SRCDIR)/itoa.c \
		$(SRCDIR)/strdup.c \
		$(SRCDIR)/stresc.c \
		$(SRCDIR)/strsep.c \
		$(SRCDIR)/strtoken.c \
		$(SRCDIR)/quid.c \
		$(SRCDIR)/sha1.c \
		$(SRCDIR)/sha2.c \
		$(SRCDIR)/hmac.c \
		$(SRCDIR)/aes.c \
		$(SRCDIR)/base64.c \
		$(SRCDIR)/crc32.c \
		$(SRCDIR)/crc64.c \
		$(SRCDIR)/md5.c \
		$(SRCDIR)/vector.c \
		$(SRCDIR)/dict.c \
		$(SRCDIR)/stack.c \
		$(SRCDIR)/json_check.c \
		$(SRCDIR)/json_parse.c \
		$(SRCDIR)/json_encode.c \
		$(SRCDIR)/diagnose.c \
		$(SRCDIR)/dstype.c \
		$(SRCDIR)/slay.c \
		$(SRCDIR)/basecontrol.c \
		$(SRCDIR)/index.c \
		$(SRCDIR)/engine.c \
		$(SRCDIR)/core.c \
		$(SRCDIR)/hashtable.c \
		$(SRCDIR)/jenhash.c \
		$(SRCDIR)/bootstrap.c \
		$(SRCDIR)/webapi.c \
		$(SRCDIR)/sql.c
TEST_SOURCES=$(TESTDIR)/benchmark-engine.c \
		$(TESTDIR)/test-quid.c \
		$(TESTDIR)/test-aes.c \
		$(TESTDIR)/test-base64.c \
		$(TESTDIR)/test-crc32.c \
		$(TESTDIR)/test-sha1.c \
		$(TESTDIR)/test-sha2.c \
		$(TESTDIR)/test-md5.c \
		$(TESTDIR)/benchmark-quid.c \
		$(TESTDIR)/test-engine.c \
		$(TESTDIR)/test-bootstrap.c \
		$(TESTDIR)/test-json_check.c
OBJECTS=$(SOURCES:.c=.o) $(SRCDIR)/main.o
TESTOBJECTS=$(SOURCES:.c=.o) $(TEST_SOURCES:.c=.o) $(TESTDIR)/runner.o
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

fixeof:
	$(CC) $(UTILDIR)/lfeof.c -pedantic -std=c99 -Wall -Werror -Wextra -o $(BINDIR)/lfeof
	find . -type f -name *.[c\|h] -print -exec $(BINDIR)/lfeof {} \;

genquid:
	$(CC) -I$(INCLUDE) $(SRCDIR)/quid.c $(SRCDIR)/arc4random.c $(UTILDIR)/genquid.c -o $(BINDIR)/genquid

cleanall: clean cleandb cleandebug cleantest cleanutil

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

cleanutil:
	@rm -rf $(UTILDIR)/*.o
	@rm -rf $(BINDIR)/lfeof
	@rm -rf $(BINDIR)/genquid