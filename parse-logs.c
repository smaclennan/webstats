#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#include <zlib.h>

#include "webstats.h"


static int verbose;
static int quiet;

static char *outdir;
static char *outfile;

/*
#define PAGES
#define DOMAINS
 */
#ifdef DOMAINS
DB *domains;
#endif
#ifdef PAGES
DB *pages;
static int max_url;
static double total = 0.0;
#endif

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

#if 0
/* Probably only of use to me ;) */
static int is_seanm_ca(char *host)
{
	if (strstr(host, "rippers.ca") ||
	    strstr(host, "m38a1.ca") ||
	    strstr(host, "ftp.seanm.ca") ||
	    strstr(host, "git.seanm.ca") ||
	    strstr(host, "emacs.seanm.ca"))
		return 0;

	return 1;
}
#endif

static void process_log(struct log *log)
{
#ifdef DOMAINS
	db_update_count(domains, log->host, 1);
#endif
#ifdef PAGES
	if (log->status == 200) { /* only worry about real files */
		int len = strlen(log->url);
		char *p = strstr(log->url, "index.htm");
		if (p)
			*p = '\0';

		if (len > max_url)
			max_url = len;

		db_update_count(pages, log->url, log->size);
	}
#endif
}

#ifdef PAGES
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
#endif

#define m(n)   (((double)(n)) / 1024.0 / 1024.0)

int main(int argc, char *argv[])
{
	int i;

	while ((i = getopt(argc, argv, "d:o:qv")) != EOF)
		switch (i) {
		case 'd':
			outdir = optarg;
			break;
		case 'o':
			outfile = optarg;
			break;
		case 'q':
			quiet = 1;
			break;
		case 'v':
			++verbose;
			break;
		default:
			puts("Sorry!");
			exit(1);
		}

#ifdef DOMAINS
	domains = db_open("domains");
	if (!domains)
		exit(1);
#endif
#ifdef PAGES
	pages = db_open("pages");
	if (!pages)
		exit(1);
#endif

	if (optind == argc)
		parse_logfile(NULL, process_log);
	else
		for (i = optind; i < argc; ++i) {
			if (verbose)
				printf("Parsing %s...\n", argv[i]);
			parse_logfile(argv[i], process_log);
		}

#ifdef DOMAINS
	puts("Domains:");
	db_walk(domains, print);
	db_close("domains", domains);
#endif
#ifdef PAGES
	setup_sort();
	db_walk(pages, sort_pages);

	for (i = 0; i < n_top; ++i) {
		double size = m(top[i].size);
		total += size;
		printf("%-60s %.1f\n", top[i].name, size);
	}

	printf("Total %.1f\n", total);
#endif

	return 0;
}
