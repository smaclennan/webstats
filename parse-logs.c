#define _GNU_SOURCE /* for strcasestr */
#include "webstats.h"


int verbose;

static char default_host[40];

static int total_hits;
static unsigned total_size;

static int default_hits;
static unsigned default_size;

static struct tm *yesterday;

/* counts */
static DB *counts;
static int total_count;

/* domains */
static void *domains;

/* pages */
static int page_type;
#define PAGE_COUNTS 1
#define PAGE_SIZES 2
static DB *pages;
static int max_url;
static int total_pages;

/* daily */
static DB *ddb;
static DB *ipdb;

static int bots;

static int print_count(char *key, void *data, int len)
{
	printf("%s %lu\n", key, *(unsigned long *)data);
	return 0;
}

static void process_log(struct log *log)
{
	int isabot;

	if (ignore_ip(log->ip))
		return;

	if (!in_range(log))
		return;

	if (yesterday && !time_equal(yesterday, log->tm))
		return;

	++total_hits;
	total_size += log->size;

	if (isdefault(log)) {
		++default_hits;
		default_size += log->size;
	}

	if (isbot(log->who)) {
		++bots;
		isabot = 1;
	}

	if (ddb) {
		char timestr[16];

		snprintf(timestr, sizeof(timestr), "%04d/%02d/%02d-%03d",
			 log->tm->tm_year + 1900, log->tm->tm_mon, log->tm->tm_mday,
			 log->tm->tm_yday);

		db_update_count(ddb, timestr, log->size);
	}

	if (ipdb && !isabot)
		db_put(ipdb, log->ip, NULL, 0, 0);

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

		++total_pages;

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
		if (strcmp(host, "-") == 0)
			host = default_host;

		int len = snprintf(url, sizeof(url), "%s%s", host, log->url);
		if (len > max_url)
			max_url = len;

		if (page_type == PAGE_SIZES)
			db_update_count(pages, url, log->size);
		else
			db_update_count(pages, url, 1);
	}
}

static struct list {
	char *name;
	unsigned long size;
} *top;
static int max_top = 10;
static int n_top;

static void setup_sort(void)
{
	int i;

	top = calloc(max_top, sizeof(struct list));
	if (!top) {
		puts("Out of memory\n");
		exit(1);
	}

	printf("max_url %d\n", max_url);
	++max_url;
	for (i = 0; i < max_top; ++i) {
		top[i].name = calloc(1, max_url);
		if (!top[i].name) {
			printf("Out of memory\n");
			exit(1);
		}
	}
}

static int sort_pages(char *key, void *data, int len)
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
			if (n_top < max_top)
				++n_top;
			return 0;
		}

	if (n_top < max_top) {
		strcpy(top[n_top].name, key);
		top[n_top].size = size;
		++n_top;
	}

	return 0;
}

#define m(n)   (((double)(n)) / 1024.0 / 1024.0)
#define k(n)   (((double)(n)) / 1024.0)

int print_daily(char *key, void *data, int len)
{
	unsigned long size = *(unsigned long *)data;
	printf("%s %6lu\n", key, (unsigned long)m(size));
	return 0;
}

static void usage(char *prog, int rc)
{
	char *p = strrchr(prog, '/');
	if (p)
		prog = p + 1;

	printf("usage: %s [-cdhvD] [-p{c|s}[n]] [-r range] [logfile ...]\nwhere:"
	       "\t-c enable counts\n"
	       "\t-d enable domains\n"
	       "\t-h help\n"
	       "\t-pc[n] enable top N page counts\n"
	       "\t-ps[n] enable top N page sizes\n"
	       "\t-v verbose\n"
	       "\t-D enable dailies\n"
	       "Note: range is time in days\n",
	       prog);

	exit(rc);
}

static int print_ip(char *key, void *data, int len)
{
	puts(key);
	return 0;
}

int main(int argc, char *argv[])
{
	int i, yarg = 0;

	while ((i = getopt(argc, argv, "cdhi:p:r:yvDI")) != EOF)
		switch (i) {
		case 'c':
			counts = stats_db_open("counts.db");
			if (!counts) {
				printf("Unable to open counts db\n");
				exit(1);
			}
			break;
		case 'd':
			domains = stats_db_open("domains.db");
			if (!domains) {
				printf("Unable to open pages db\n");
				exit(1);
			}
			break;
		case 'h':
			usage(argv[0], 0);
		case 'i':
			add_ip_ignore(optarg);
			break;
		case 'p':
			if (*optarg == 'c')
				page_type = PAGE_COUNTS;
			else if (*optarg == 's')
				page_type = PAGE_SIZES;
			else
				usage(argv[1], 1);
			max_top = strtol(optarg + 1, NULL, 10);
			if (max_top == 0) max_top = 10;

			pages = stats_db_open("pages.db");
			if (!pages) {
				printf("Unable to open pages db\n");
				exit(1);
			}
			break;
		case 'r':
			init_range(strtol(optarg, NULL, 10));
			break;
		case 'y':
			++yarg;
			break;
		case 'v':
			++verbose;
			break;
		case 'D':
			ddb = stats_db_open("daily.db");
			if (!ddb) {
				printf("Unable to open daily db\n");
				exit(1);
			}
			break;
		case 'I':
			ipdb = stats_db_open("ips.db");
			if (!ipdb) {
				printf("Unable to open ip db\n");
				exit(1);
			}
			break;
		default:
			puts("Sorry!");
			usage(argv[0], 1);
		}

	if (yarg)
		yesterday = calc_yesterdays(yarg);

	if (!get_default_host(default_host, sizeof(default_host)))
		strcpy(default_host, "seanm.ca");

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
		stats_db_close(ddb, "daily.db");
	}

	if (domains) {
		puts("Domains:");
		db_walk(domains, print_count);
		stats_db_close(domains, "domains.db");
	}

	if (pages) {
		setup_sort();
		db_walk(pages, sort_pages);

		if (page_type == PAGE_SIZES) {
			double total = 0.0;

			for (i = 0; i < n_top; ++i) {
				double size = m(top[i].size);
				total += size;
				printf("%-60s %.1f\n", top[i].name, size);
			}

			printf("Total %.1f\n", total);
		} else {
			unsigned long total = 0;

			for (i = 0; i < n_top; ++i) {
				total += top[i].size;
				printf("%-60s %lu\n", top[i].name, top[i].size);
			}

			printf("Total %ld/%u\n", total, total_pages);
		}

		stats_db_close(pages, "pages.db");
	}

	if (counts) {
		puts("Counts:");
		db_walk(counts, print_count);
		stats_db_close(counts, "counts");
		printf("Total: %d\n", total_count);
	}

	if (ipdb) {
		puts("IPs:");
		db_walk(ipdb, print_ip);
		stats_db_close(counts, "ips.db");
	}

	printf("Hits: %d/%d\n", default_hits, total_hits);
	printf("Size: %.1f/%.1f\n", k(default_size), k(total_size));

//	printf("Bots %d/%d %.1f%%\n", bots, total_hits, (double)bots / (double)total_hits * 100.0);

	return 0;
}
