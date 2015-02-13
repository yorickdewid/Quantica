INCLUDE=include
SRCDIR=src
TESTDIR=test
BINDIR=bin
CFLAGS=-c -g -Wall -Wextra -D_LARGEFILE64_SOURCE -DDEBUG
LDFLAGS=-lrt
SOURCES=$(SRCDIR)/bswap.c $(SRCDIR)/quid.c $(SRCDIR)/engine.c $(SRCDIR)/core.c $(TESTDIR)/test-benchmark.c
OBJECTS=$(SOURCES:.c=.o) $(SRCDIR)/main.o
TESTOBJECTS=$(SOURCES:.c=.o) $(TESTDIR)/test.o
EXECUTABLE=quantica
EXECUTABLETEST=quantica_test

debug: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $(BINDIR)/$@

test: $(EXECUTABLETEST)

$(EXECUTABLETEST): $(TESTOBJECTS)
	$(CC) $(LDFLAGS) $(TESTOBJECTS) -o $(BINDIR)/$@

.c.o:
	$(CC) -I$(INCLUDE) $(CFLAGS) $< -o $@

cleanall: clean cleandb cleandebug cleantest

clean:
	@rm -rf $(SRCDIR)/*.o
	@rm -rf $(TESTDIR)/*.o

cleandb:
	@rm -rf $(BINDIR)/*.db
	@rm -rf $(BINDIR)/*.idx
	@rm -rf $(BINDIR)/*.log

cleandebug: clean
	@rm -rf $(BINDIR)/$(EXECUTABLE)

cleantest: clean cleandb
	@rm -rf $(BINDIR)/$(EXECUTABLETEST)
