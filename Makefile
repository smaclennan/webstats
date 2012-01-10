CFLAGS = -Wall -O3

all:	agent parse-logs webstats

agent:	agent.c

parse-logs: parse-logs.o parse.o time.o statsdb.o
	gcc -O3 -Wall -o $@ $+ -ldb -lz

webstats: webstats.o parse.o time.o statsdb.o
	gcc -O3 -Wall -o $@ $+ -lgd -ldb -lz

clean:
	rm -f *.o agent parse-logs webstats TAGS core
