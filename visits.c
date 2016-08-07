#include "webstats.h"

int verbose;
static int clickthru;
static char *host;
static struct tm *yesterday;

static int bots;
static int total_hits;

struct url {
	const char *url;
	int count;
	struct url *next;
};

static struct visit {
	char *ip;
	time_t last_visit;
	int count;
	int good;
	int bot;
	struct url *urls, *utail;
	struct visit *next;
} *visits;

/* Returns 0 on not a visit, 1 on new visit, 2 on visit hit */
int is_visit(struct log *log, int clickthru)
{
	if (strcmp(log->method, "GET") && strcmp(log->method, "HEAD"))
		return 0;

	/* Must check before setting time */
	if (clickthru && isdefault(log))
		return 0;

	if (isbot(log))
		return 0;

	return 1;
}

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
	int good = 0;
	int bot = 0;

	/* favicon.ico does not count in good status */
	if (valid_status(log->status) && strcmp(log->url, "/favicon.ico"))
		good = 1;

	if (strcmp(log->url, "/robots.txt") == 0)
		bot = 1;

	for (v = visits; v; v = v->next)
		if (strcmp(v->ip, log->ip) == 0) {
			if (abs(v->last_visit - log->time) < VISIT_TIMEOUT) {
				v->last_visit = log->time;
				++v->count;
				if (good)
					++v->good;
				if (bot)
					v->bot = 1;
				add_url(v, log);
				return;
			} else
				break; /* new visit */
		}

	v = calloc(1, sizeof(struct visit));
	if (!v || (v->ip = strdup(log->ip)) == NULL) {
		puts("Out of memory");
		exit(1);
	}

	v->next = visits;
	visits = v;
	++v->count;
	v->last_visit = log->time;

	/* favicon.ico does not count in good status */
	if (good)
		++v->good;
	v->bot = bot;

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

	++total_hits;

	if (isbot(log)) {
		++bots;
		return;
	}

	if (is_visit(log, clickthru))
		add_visit(log);
}

int main(int argc, char *argv[])
{
	int i;

	while ((i = getopt(argc, argv, "h:ci:vy")) != EOF)
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

	if (optind == argc)
		parse_logfile(NULL, process_log);
	else
		for (i = optind; i < argc; ++i) {
			if (verbose)
				printf("Parsing %s...\n", argv[i]);
			parse_logfile(argv[i], process_log);
		}

	struct visit *v;
	struct url *u;
	int n_visits = 0;
	int visit_hits = 0;
	int s404 = 0;

	for (v = visits; v; v = v->next) {
		if (v->bot)
			bots += v->count;
		else
			if (v->good) {
			printf("%-16s %d %d\n", v->ip, v->count, v->good);
			for (u = v->urls; u; u = u->next)
				printf("    %-30s %d\n", u->url, u->count);
			++n_visits;
			visit_hits += v->count;
		} else
			s404 += v->count;
	}

#define percent(a) ((double)(a) * 100.0 / (double)total_hits)
	printf("Visits %d visit hits %d (%.0f%%) 404 %d (%.0f%%) bots %d (%.0f%%)\n",
		   n_visits, visit_hits, percent(visit_hits), s404, percent(s404),
		   bots, percent(bots));

	if (total_hits - visit_hits - s404 - bots)
		puts("Total problems\n");

	return 0;
}
