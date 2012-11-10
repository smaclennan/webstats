CFLAGS = -Wall -g # -O3

all:	libwebstats.a agent parse-logs webstats

agent:	agent.c

libwebstats.a: parse.o time.o is.o statsdb.o
	@rm -f $@
	ar cr $@ $+

parse-logs: parse-logs.o libwebstats.a
	gcc -O3 -Wall -o $@ $+ -ldb -lz

webstats: webstats.o libwebstats.a
	gcc -O3 -Wall -o $@ $+ -lgd -ldb -lz

clean:
	rm -f *.o agent parse-logs webstats libwebstats.a TAGS core
