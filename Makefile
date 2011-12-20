CFLAGS = -Wall -g3

all:	agent parse-logs webstats

agent:	agent.c

parse-logs: parse-logs.c statsdb.o
	gcc -O3 -Wall $+ -o parse-logs -ldb -lz

webstats: webstats.c statsdb.o
	gcc -O3 -Wall $+ -o webstats -lgd -ldb

clean:
	rm -f agent parse-logs webstats TAGS core