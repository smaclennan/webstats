CFLAGS = -O3 -Wall

agent:	agent.c

all:	agent

clean:
	rm -f agent TAGS core