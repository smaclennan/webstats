#define _GNU_SOURCE /* for strcasestr */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>

#include <zlib.h>

#include <gd.h>
#include <gdfontmb.h>
#include <gdfonts.h>

#define ENABLE_VISITS
#ifdef ENABLE_VISITS
#include "statsdb.h"
#include <arpa/inet.h>

#define WIDTH 642
#else
#define WIDTH 422

#define DB void
static inline DB *db_open(char *fname) { return fname; }
static inline int db_put(DB *db, char *ip) { return 1; }
static inline void db_close(char *fname, DB *db) {}
#endif

static int lineno;
static int max;

static struct site {
	char *name;
	int color;
	int hits;
	unsigned long size;
	unsigned long arc;
	DB *ipdb;
	unsigned long visits;
} sites[] = {
	{ "seanm.ca", 0xff0000 }, /* must be first! */
	{ "rippers.ca", 0x0000ff },
	{ "ftp.seanm.ca", 0xffa500 },
	{ "emacs", 0x00ff00 },
};
static int n_sites = sizeof(sites) / sizeof(struct site);

struct list {
	char *name;
	struct list *next;
};

static int verbose;

static struct list *includes;
static struct list *others;

static unsigned long total_hits;
static unsigned long total_size;
static unsigned long total_visits;

static time_t min_date = 0x7fffffff, max_date;

static char *outdir;
static char *outfile = "stats.html";
static char *outgraph = "pie.gif";


static int parse_date(struct tm *tm, char *month);
static char *cur_time(time_t now);
static char *cur_date(time_t now);
static int days(void);


/* filename mallocs space, you should free it */
static char *filename(char *fname, char *ext)
{
	static char *out;
	int len = strlen(fname) + 1;

	if (outdir)
		len += strlen(outdir) + 1;
	if (ext)
		len += strlen(ext);

	out = malloc(len);
	if (!out) {
		printf("Out of memory\n");
		exit(1);
	}

	if (outdir)
		sprintf(out, "%s/%s", outdir, fname);
	else
		strcpy(out, fname);

	if (ext) {
		char *p = strrchr(out, '.');
		if (p)
			*p = '\0';
		strcat(out, ext);
	}

	return out;
}

static void out_header(FILE *fp, char *title)
{
	/* Header proper */
	fprintf(fp,
		"<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\">\n"
		"<html lang=\"en\">\n"
		"<head>\n"
		"  <title>%s</title>\n"
		"  <meta http-equiv=\"Content-type\" content=\"text/html;charset=utf-8\">\n"
		"  <style type=\"text/css\"> <!-- body { margin: 0 10%%; } --> </style>\n"
		"</head>\n", title);

	/* Body */
	fprintf(fp, "<body BGCOLOR=\"#E8E8E8\">\n");
	fprintf(fp, "<h2>%s</h2>\n", title);
	fprintf(fp, "<small><strong>\n");
	/* Warning: cur_time/date has a local static for buffer */
	fprintf(fp, "Summary Period: %s", cur_date(min_date));
	fprintf(fp, " to %s (%d days)<br>\n", cur_date(max_date), days());
	fprintf(fp, "Generated %s<br>\n", cur_time(time(NULL)));
	fprintf(fp, "</strong></small>\n<hr>\n");
	fprintf(fp, "<center>\n\n");
}

static void out_trailer(FILE *fp)
{
	/* trailer */
	fprintf(fp, "\n</body>\n</html>\n");
}

static void add_include(char *fname, FILE *out)
{
	char line[4096], *p;
	int state = 0;
	FILE *in = fopen(fname, "r");
	if (!in) {
		perror(fname);
		return;
	}

	fprintf(out, "\n<!-- included %s -->\n", fname);

	while (fgets(line, sizeof(line), in))
		switch (state) {
		case 0: /* looking for body tag */
			p = strstr(line, "<body");
			if (p) {
				p = strchr(p, '>');
				if (p) {
					state = 2;
					for (++p; isspace(*p); ++p)
						;
					if (p)
						fputs(p, out);
				} else
					state = 1;
			}
			break;
		case 1: /* looking for body '>' */
			p = strchr(line, '>');
			if (p) {
				state = 2;
				for (++p; isspace(*p); ++p)
					;
				if (p)
					fputs(line, out);
			}
			break;
		case 2: /* looking for </body> */
			p = strstr(line, "</body>");
			if (p) {
				state = 3;
				*p = '\0';
				for (p = line; isspace(*p); ++p)
					;
				if (*p)
					fputs(line, out);
			} else
				fputs(line, out);
			break;
		case 3: /* waiting for EOF ;) */
			break;
		}

	fclose(in);
}

