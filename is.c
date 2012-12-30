#define _GNU_SOURCE /* for strcasestr */
#include "webstats.h"

#define VISIT_TIMEOUT (30 * 60) /* 30 minutes in seconds */

int isbot(char *who)
{
	if (strcasestr(who, "bot") ||
	    strcasestr(who, "spider") ||
	    strcasestr(who, "crawl") ||
	    strcasestr(who, "link")) {
		if (verbose > 1)
			puts(who);
		return 1;
	}

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

int ispage(struct log *log)
{
	char *p;

	if (log->status != 200)
		return 0;

	/* Asking for default page is good */
	if (isdefault(log))
		return 1;

	/* Check the extension */
	p = strrchr(log->url, '.');
	if (!p)
		return 0;

	if (strncmp(p, ".htm", 4) == 0)
		return 1;

	if (strcmp(p, ".js") == 0)
		return 1;

	return 0;
}

int isvisit(struct log *log, DB *ipdb, int clickthru)
{
	time_t lasttime;

	if (log->status != 200)
		return 0;

	if (strcmp(log->method, "GET"))
		return 0;

	if (!ispage(log))
		return 0;

	/* Must check before setting time */
	if (clickthru && isdefault(log))
		return 0;

	if (isbot(log->who))
		return 0;

	if (!ipdb)
		return 1; /* success */

	if (db_put_data(ipdb, log->ip, &log->time, sizeof(log->time), DB_NOOVERWRITE) == 0)
		return 1; /* success - ip not in db */

	if (db_get_data(ipdb, log->ip, &lasttime, sizeof(lasttime)) != sizeof(lasttime)) {
		puts("DB get failed!");
		return 0;
	}

	/* Note: time goes forwards through one file, but backwards through the files */
	if (abs(log->time - lasttime) < VISIT_TIMEOUT)
		return 0;

	/* Update db with new time */
	db_put_data(ipdb, log->ip, &log->time, sizeof(log->time), 0);

	return 1;
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
