#define _GNU_SOURCE /* for strcasestr */
#include "webstats.h"

char *botfile;
static char **botlist;
static int n_bots = -1;

static char *default_bots[] = { "bot", "spider", "crawl", "link", "slurp" };

static int add_bot(char *line, void *data)
{
	botlist = realloc(botlist, (n_bots + 1) * sizeof(char *));
	line = strdup(line);
	if (!botlist || !line)
		return 1;
	botlist[n_bots++] = line;
	return 0;
}

static void setup_botlist(void)
{
	if (botfile) {
		n_bots = 0;
		if (readfile(add_bot, NULL, botfile,
				RF_IGNORE_EMPTY | RF_IGNORE_COMMENTS))
			exit(1);
	} else {
		n_bots = 5;
		botlist = default_bots;
	}
}

/* We are also lumping some attacks into bots */
int isbot(struct log *log)
{
	if (n_bots == -1)
		setup_botlist();

	/* Bad request: count as bot */
	if (log->status == 400)
		return 1;

	if (strcmp(log->method, "POST") == 0)
		return 1;

	int i;
	for (i = 0; i < n_bots; ++i)
		if (strcasestr(log->who, botlist[i])) {
			if (verbose > 1)
				puts(log->who);
			return 1;
		}

	/* The problem with this is we don't catch the other hits by the bot */
	if (log->url && strcmp(log->url, "/robots.txt") == 0)
		return 1;

	/* Also count bogus lines as bots */
	if (strncmp(log->url, "UNKNOWN", 7) == 0)
		return 1;

	return 0;
}

int isdefault(struct log *log)
{	/* Asking for default page is good */
	int len = strlen(log->url);
	if (len == 0)
		return 1;
	if (*(log->url + len - 1) == '/')
		return 1;
	return 0;
}

int valid_status(int status)
{
	switch (status) {
	case 200:
	case 302:
	case 304:
		return 1;
	default:
		return 0;
	}
}

int get_default_host(char *host, int len)
{	/* lighttpd specific */
	char line[128], def[128];
	FILE *fp = fopen("/etc/lighttpd/lighttpd.conf", "r");
	if (!fp)
		return 0;

	while (fgets(line, sizeof(line), fp))
		if (sscanf(line,
			   "simple-vhost.default-host  = \"%[^\"]", def) == 1) {
			snprintf(host, len, "%s", def);
			fclose(fp);
			return 1;
		}

	fclose(fp);
	return 0;
}
