#include "webstats.h"


static struct ignore {
	char ip[16];
	struct ignore *next;
} *ignores;


int ignore_ip(char *ip)
{
	struct ignore *ignore;

	if (strncmp(ip, "192.168.", 8) == 0)
		return 1;

	for (ignore = ignores; ignore; ignore = ignore->next)
		if (strcmp(ip, ignore->ip) == 0)
			return 1;

	return 0;
}

void add_ip_ignore(char *ip)
{
	static struct ignore *tail;
	struct ignore *new = calloc(1, sizeof(struct ignore));
	if (!new) {
		printf("Out of memory\n");
		exit(1);
	}
	snprintf(new->ip, sizeof(new->ip), "%s", ip);
	if (ignores)
		tail->next = new;
	else
		ignores = new;
	tail = new;
}
