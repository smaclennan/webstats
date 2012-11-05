#define _GNU_SOURCE /* for strcasestr */
#include "webstats.h"

#define VISIT_TIMEOUT (30 * 60) /* 30 minutes in seconds */

int isbrowser(char *who)
{
#if 1
	if (strcasestr(who, "bot") ||
	    strcasestr(who, "spider") ||
	    strcasestr(who, "crawl") ||
	    strcasestr(who, "link")) {
		if (verbose > 1)
			puts(who);
		return 0;
	}
#endif

	return 1;
}

int ispage(struct log *log)
{
	char *p;
	int len;

	if (log->status != 200)
		return 0;

	/* Asking for default page is good */
	len = strlen(log->url);
	if (len == 0)
		return 1;
	if (*(log->url + len - 1) == '/')
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

int isvisit(struct log *log, DB *ipdb)
{
	time_t lasttime;

	if (log->status != 200)
		return 0;

	if (strstr(log->url, "robot.txt")) {
		if (verbose)
			printf("Blacklist %s\n", log->ip);
		db_put_data(ipdb, log->ip, &log->time, sizeof(log->time), 0);
		return 0;
	}

	if (!ispage(log))
		return 0;

	if (!isbrowser(log->who))
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
