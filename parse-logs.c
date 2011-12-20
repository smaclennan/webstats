#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#include <zlib.h>

#include "statsdb.h"

static int lineno;
static int max;

static int verbose;
static int quiet;

static time_t min_date = 0x7fffffff, max_date;

static char *outdir;
static char *outfile;

static int parse_date(struct tm *tm, char *month);

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

static inline FILE *my_fopen(char *logfile)
{
	if (logfile) {
		FILE *fp = gzopen(logfile, "rb");
		if (!fp) {
			perror(logfile);
			exit(1);
		}
		return fp;
	} else
		return stdin;
}

static inline char *my_gets(char *line, int size, FILE *fp)
{
	if (fp == stdin)
		return fgets(line, size, fp);
	else
		return gzgets(fp, line, size);
}

static inline void my_fclose(FILE *fp)
{
	if (fp != stdin)
		gzclose(fp);
}

static void parse_logfile(char *logfile)
{
	char line[4096], url[4096], refer[4096], who[4096];
	int len;
	gzFile fp = gzopen(logfile, "rb");
	if (!fp) {
		perror(logfile);
		exit(1);
	}

	while (gzgets(fp, line, sizeof(line))) {
		char ip[20], host[20], month[8], sstr[20], method[20];
		char *s, *e;
		int n, where;
		int status;
		unsigned long size;
		struct tm tm;

		++lineno;
		len = strlen(line);
		if (len > max) {
			max = len;
			if (len == sizeof(line) - 1) {
				printf("PROBLEMS 0\n");
				gzgets(fp, line, sizeof(line));
				continue;
			}
		}

		memset(&tm, 0, sizeof(tm));
		n = sscanf(line,
			   "%s %s - [%d/%[^/]/%d:%d:%d:%d %*d] "
			   "\"%s %s HTTP/1.%*d\" %d %s \"%n",
			   ip, host,
			   &tm.tm_mday, month, &tm.tm_year,
			   &tm.tm_hour, &tm.tm_min, &tm.tm_sec,
			   method, url, &status, sstr, &where);

		if (n == 10) {
			/* sscanf \"%[^\"]\" cannot handle an empty string. */
			*url = '\0';
			if (sscanf(line,
				   "%s %s - [%d/%[^/]/%d:%d:%d:%d %*d] "
				   "\"\" %d %s \"%n",
				   ip, host,
				   &tm.tm_mday, month, &tm.tm_year,
				   &tm.tm_hour, &tm.tm_min, &tm.tm_sec,
				   &status, sstr, &where) != 10) {
				printf("%d: Error [8] %s", lineno, line);
				continue;
			}
		} else if (n != 12) {
			printf("%d: Error [%d] %s", lineno, n, line);
			continue;
		}

		/* This handles a '-' in the size field */
		size = strtol(sstr, NULL, 10);

		/* People seem to like to embed quotes in the refer
		 * and who strings :( */
		s = line + where;
		e = strchr(s, '"');
		while (e && *(e + 1) != ' ')
			e = strchr(e + 1, '"');
		if (!e) {
			printf("%d: Error %s", lineno, line);
			continue;
		}

		*e = '\0';
		snprintf(refer, sizeof(refer), "%s", s);

		/* Warning the who will contains the quotes. */
		snprintf(who, sizeof(who), "%s", e + 2);

		/* Don't count local access. */
		if (strncmp(ip, "192.168.", 8) == 0)
			continue;

		parse_date(&tm, month);

#ifdef DOMAINS
		db_put(domains, host);
#endif
#ifdef PAGES
		db_put_count(pages, url);
#endif
	}

	my_fclose(fp);
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
		parse_logfile(NULL);
	else
		for (i = optind; i < argc; ++i) {
			if (verbose)
				printf("Parsing %s...\n", argv[i]);
			parse_logfile(argv[i]);
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

static char *months[12] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};


static int parse_date(struct tm *tm, char *month)
{
	time_t this;

	tm->tm_year -= 1900;

	for (tm->tm_mon = 0; tm->tm_mon < 12; ++tm->tm_mon)
		if (strcmp(months[tm->tm_mon], month) == 0) {
			this = mktime(tm);
			if (this == (time_t)-1) {
				perror("mktime");
				exit(1);
			}
			if (this > max_date)
				max_date = this;
			if (this < min_date)
				min_date = this;
			return 0; /* success */
		}

	printf("BAD MONTH %s\n", month);
	return 1;
}
