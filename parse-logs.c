#define _GNU_SOURCE /* for strcasestr */
#include "webstats.h"


static int verbose;

/* counts */
static DB *counts;
static int total_count;

/* domains */
static DB *domains;

/* pages */
static DB *pages;
static int max_url;
static double total = 0.0;

/* daily */
static DB *ddb;


#if 0
static int isabot(char *who)
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
#endif

#if 0
static int is_page(char *url)
{
	char *p;

	p = url + strlen(url);
	if (p > url)
		--p;
	if (*p == '/')
		return 1;

	p = strstr(url, "index.html");
	if (p) {
		*p = '\0';
		return 1;
	}

	/* Check the extension */
	p = strrchr(url, '.');
	if (!p)
		return 0;

	if (strncmp(p, ".htm", 4) == 0)
		return 1;

	if (strcmp(p, ".js") == 0)
		return 1;

	return 0;
}
#endif

/* Probably only of use to me ;) */
static int is_seanm_ca(char *host)
{
	if (strstr(host, "rippers.ca") || strstr(host, "m38a1.ca"))
		return 0;

	return 1;
}

static void process_log(struct log *log)
{
	if (!in_range(log))
		return;

	if (ddb) {
		char timestr[16];

		snprintf(timestr, sizeof(timestr), "%04d/%02d/%02d-%03d",
			 log->tm->tm_year + 1900, log->tm->tm_mon, log->tm->tm_mday,
			 log->tm->tm_yday);

		db_update_count(ddb, timestr, log->size);
	}

	if (counts) {
		db_update_count(counts, log->url, 1);
		++total_count;
	}

	if (domains)
		db_update_count(domains, log->host, 1);

	if (pages && log->status == 200) { /* only worry about real files */
		char url[256], *host;
		char *p = strstr(log->url, "index.htm");
		if (p)
			*p = '\0';

#if 0
		/* By directory */
		p = log->url;
		if (*p == '/')
			++p;
		p = strchr(p, '/');
		if (p)
			*p = '\0';
		else
			strcpy(log->url, "/");
#endif

		host = log->host;
		if (is_seanm_ca(host))
			host = "seanm.ca";

		int len = snprintf(url, sizeof(url), "%s%s", host, log->url);
		if (len > max_url)
			max_url = len;

		db_update_count(pages, url, log->size);
	}
}

#define TEN 10
static struct list {
	char *name;
	unsigned long size;
} top[TEN];
int n_top;

static void setup_sort(void)
{
	int i;

	printf("max_url %d\n", max_url);
	++max_url;
	for (i = 0; i < TEN; ++i) {
		top[i].name = calloc(1, max_url);
		if (!top[i].name) {
			printf("Out of memory\n");
			exit(1);
		}
	}
}

static void sort_pages(char *key, void *data, int len)
{
	int i, j;
	unsigned long size = *(unsigned long *)data;

	for (i = 0; i < n_top; ++i)
		if (size > top[i].size) {
			for (j = n_top - 1; j > i; --j) {
				strcpy(top[j].name, top[j - 1].name);
				top[j].size = top[j - 1].size;
			}
			strcpy(top[i].name, key);
			top[i].size = size;
			if (n_top < TEN)
				++n_top;
			return;
		}

	if (n_top < TEN) {
		strcpy(top[n_top].name, key);
		top[n_top].size = size;
		++n_top;
	}
}

#define m(n)   (((double)(n)) / 1024.0 / 1024.0)

void print_daily(char *key, void *data, int len)
{
	unsigned long size = *(unsigned long *)data;
	printf("%s %6lu\n", key, (unsigned long)m(size));
}

static void usage(char *prog, int rc)
{
	char *p = strrchr(prog, '/');
	if (p)
		prog = p + 1;

	printf("usage: %s [-cdhpvD] [-r range] [logfile ...]\nwhere:"
	       "\t-c enable counts\n"
	       "\t-d enable domains\n"
	       "\t-h help\n"
	       "\t-p enable pages\n"
	       "\t-v verbose\n"
	       "\t-D enable dailies\n",
	       prog);

	exit(rc);
}

int main(int argc, char *argv[])
{
	int i;

	while ((i = getopt(argc, argv, "cdhpr:vD")) != EOF)
		switch (i) {
		case 'c':
			counts = db_open("counts.db");
			if (!counts) {
				printf("Unable to open counts db\n");
				exit(1);
			}
			break;
		case 'd':
			domains = db_open("domains.db");
			if (!domains) {
				printf("Unable to open pages db\n");
				exit(1);
			}
			break;
		case 'h':
			usage(argv[0], 0);
		case 'p':
			pages = db_open("pages.db");
			if (!pages) {
				printf("Unable to open pages db\n");
				exit(1);
			}
			break;
		case 'r':
			init_range(strtol(optarg, NULL, 10));
			break;
		case 'v':
			++verbose;
			break;
		case 'D':
			ddb = db_open("daily.db");
			if (!ddb) {
				printf("Unable to open daily db\n");
				exit(1);
			}
			break;
		default:
			puts("Sorry!");
			usage(argv[0], 1);
		}

	if (optind == argc)
		parse_logfile(NULL, process_log);
	else
		for (i = optind; i < argc; ++i) {
			if (verbose)
				printf("Parsing %s...\n", argv[i]);
			parse_logfile(argv[i], process_log);
		}

	range_fixup();

	if (ddb) {
		db_walk(ddb, print_daily);
		db_close(ddb, "daily.db");
	}

	if (domains) {
		puts("Domains:");
		db_walk(domains, print);
		db_close(domains, "domains.db");
	}

	if (pages) {
		setup_sort();
		db_walk(pages, sort_pages);

		for (i = 0; i < n_top; ++i) {
			double size = m(top[i].size);
			total += size;
			printf("%-60s %.1f\n", top[i].name, size);
		}

		printf("Total %.1f\n", total);

		db_close(pages, "pages.db");
	}

	if (counts) {
		puts("Counts:");
		db_walk(counts, print_count);
		db_close(counts, "counts");
		printf("Total: %d\n", total_count);
	}

	return 0;
}
