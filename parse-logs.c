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
#define DOMAINS
#define PAGES
 */
#ifdef DOMAINS
DB *domains;
#endif
#ifdef PAGES
DB *pages;
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
	    strcmp(host, "ftp.seanm.ca") == 0 ||
	    strstr(host, "emacs.seanm.ca"))
		return 0;

	return 1;
}
#endif

static void process_log(struct log *log)
{
#ifdef DOMAINS
	db_put_count(domains, log->host);
#endif
#ifdef PAGES
	db_put_count(domains, log->url);
#endif
}

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
	puts("Pages:");
	db_walk(pages, print_count);
	db_close("pages", pages);
#endif

	return 0;
}