static void out_html(char *fname)
{
	int i;
	FILE *fp = fopen(fname, "w");
	if (!fp) {
		perror(fname);
		return;
	}

	out_header(fp, "Statistics for YOW");

	fprintf(fp, "<p><img src=\"pie.gif\" width=%d height=235 alt=\"Pie Charts\">\n\n",
		WIDTH);

	fprintf(fp, "<p><table WIDTH=\"80%%\" BORDER=2 CELLSPACING=1 CELLPADDING=1");
	fprintf(fp, " summary=\"Satistics.\">\n");

	fputs("<tr><th>Site"
	      "<th colspan=2>Hits"
	      "<th colspan=2>Size (M)\n"
#ifdef ENABLE_VISITS
	      "<th colspan=2>Visits\n"
#endif
	      , fp);

	for (i = 0; i < n_sites; ++i) {
		if (sites[i].hits == 0)
			continue;
		fprintf(fp, "<tr><td>%s"
			"<td align=right>%d<td align=right>%.1f%%"
			"<td align=right>%ld<td align=right>%.1f%%"
#ifdef ENABLE_VISITS
			"<td align=right>%ld<td align=right>%.1f%%"
#endif
			"%s", sites[i].name,
			sites[i].hits,
			(double)sites[i].hits * 100.0 / (double)total_hits,
			sites[i].size / 1024,
			(double)sites[i].size * 100.0 / (double)total_size,
#ifdef ENABLE_VISITS
			sites[i].visits,
			(double)sites[i].visits * 100.0 / (double)total_visits,
#endif
			"\n");
	}

	fprintf(fp, "<tr><td>Totals<td align=right>%ld<td>&nbsp;"
		"<td align=right>%ld<td>&nbsp;\n"
#ifdef ENABLE_VISITS
		"<td align=right>%ld<td>&nbsp;\n"
#endif
		, total_hits, total_size / 1024
#ifdef ENABLE_VISITS
		, total_visits
#endif
		);

	fprintf(fp, "</table>\n</center>\n");

	while (includes) {
		add_include(includes->name, fp);
		includes = includes->next;
	}

	out_trailer(fp);

	fclose(fp);
}

static void out_hr(FILE *fp)
{
	fputs("-----------------------------------------------------", fp);
#ifdef ENABLE_VISITS
	fputs("----------------", fp);
#endif
	fputs("\n", fp);
}

static void out_txt(char *fname)
{
	int i;
	FILE *fp = fopen(fname, "w");
	if (!fp) {
		perror(fname);
		return;
	}

	fprintf(fp, "Statistics for YOW\n");
	fprintf(fp, "Summary Period: %s", cur_date(min_date));
	fprintf(fp, " to %s (%d days)\n", cur_date(max_date), days());
	fprintf(fp, "Generated %s\n\n", cur_time(time(NULL)));

#ifdef ENABLE_VISITS
	fputs("Site\t\t\t Hits\t\t     Size\t    Visits\n", fp);
#else
	fputs("Site\t\t\t Hits\t\t     Size\n", fp);
#endif
	out_hr(fp);

	for (i = 0; i < n_sites; ++i) {
		if (sites[i].hits == 0)
			continue;
		fprintf(fp, "%-20s"
			"%6d  %3.1f%%"
			"\t%6ld  %3.1f%%"
#ifdef ENABLE_VISITS
			"\t%6ld  %3.1f%%"
#endif
			"\n", sites[i].name,
			sites[i].hits,
			(double)sites[i].hits * 100.0 / (double)total_hits,
			sites[i].size / 1024,
			(double)sites[i].size * 100.0 / (double)total_size
#ifdef ENABLE_VISITS
			,
			sites[i].visits,
			(double)sites[i].visits * 100.0 / (double)total_visits
#endif
			);
	}

	out_hr(fp);
	fprintf(fp, "%-20s%6ld      \t"
		"%6ld      \t"
#ifdef ENABLE_VISITS
		"%6ld"
#endif
		"\n", "Totals", total_hits, total_size / 1024
#ifdef ENABLE_VISITS
		, total_visits
#endif
		);

	fclose(fp);
}

