#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#include <zlib.h>

static int lineno;
static int max;

static int verbose;
static int quiet;

static time_t min_date = 0x7fffffff, max_date;

static char *outdir;
static char *outfile;


static int parse_date(struct tm *tm, char *month);

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
static int ispage(char *url)
{
	char fname[4096], *p;
	int len;

	if (sscanf(url, "GET %s HTTP/1.", fname) != 1)
		return 0;

	/* Asking for default page is good */
	len = strlen(fname);
	if (len == 0)
		return 0;
	if (*(fname + len - 1) == '/')
		return 1;

	/* Check the extension */
	p = strrchr(fname, '.');
	if (!p)
		return 0;

	if (strncmp(p, ".htm", 4) == 0)
		return 1;

	if (strcmp(p, ".js") == 0)
		return 1;

	return 0;
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
	char how[12];
	int len, http;
	gzFile fp = my_fopen(logfile);

	while (my_gets(line, sizeof(line), fp)) {
		char ip[20], host[20], month[8];
		int status;
		unsigned long size;
		struct tm tm;

		++lineno;
		len = strlen(line);
		if (len > max) {
			max = len;
			if (len == sizeof(line) - 1) {
				printf("PROBLEMS 0\n");
				my_gets(line, sizeof(line), fp);
				continue;
			}
		}

		memset(&tm, 0, sizeof(tm));
		if (sscanf(line,
			   "%s %s - [%d/%[^/]/%d:%d:%d:%d %*d] "
			   "%10s %s HTTP/1.%d\" %d %lu \"%[^\"]\" \"%[^\"]\"",
			   ip, host,
			   &tm.tm_mday, month, &tm.tm_year,
			   &tm.tm_hour, &tm.tm_min, &tm.tm_sec,
			   how, url, &http, &status, &size, refer, who) != 15) {
			if (!quiet)
				printf("%d: Error %s", lineno, line);
			continue;
		}
		if (*how != '"') {
			if (!quiet)
				printf("%d: Error %s", lineno, line);
			continue;
		}
		memmove(how, how + 1, 10);

		parse_date(&tm, month);

		if (strncmp(ip, "192.168.", 8) == 0)
			continue;
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

	if (optind == argc)
		parse_logfile(NULL);
	else
		for (i = optind; i < argc; ++i) {
			if (verbose)
				printf("Parsing %s...\n", argv[i]);
			parse_logfile(argv[i]);
		}


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

/*
 * Local Variables:
 * compile-command: "gcc -O3 -Wall parse-logs.c -o parse-logs -ldb -lz"
 * End:
 */
