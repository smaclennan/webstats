#define _GNU_SOURCE /* for strcasestr */
#include "webstats.h"

#define VISIT_TIMEOUT (30 * 60) /* 30 minutes in seconds */

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

/* Usually you will want to call isbot first.
 * Nowhere near complete... just catches the big ones.
 */
int isbrowser(char *who)
{
	if (*who == '"') ++who;
	if (strncmp(who, "Mozilla", 7) == 0 ||
	    strncmp(who, "Opera", 5) == 0 ||
	    strncmp(who, "Safari", 6) == 0)
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

static int valid_status(int status)
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

static int ispage(struct log *log)
{
	char *p;

	/* Asking for default page is good */
	if (isdefault(log))
		return 1;

	/* Check the extension */
	p = strrchr(log->url, '.');
	if (!p)
		return 0;

	if (strncmp(p, ".htm", 4) == 0)
		return 1;

	return 0;
}

/* Returns 0 on not a visit, 1 on new visit, 2 on visit hit */
int isvisit(struct log *log, DB *ipdb, int clickthru)
{
	time_t lasttime;

	if (!valid_status(log->status))
		return 0;

	if (strcmp(log->method, "GET") && strcmp(log->method, "HEAD"))
		return 0;

	/* Must check before setting time */
	if (clickthru && isdefault(log))
		return 0;

	if (isbot(log))
		return 0;

	if (!ipdb)
		return ispage(log);

	int rc = 2; /* visit hit */

	if (db_put(ipdb, log->ip, &log->time, sizeof(log->time), DB_NOOVERWRITE) == 0)
		rc = 1; /* new visit */

	if (db_get(ipdb, log->ip, &lasttime, sizeof(lasttime)) != sizeof(lasttime)) {
		printf("DB get failed! %s %s\n", log->ip, log->url);
		return 0;
	}

	/* Note: time goes forwards through one file, but backwards through the files */
	if (abs(log->time - lasttime) < VISIT_TIMEOUT)
		/* Update db with new time */
		db_put(ipdb, log->ip, &log->time, sizeof(log->time), 0);
	else
		rc = 1; /* new visit */

	return rc;
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