static void out_gopher(char *fname)
{
	int i;
	FILE *fp = fopen(fname, "w");
	if (!fp) {
		perror(fname);
		return;
	}

	out_header(fp, "Statistics for YOW gopher");

	fprintf(fp, "<p><table WIDTH=\"80%%\" BORDER=2 CELLSPACING=1 CELLPADDING=1");
	fprintf(fp, " summary=\"Satistics.\">\n");

	fputs("<tr><th>Site"
	      "<th>Hits"
	      "<th>Size (M)\n"
#ifdef ENABLE_VISITS
	      "<th>Visits\n"
#endif
	      , fp);

	for (i = 0; i < n_sites; ++i) {
		fprintf(fp, "<tr><td>%s"
			"<td align=right>%d"
			"<td align=right>%ld"
#ifdef ENABLE_VISITS
			"<td align=right>%ld"
#endif
			"%s", sites[i].name,
			sites[i].hits,
			sites[i].size / 1024,
#ifdef ENABLE_VISITS
			sites[i].visits,
#endif
			"\n");
	}

	fprintf(fp, "<tr><td>Totals<td align=right>%ld"
		"<td align=right>%ld\n"
#ifdef ENABLE_VISITS
		"<td align=right>%ld\n"
#endif
		, total_hits, total_size / 1024
#ifdef ENABLE_VISITS
		, total_visits
#endif
		);

	fprintf(fp, "</table>\n");

	out_trailer(fp);

	fclose(fp);
}

static int getcolor(gdImagePtr im, int color)
{
	return gdImageColorAllocate(im,
				    (color >> 16) & 0xff,
				    (color >> 8) & 0xff,
				    color & 0xff);
}

static void draw_pie(gdImagePtr im, int cx, int cy, int size)
{
	int color;
	int i, s = 0, e;

	for (i = 0; i < n_sites; ++i) {
		color = getcolor(im, sites[i].color);
		/* convert percent to arc */
		if (sites[i].arc == 0)
			continue;
		e = s + sites[i].arc;
		gdImageFilledArc(im, cx, cy, size, size, s, e, color, gdArc);
		s = e;
	}
}

static void out_graphs()
{
	FILE *fp;
	char *fname;
	int i, tarc, color;
	int x;

	gdImagePtr im = gdImageCreate(WIDTH, 235);
	color = gdImageColorAllocate(im, 0xff, 0xff, 0xff); /* background */
	gdImageColorTransparent(im, color);

	color = gdImageColorAllocate(im, 0, 0, 0); /* text */

	gdImageString(im, gdFontMediumBold, 87, 203,
		      (unsigned char *)"Hits", color);
	gdImageString(im, gdFontMediumBold, 305, 203,
		      (unsigned char *)"Bytes", color);
#ifdef ENABLE_VISITS
	gdImageString(im, gdFontMediumBold, 522, 203,
		      (unsigned char *)"Visits", color);
#endif

	x = 35;
	for (i = 0; i < n_sites; ++i) {
		color = getcolor(im, sites[i].color);
		gdImageString(im, gdFontSmall, x, 220,
			      (unsigned char *)sites[i].name, color);
		x += 100;
	}

	for (tarc = i = 0; i < n_sites; ++i) {
		sites[i].arc = sites[i].hits * 360 / total_hits;
		tarc += sites[i].arc;

#if 0
		printf("%s %d %ld%% %ld\n",
		       sites[i].name,
		       sites[i].hits,
		       sites[i].hits * 100 / total_hits,
		       sites[i].arc);
#endif
	}

	/* Compensate the first arc */
	sites[0].arc += 360 - tarc;

	draw_pie(im, 100, 100, 198);

	/* Calculate the size arcs */
	for (tarc = i = 0; i < n_sites; ++i) {
		sites[i].arc = sites[i].size * 360 / total_size;
		tarc += sites[i].arc;

#if 0
		printf("%s %ld %ld%% %ld\n",
		       sites[i].name,
		       sites[i].size,
		       sites[i].size * 100 / total_size,
		       sites[i].arc);
#endif
	}

	/* Compensate the first arc */
	sites[0].arc += 360 - tarc;

	draw_pie(im, 320, 100, 198);

#ifdef ENABLE_VISITS
	/* Calculate the visit arcs */
	for (tarc = i = 0; i < n_sites; ++i) {
		sites[i].arc = sites[i].visits * 360 / total_visits;
		tarc += sites[i].arc;

#if 0
		printf("%s %ld %ld%% %ld\n",
		       sites[i].name,
		       sites[i].size,
		       sites[i].size * 100 / total_size,
		       sites[i].arc);
#endif
	}

	/* Compensate the first arc */
	sites[0].arc += 360 - tarc;

	draw_pie(im, 540, 100, 198);
#endif

	/* Save to file. */
	fname = filename(outgraph, NULL);
	fp = fopen(fname, "wb");
	if (!fp) {
		perror(fname);
		exit(1);
	}
	gdImageGif(im, fp);
	fclose(fp);

	/* Destroy it */
	gdImageDestroy(im);
}

