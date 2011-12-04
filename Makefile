CFLAGS = -Wall -g3

all:	agent parse-logs webstats

agent:	agent.c

parse-logs: parse-logs.c
	gcc -O3 -Wall parse-logs.c -o parse-logs -ldb -lz

webstats: webstats.c
	gcc -O3 -Wall webstats.c -o webstats -lgd -ldb

clean:
	rm -f agent parse-logs webstats TAGS core