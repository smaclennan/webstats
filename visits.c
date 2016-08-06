#include "webstats.h"

int verbose;
static int clickthru;
static int not;
static char *host;
static struct tm *yesterday;
static DB *ipdb;

struct url {
	const char *url;
	int count;
	struct url *next;
};

static struct visit {
	char *ip;
	int count;
	struct url *urls, *utail;
	struct visit *next;
} *visits;

static void add_url(struct visit *v, struct log *log)
{
	struct url *u;

	for (u = v->urls; u; u = u->next)
		if (strcmp(u->url, log->url) == 0) {
			++u->count;
			return;
		}

	u = calloc(1, sizeof(struct url));
	if (!u) {
		puts("Out of memory");
		exit(1);
	}

	u->url = urlcache_get(log->url);
	++u->count;

	if (v->urls)
		v->utail->next = u;
	else
		v->urls = u;
	v->utail = u;
}

static void add_visit(struct log *log)
{
	struct visit *v;

	for (v = visits; v; v = v->next)
		if (strcmp(v->ip, log->ip) == 0) {
			++v->count;
			add_url(v, log);
			return;
		}

	v = calloc(1, sizeof(struct visit));
	if (!v || (v->ip = strdup(log->ip)) == NULL) {
		puts("Out of memory");
		exit(1);
	}

	v->next = visits;
	visits = v;
	++v->count;

	add_url(v, log);
}

static void process_log(struct log *log)
{
	if (ignore_ip(log->ip))
		return;

	if (host && strstr(log->host, host) == NULL)
		return;

	if (yesterday && !time_equal(yesterday, log->tm))
		return;

	if (isbot(log))
		return;

	if (not) {
		if (!isvisit(log, ipdb, clickthru))
			fputs(log->line, stdout);
	} else if (isvisit(log, ipdb, clickthru))
		add_visit(log);
}

int main(int argc, char *argv[])
{
	int i;

	while ((i = getopt(argc, argv, "h:ci:nvy")) != EOF)
		switch (i) {
		case 'h':
			host = optarg;
			break;
		case 'c':
			clickthru = 1;
			break;
		case 'i':
			add_ip_ignore(optarg);
			break;
		case 'n':
			++not;
			break;
		case 'y':
			yesterday = calc_yesterday();
			break;
		case 'v':
			++verbose;
			break;
		default:
			puts("Sorry!");
			exit(1);
		}

	ipdb = stats_db_open("ipdb");
	if (!ipdb) {
		printf("Unable to open ip db\n");
		exit(1);
	}

	if (optind == argc)
		parse_logfile(NULL, process_log);
	else
		for (i = optind; i < argc; ++i) {
			if (verbose)
				printf("Parsing %s...\n", argv[i]);
			parse_logfile(argv[i], process_log);
		}

	if (ipdb)
		stats_db_close(ipdb, "ipdb");

	if (!not) {
		struct visit *v;
		struct url *u;

		for (v = visits; v; v = v->next) {
			printf("%-16s %d\n", v->ip, v->count);
			for (u = v->urls; u; u = u->next)
				printf("    %-30s %d\n", u->url, u->count);
		}
	}

	return 0;
}