static void add_list(char *name, struct list **head)
{
	struct list *l = calloc(1, sizeof(struct list));
	if (!l) {
		puts("Out of memory");
		exit(1);
	}
	l->name = strdup(name);
	if (!l->name) {
		puts("Out of memory");
		exit(1);
	}
	l->next = *head;
	*head = l;
}

static void add_others(char *name)
{
	add_list(name, &others);
}

static int isbrowser(char *who)
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

static void update_site(int i, unsigned long size, int status,
			char *ip, char *url, char *who, char *refer)
{
	++sites[i].hits;
	sites[i].size += size;

	if (status == 200 && ispage(url) && isbrowser(who)) {
		if (db_put(sites[i].ipdb, ip) == 0) {
			++sites[i].visits;
			if (verbose)
				printf("%s: %s\n", sites[i].name, ip);
		}
	}
}

static void parse_logfile(char *logfile)
{
	char line[4096], url[4096], refer[4096], who[4096];
	struct list *l;
	int len, i;
	gzFile fp = gzopen(logfile, "rb");
	if (!fp) {
		perror(logfile);
		exit(1);
	}

	while (gzgets(fp, line, sizeof(line))) {
		char ip[20], host[20], month[8], sstr[20];
#ifndef SIMPLE
		char *s, *e;
		int n, where;
#endif
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
#ifdef SIMPLE
		if (sscanf(line,
			   "%s %s - [%d/%[^/]/%d:%d:%d:%d %*d] "
			   "\"%[^\"]\" %d %lu \"%[^\"]\" \"%[^\"]\"",
			   ip, host,
			   &tm.tm_mday, month, &tm.tm_year,
			   &tm.tm_hour, &tm.tm_min, &tm.tm_sec,
			   url, &status, &size, refer, who) != 13) {
			printf("%d: Error %s", lineno, line);
			continue;
		}
#else
		n = sscanf(line,
			   "%s %s - [%d/%[^/]/%d:%d:%d:%d %*d] "
			   "\"%[^\"]\" %d %s \"%n",
			   ip, host,
			   &tm.tm_mday, month, &tm.tm_year,
			   &tm.tm_hour, &tm.tm_min, &tm.tm_sec,
			   url, &status, sstr, &where);

		if (n == 8) {
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
		} else if (n != 11) {
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
#endif

		/* Don't count local access. */
		if (strncmp(ip, "192.168.", 8) == 0)
			continue;

		parse_date(&tm, month);

		/* Unqualified lines */
		if (*host == '-') {
#if 0
			puts(line);
#endif
			update_site(0, size, status, ip, url, who, refer);
			continue;
		}

		for (i = 1; i < n_sites; ++i)
			if (strstr(host, sites[i].name)) {
				update_site(i, size, status, ip, url, who, refer);
				break;
			}

		if (i < n_sites)
			continue; /* matched */

		if (strcmp(host, "yow") == 0 ||
		    strcmp(host, "localhost") == 0 ||
		    strcmp(host, "192.168.0.1") == 0 ||
		    strcmp(host, "127.0.0.1") == 0)
			continue; /* don't count locals */

		/* lighttpd defaults to seanm.ca for everything else */
		update_site(0, size, status, ip, url, who, refer);

#if 1
		if (strstr(host, "seanm.ca") == NULL) {
			for (l = others; l; l = l->next)
				if (strcmp(l->name, host) == 0)
					break;

			if (!l) {
				add_others(host);
				puts(host);
			}
		}
#endif
	}

	gzclose(fp);
}

/* Gopher logfile is more limited. Plus, there is no concept of
 * virtual sites. */
static void parse_gopher_log(char *logfile)
{
	char line[4096], url[4096];
	int len, site;
	gzFile fp = gzopen(logfile, "rb");
	if (!fp) {
		perror(logfile);
		exit(1);
	}

	while (gzgets(fp, line, sizeof(line))) {
		char ip[20], month[8];
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
		if (sscanf(line,
			   "%s - - [%d/%[^/]/%d:%d:%d:%d %*d] "
			   "\"%[^\"]\" %d %lu",
			   ip,
			   &tm.tm_mday, month, &tm.tm_year,
			   &tm.tm_hour, &tm.tm_min, &tm.tm_sec,
			   url, &status, &size) != 10) {
			printf("%d: Error %s", lineno, line);
			continue;
		}

		/* Don't count local access. */
		if (strncmp(ip, "192.168.", 8) == 0)
			continue;

		parse_date(&tm, month);

		site = strstr(url, "HTTP/1.") == NULL;

		++sites[site].hits;
		sites[site].size += size;

		if (status == 200 && db_put(sites[site].ipdb, ip) == 0) {
			++sites[site].visits;
			if (verbose)
				printf("visit %s\n", ip);
		}
	}

	gzclose(fp);
}

int main(int argc, char *argv[])
{
	int i, gopher = 0;

	while ((i = getopt(argc, argv, "d:g:o:vGI:")) != EOF)
		switch (i) {
		case 'd':
			outdir = optarg;
			break;
		case 'g':
			outgraph = optarg;
			break;
		case 'o':
			outfile = optarg;
			break;
		case 'v':
			++verbose;
			break;
		case 'G':
			gopher = 1;
			n_sites = 2;
			sites[0].name = "Gopher";
			sites[1].name = "HTTP";
			break;
		case 'I':
			add_list(optarg, &includes);
			break;
		default:
			puts("Sorry!");
			exit(1);
		}

	if (optind == argc) {
		puts("I need a logfile to parse!");
		exit(1);
	}

	/* preload some know others */
	add_others("seanm.dyndns.org");
	add_others("216.138.233.67");
	add_others("toronto-hs-216-138-233-67.s-ip.magma.ca");
	add_others("m38a1.ca");

	for (i = 0; i < n_sites; ++i) {
		sites[i].ipdb = db_open(sites[i].name);
		if (!sites[i].ipdb) {
			printf("Unable to open db\n");
			exit(1);
		}
	}

	for (i = optind; i < argc; ++i) {
		if (verbose)
			printf("Parsing %s...\n", argv[i]);
		if (gopher)
			parse_gopher_log(argv[i]);
		else
			parse_logfile(argv[i]);
	}

	for (i = 0; i < n_sites; ++i)
		db_close(sites[i].name, sites[i].ipdb);

	/* Calculate the totals */
	for (i = 0; i < n_sites; ++i) {
		total_hits += sites[i].hits;
		total_visits += sites[i].visits;
		sites[i].size /= 1024; /* convert to k */
		total_size += sites[i].size;
	}
	/* Make sure there are no /0 errors */
	if (total_hits == 0)
		total_hits = 1;
	if (total_visits == 0)
		total_visits = 1;
	if (total_size == 0)
		total_size = 1;

	if (gopher)
		out_gopher(filename(outfile, NULL));
	else {
		out_graphs();
		out_html(filename(outfile, NULL));
	}

	out_txt(filename(outfile, ".txt"));

	printf("Max line %d\n", max); // SAM DBG

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

static char *cur_time(time_t now)
{
	static char timestamp[32];

	/* convert to timestamp string */
	strftime(timestamp, sizeof(timestamp),
		 "%b %d %Y %H:%M %Z", localtime(&now));

	return timestamp;
}

static char *cur_date(time_t now)
{
	static char timestamp[32];

	/* convert to timestamp string */
	strftime(timestamp, sizeof(timestamp), "%b %d %Y", localtime(&now));

	return timestamp;
}

static int days(void)
{
	return ((max_date - min_date) / 60 / 60 + 23) / 24;
}
