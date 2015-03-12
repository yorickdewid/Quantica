INCLUDE=include
SRCDIR=src
TESTDIR=test
BINDIR=bin
CFLAGS=-c -g -Wall -Werror -Wextra -DDEBUG
LDFLAGS=-lrt
SOURCES=$(SRCDIR)/bswap.c $(SRCDIR)/common.c $(SRCDIR)/quid.c $(SRCDIR)/sha1.c $(SRCDIR)/aes.c \
        $(SRCDIR)/crc32.c $(SRCDIR)/engine.c $(SRCDIR)/core.c $(SRCDIR)/bootstrap.c \
        $(SRCDIR)/webapi.c $(TESTDIR)/benchmark-engine.c $(TESTDIR)/test-quid.c \
        $(TESTDIR)/benchmark-quid.c $(TESTDIR)/test-engine.c $(TESTDIR)/test-bootstrap.c
OBJECTS=$(SOURCES:.c=.o) $(SRCDIR)/main.o
TESTOBJECTS=$(SOURCES:.c=.o) $(TESTDIR)/runner.o
EXECUTABLE=quantica
EXECUTABLETEST=quantica_test

debug: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $(BINDIR)/$@

test: $(EXECUTABLETEST)

$(EXECUTABLETEST): $(TESTOBJECTS)
	$(CC) $(TESTOBJECTS) $(LDFLAGS) -o $(BINDIR)/$@

.c.o:
	$(CC) -I$(INCLUDE) $(CFLAGS) $< -o $@

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
