CFLAGS = -Wall -g # -O3

# Enable GD for pie graphs
CFLAGS += -DHAVE_GD

ifneq ($(findstring HAVE_GD,$(CFLAGS)),)
LIBS += -lgd
endif

all:	libwebstats.a agent parse-logs webstats visits gigcal

agent:	agent.c
	gcc -O3 -Wall -o $@ $+

libwebstats.a: parse.o time.o is.o statsdb.o ignore.o urlcache.o
	@rm -f $@
	ar cr $@ $+

parse-logs: parse-logs.o libwebstats.a
	gcc -O3 -Wall -o $@ $+ -ldb -lz -lsamlib

gigcal: gigcal.o libwebstats.a
	gcc -O3 -Wall -o $@ $+ -ldb -lz -lsamlib

webstats: webstats.o libwebstats.a
	gcc -O3 -Wall -o $@ $+ $(LIBS) -ldb -lz -lsamlib

visits: visits.o libwebstats.a
	gcc -O3 -Wall -o $@ $+ -ldb -lz -lsamlib

clean:
	rm -f *.o agent parse-logs visits webstats libwebstats.a TAGS core
