/*
 * agent - produce the agent output file used in seanm.ca
 * Copyright (C) 2002-2008  Sean MacLennan <seanm@seanm.ca>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#define _GNU_SOURCE /* for strcasestr */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>

#include <gd.h>
#include <gdfontmb.h>
#include <gdfonts.h>

/*
 * TODO - deal with timezone
 * TODO - downcase the name for matching
 * TODO - file for OS matching
 */


/* YOW - you probably want to change this */
//#define BASEDIR		"/var/www/seanm.ca/stats"
#define BASEDIR		"."

#define OS_STATS_FILE	"os.html"
#define UNKNOWN_FILE	"unknown-os.html"
#define UNKNOWN_BROWSERS	"unknown-browsers.html"
#define ALL_FILE	"all.html"


#ifdef THTTPD
static struct urlstr {
	char *str;
	int len;
} urllist[] = {
	{ "/m38a1.ca/", 0 }, { "/www.m38a1.ca/", 0 },
	{ "/seanm.ca/", 0 }, { "/www.seanm.ca/", 0 },
	{ "/xemacs.seanm.ca/", 0 },
	{ "/emacs.seanm.ca/", 0 },
	{ "/ftp.seanm.ca/", 0 },
	{ "/rippers.ca", 0 }, { "/www.rippers.ca/", 0 },
	{ "/seanm.dyndns.org/", 0 },
};
#define NUMURLS (sizeof(urllist) / sizeof(struct urlstr))
#endif

static char **bots;
static int nbots;

struct name_count {
	char *name;
	int hits;
	int files;
	int pages;
	int group;
	unsigned long arc; /* for graphs */
	int color;
};


/* As of 2010 this is about 4100 */
static struct name_count *agents;
static int n_agents;
static int max_agents;

/* As of 2010 this is about 15 */
static struct name_count *os;
static int n_os;
static int max_os;

/* As of 2010 this is about 90 */
static struct name_count *unknown_os;
static int n_unknown_os;
static int max_unknown_os;

static struct name_count *unknown_browsers;
static int n_unknown_browsers;
static int max_unknown_browsers;

#define WINDOZE		0
#define APPLE       1
#define UNIX		2
#define OTHER		3
static struct name_count groups[] = {
	{ .name = "Microsoft", .color = 0x0000ff },
	{ .name = "Apple", .color = 0xff0000 },
	{ .name = "Unix", .color = 0x008040 },
	{ .name = "Other", .color = 0xffff00 },
};
static int n_groups = (sizeof(groups) / sizeof(struct name_count));

#define OTHER_BROWSER		0
#define MSIE			1
#define NETSCAPE		2
#define BOTS			3
#define OPERA			4
#define SAFARI			5
#define CHROME			6
#define JAVA			7
#define EMPTY			8
static struct name_count browsers[] = {
	{ .name = "Everybody Else(tm)" },
	{ .name = "Internet Explorer" },
	{ .name = "Mozilla" },
	{ .name = "Bots" },
	{ .name = "Opera" },
	{ .name = "Safari" },
	{ .name = "Chrome" },
	{ .name = "Scripting" },
	{ .name = "Empty" },
};
#define N_BROWSERS (sizeof(browsers) / sizeof(struct name_count))

/* The bot index *after* sorting */
static int bot_index;
static int empty_index;
static int java_index;

static int totalhits;
static int totalfiles;
static int totalpages;

static int os_hits;
static int os_files;
static int os_pages;

static time_t min_date = 0x7fffffff, max_date;

static void badline(char *line, char *p, int n)
{
	printf("BAD LINE %d\n", n);
	printf("%s", line);
	if (p)
		printf("%s", p);
}

static void process_file(FILE *fp, char *fname);
static void add_agent(char *agent, int file, int page);
static int  parse_agent(struct name_count *agent);
static void sort_oses(void);
static char *cur_time(time_t now);
static int  parse_date(char *date);
static void addbot(char *bot);

static double percent_hits(int hits)
{
	return (double)hits * 100.0 / (double)totalhits;
}

static double percent_files(int files)
{
	return (double)files * 100.0 / (double)totalfiles;
}

static double percent_pages(int pages)
{
	return (double)pages * 100.0 / (double)totalpages;
}

static double percent_browser_hits(int hits)
{
	return (double)hits * 100.0 /
		(double)(totalhits -
				 browsers[bot_index].hits -
				 browsers[empty_index].hits -
				 browsers[java_index].hits);
}

static double percent_browser_files(int files)
{
	return (double)files * 100.0 /
		(double)(totalfiles -
				 browsers[bot_index].files -
				 browsers[empty_index].files -
				 browsers[java_index].files);
}

static double percent_browser_pages(int pages)
{
	return (double)pages * 100.0 /
		(double)(totalpages -
				 browsers[bot_index].pages -
				 browsers[empty_index].pages -
				 browsers[java_index].pages);
}

static double percent_os_hits(int hits)
{
	return (double)hits * 100.0 / (double)os_hits;
}

static double percent_os_files(int files)
{
	return (double)files * 100.0 / (double)os_files;
}

static double percent_os_pages(int pages)
{
	return (double)pages * 100.0 / (double)os_pages;
}

static void out_html();

static enum {
	COMPARE_HITS,
	COMPARE_FILES,
	COMPARE_PAGES,
} compare_type;


static void usage(void)
{
	printf("usage: agent [-b basedir] [-f | -h] log_file ...\n");
	exit(1);
}


static void init_urllist(void)
{
#ifdef THTTPD
	int i;

	for (i = 0; i < NUMURLS; ++i)
		urllist[i].len = strlen(urllist[i].str);
#endif
}


static void init_bots(char *botfile)
{
	char line[1024], *p;
	FILE *fp = fopen(botfile, "r");
	if (!fp) {
		perror(botfile);
		exit(1);
	}

	while (fgets(line, sizeof(line), fp)) {
		if (*line == '#')
			continue;
		p = strchr(line, '\n');
		if (p)
			*p = '\0';
		addbot(line);
	}

	fclose(fp);
}

static char *must_strdup(char *str)
{
	char *new = strdup(str);
	if (new)
		return new;

	printf("Out of memory strdup.\n");
	exit(1);
}

#ifdef ADD_GRAPHS
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
	int i, s, e;

	s = 0;
	for (i = 0; i < n_groups; ++i) {
		if (groups[i].arc == 0)
			continue;

		color = getcolor(im, groups[i].color);

		e = s + groups[i].arc;

		gdImageFilledArc(im, cx, cy, size, size,
						 s, e, color, gdArc);

		s = e;
	}
}

static void out_graphs(void)
{
	FILE *fp;
	char *fname;
	int i, tarc, color;
	int x;
	int width = 422;
	int offset = 35;

	gdImagePtr im = gdImageCreate(width, 235);
	color = gdImageColorAllocate(im, 0xff, 0xff, 0xff); /* background */
	gdImageColorTransparent(im, color);

	color = gdImageColorAllocate(im, 0, 0, 0); /* text */

	gdImageString(im, gdFontMediumBold, 87, 203,
			  (unsigned char *)"Microsoft", color);
	gdImageString(im, gdFontMediumBold, 305, 203,
			  (unsigned char *)"Apple", color);
	gdImageString(im, gdFontMediumBold, 522, 203,
				  (unsigned char *)"Unix", color);
	gdImageString(im, gdFontMediumBold, 522, 203,
				  (unsigned char *)"Other", color);

	x = offset;
	for (i = 0; i < n_groups; ++i)
		if (groups[i].pages) {
			color = getcolor(im, groups[i].color);
			gdImageString(im, gdFontSmall, x, 220,
					  (unsigned char *)groups[i].name, color);
			x += 100;
		}

	for (tarc = 0, i = n_groups - 1; i > 0; --i) {
		groups[i].arc = groups[i].pages * 360 / totalpages;
		tarc += groups[i].arc;
	}

	/* Compensate the first arc */
	groups[0].arc = 360 - tarc;

	draw_pie(im, 100, 100, 198);

	/* Save to file. */
	fname = "os.gif";
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
#else
void out_graphs(void) {}
#endif

int main(int argc, char *argv[])
{
	FILE *fp;
	int arg, c, i;
	char *basedir = BASEDIR;
	char *botfile = "./botfile";

	compare_type = COMPARE_HITS;

	while ((c = getopt(argc, argv, "b:c:fhp")) != -1)
		switch (c) {
		case 'b':
			basedir = optarg;
			break;
		case 'c':
			botfile = optarg;
			break;
		case 'f':
			compare_type = COMPARE_FILES;
			break;
		case 'h':
			compare_type = COMPARE_HITS;
			break;
		case 'p':
			compare_type = COMPARE_PAGES;
			break;
		default:
			usage();
		}

	if (optind == argc)
		usage();

	if (chdir(basedir)) {
		perror(basedir);
		exit(1);
	}

	init_urllist();

	init_bots(botfile);

	for (arg = optind; arg < argc; ++arg)
		if (strcmp(argv[arg], "-") == 0)
			process_file(stdin, "stdin");
		else {
			fp = fopen(argv[arg], "r");
			if (!fp)
				perror(argv[arg]);
			else {
				process_file(fp, argv[arg]);
				fclose(fp);
			}
		}

	for (i = 0; i < n_agents; ++i)
		parse_agent(&agents[i]);

	sort_oses();

	out_html();

	out_graphs();

	return 0;
}


static int greater(struct name_count *group, struct name_count *os)
{
	switch (compare_type) {
	case COMPARE_FILES:
		return group->files >= os->files;
	case COMPARE_PAGES:
		return group->pages >= os->pages;
	case COMPARE_HITS:
	default:
		return group->hits >= os->hits;
	}
}

static void out_html(void)
{
	FILE *fp;
	int i, n, cur_group = 0;
	int need_unknown = 0;

	fp = fopen(OS_STATS_FILE, "w");
	if (!fp) {
		perror(OS_STATS_FILE);
		exit(1);
	}

	/* header */
	fprintf(fp,
		"<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 "
		"Transitional//EN\">\n");
	fprintf(fp, "<html lang=\"en\">\n<head>\n");
	fprintf(fp, "<title>Agent Statistics for YOW</title>\n");
	fprintf(fp, "<link rel=\"stylesheet\" href=\"style-sheet\">\n");
	fprintf(fp, "</head>\n");
	fprintf(fp, "<body BGCOLOR=\"#E8E8E8\" TEXT=\"#000000\" "
		"LINK=\"#0000FF\" VLINK=\"#FF0000\">\n");
	fprintf(fp, "<h2>Agent Statistics for yow</h2>\n");
	fprintf(fp, "<small><strong>\n");
	/* Warning: cur_time has a local static for buffer */
	fprintf(fp, "Summary Period: %s", cur_time(min_date));
	fprintf(fp, " to %s<br>\n", cur_time(max_date));
	fprintf(fp, "Generated %s<br>\n", cur_time(time(NULL)));
	switch (compare_type) {
	case COMPARE_HITS:
		fprintf(fp, "Sorted by hits.<br>\n");
		break;
	case COMPARE_FILES:
		fprintf(fp, "Sorted by files.<br>\n");
		break;
	case COMPARE_PAGES:
		fprintf(fp, "Sorted by pages.<br>\n");
		break;
	}
	fprintf(fp, "</strong></small>\n<hr>\n");
	fprintf(fp, "<center>\n");

	fprintf(fp, "<p><table WIDTH=\"80%%\" BORDER=2 CELLSPACING=1 "
		"CELLPADDING=1\n");
	fprintf(fp, " summary=\"Summary of statistics for the period.\">\n");
	fprintf(fp, "<tr><th colspan=2>Summary\n");
	fprintf(fp, "<tr><td class=text>Total Hits<td class=n>%d\n",
		totalhits);
	fprintf(fp, "<tr><td class=text>Total Files<td class=n>%d\n",
		totalfiles);
	fprintf(fp, "<tr><td class=text>Total Pages<td class=n>%d\n",
		totalpages);
	fprintf(fp, "<tr><td class=text>Total Unique Agents<td class=n>%d\n",
		n_agents);
	fprintf(fp, "<tr><td class=text>Total Unknown Agents<td class=n>%d\n",
		n_unknown_browsers);
	fprintf(fp, "<tr><td class=text>Total Unique OSes<td class=n>%d\n",
		n_os);
	fprintf(fp, "<tr><td class=text>Total Unknown OSes<td class=n>%d\n",
		n_unknown_os);
	fprintf(fp, "</table>\n");

	fprintf(fp, "<p><table WIDTH=\"80%%\" BORDER=2 CELLSPACING=1 "
		"CELLPADDING=1\n");
	fprintf(fp, " summary=\"Detailed statistics about bots.\">\n");
	fprintf(fp, "<tr><th colspan=8>Bots\n");
	fprintf(fp, "<tr><th class=day>#<th class=hits colspan=2>Hits\n"
		"<th class=files colspan=2>Files\n"
		"<th class=pages colspan=2>Pages\n"
		"<th class=name>Browser\n");
	fprintf(fp, "<tr><td class=day>1"
		"<td class=n>%d<td>%.0f%%"
		"<td class=n>%d<td>%.0f%%"
		"<td class=n>%d<td>%.0f%%\n"
		"<td class=text>%s\n",
		browsers[bot_index].hits,
		percent_hits(browsers[bot_index].hits),
		browsers[bot_index].files,
		percent_files(browsers[bot_index].files),
		browsers[bot_index].pages,
		percent_pages(browsers[bot_index].pages),
		browsers[bot_index].name);
	fprintf(fp, "<tr><td class=day>2"
		"<td class=n>%d<td>%.0f%%"
		"<td class=n>%d<td>%.0f%%"
		"<td class=n>%d<td>%.0f%%\n"
		"<td class=text>%s\n",
		browsers[java_index].hits,
		percent_hits(browsers[java_index].hits),
		browsers[java_index].files,
		percent_files(browsers[java_index].files),
		browsers[java_index].pages,
		percent_pages(browsers[java_index].pages),
		browsers[java_index].name);
	fprintf(fp, "<tr><td class=day>3"
		"<td class=n>%d<td>%.0f%%"
		"<td class=n>%d<td>%.0f%%"
		"<td class=n>%d<td>%.0f%%\n"
		"<td class=text>%s\n",
		browsers[empty_index].hits,
		percent_hits(browsers[empty_index].hits),
		browsers[empty_index].files,
		percent_files(browsers[empty_index].files),
		browsers[empty_index].pages,
		percent_pages(browsers[empty_index].pages),
		browsers[empty_index].name);
	fprintf(fp, "</table>\n");

	fprintf(fp, "<p><table WIDTH=\"80%%\" BORDER=2 CELLSPACING=1 "
		"CELLPADDING=1\n");
	fprintf(fp, " summary=\"Detailed statistics about browsers used.\">\n");
	fprintf(fp, "<tr><th colspan=8>Browsers\n");
	fprintf(fp, "<tr><th class=day>#<th class=hits colspan=2>Hits\n"
		"<th class=files colspan=2>Files\n"
		"<th class=pages colspan=2>Pages\n"
		"<th class=name>Browser\n");
	for (i = 0; i < N_BROWSERS; ++i) {
		if (i == bot_index || i == empty_index || i == java_index)
			continue;
		if (browsers[i].hits == 0)
			continue;
		if (strcmp(browsers[i].name, "Everybody Else(tm)") == 0)
			fprintf(fp, "<tr><td class=day>%d"
					"<td class=n>%d<td>%.1f%%"
					"<td class=n>%d<td>%.1f%%"
					"<td class=n>%d<td>%.1f%%\n"
					"<td class=text><a href=\"unknown-browsers.html\">%s</a>\n",
					i + 1,
					browsers[i].hits,
					percent_browser_hits(browsers[i].hits),
					browsers[i].files,
					percent_browser_files(browsers[i].files),
					browsers[i].pages,
					percent_browser_pages(browsers[i].pages),
					browsers[i].name);
		else
			fprintf(fp, "<tr><td class=day>%d"
					"<td class=n>%d<td>%.1f%%"
					"<td class=n>%d<td>%.1f%%"
					"<td class=n>%d<td>%.1f%%\n"
					"<td class=text>%s\n",
					i + 1,
					browsers[i].hits,
					percent_browser_hits(browsers[i].hits),
					browsers[i].files,
					percent_browser_files(browsers[i].files),
					browsers[i].pages,
					percent_browser_pages(browsers[i].pages),
					browsers[i].name);
	}
	fprintf(fp, "</table>\n");

	fprintf(fp, "<p><table WIDTH=\"80%%\" BORDER=2 "
		"CELLSPACING=1 CELLPADDING=1\n");
	fprintf(fp, " summary=\"Detailed statistics about "
		"operating systems used.\">\n");
	fprintf(fp, "<tr><th colspan=8>Operating Systems\n");
	fprintf(fp, "<tr><th class=day>#<th class=hits colspan=2>Hits"
		"<th class=files colspan=2>Files"
		"<th class=pages colspan=2>Pages"
		"<th class=name>OS\n");
	cur_group = 0;
	for (i = 0, n = 1; i < n_os; ++i, ++n) {
		while (cur_group < n_groups &&
			   greater(&groups[cur_group], &os[i])) {
			fprintf(fp, "<tr bgcolor=\"#D0D0E0\"><td class=day>-"
				"<td class=n>%d<td>%.1f%%",
				groups[cur_group].hits,
				percent_os_hits(groups[cur_group].hits));
			fprintf(fp, "<td class=n>%d<td>%.1f%%\n",
				groups[cur_group].files,
				percent_os_files(groups[cur_group].files));
			fprintf(fp, "<td class=n>%d<td>%.1f%%\n",
				groups[cur_group].pages,
				percent_os_pages(groups[cur_group].pages));
			fprintf(fp, "<td class=text>%s\n",
				groups[cur_group].name);
			++cur_group;
		}
/*		if (percent_pages(os[i].pages) >= 1.0) */
		{
			fprintf(fp, "<tr><td class=day>%d"
				"<td class=n>%d<td>%.1f%%"
				"<td class=n>%d<td>%.1f%%"
				"<td class=n>%d<td>%.1f%%\n",
				n,
				os[i].hits, percent_os_hits(os[i].hits),
				os[i].files, percent_os_files(os[i].files),
				os[i].pages, percent_os_pages(os[i].pages));
			if (strcmp(os[i].name, "Unknown") == 0) {
				need_unknown = 1;
				fprintf(fp, "<td class=text><a href="
					"\"unknown-os.html\">Unknown</a>\n");
			} else
				fprintf(fp, "<td class=text>%s\n", os[i].name);
		}
	}

	/* Just in case */
	while (cur_group < n_groups) {
		fprintf(fp, "<tr bgcolor=\"#D0D0E0\"><td class=day>%d"
			"<td class=n>%d<td>%.1f%%",
			n, groups[cur_group].hits,
			percent_os_hits(groups[cur_group].hits));
		fprintf(fp, "<td class=n>%d<td>%.1f%%\n",
			groups[cur_group].files,
			percent_os_files(groups[cur_group].files));
		fprintf(fp, "<td class=n>%d<td>%.1f%%\n",
			groups[cur_group].pages,
			percent_os_pages(groups[cur_group].pages));
		fprintf(fp, "<td class=text>%s\n", groups[cur_group].name);
		++cur_group;
		if (groups[cur_group].name == NULL)
			++cur_group; /* skip NONE */
	}

	fprintf(fp, "</table>\n\n");

	fprintf(fp, "<hr>\n");
	fprintf(fp, "<small><a href=\"index.html\">[Back]</a>"
		"&nbsp;<a href=\"agent-help.html\">[Help]</a></small>\n");

	/* trailer */
	fprintf(fp, "</center></body>\n</html>\n");

	fclose(fp);

#if 0
	fp = fopen(ALL_FILE, "w");
	if (!fp) {
		perror(ALL_FILE);
		exit(1);
	}

	/* header */
	fprintf(fp, "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 "
		"Transitional//EN\">\n");
	fprintf(fp, "<html>\n<head>");
	fprintf(fp, "<title>Unknown agents for YOW</title>");
	fprintf(fp, "</head>\n<body bgcolor=\"#e8e8e8\">\n");
	fprintf(fp, "<center><h1>Unknown agents for YOW</h1>");
	fprintf(fp, "<p><table WIDTH=510 BORDER=2 CELLSPACING=1 "
		"CELLPADDING=1>\n");
	fprintf(fp, "<tr><th colspan=2>Unknown\n");
	for (i = 0; i < n_agents; ++i) {
		char *p = agents[i].name;
		while (isspace(*p))
			++p;
		if (*p)
			fprintf(fp, "<tr><td bgcolor=\"#4fa83f\">&nbsp;<td>%s"
				"<td align=right>%d\n",
				agents[i].name, agents[i].hits);
		else
			fprintf(fp, "<tr><td bgcolor=\"#ff0000\">&nbsp;"
				"<td>(empty)<td align=right>%d\n",
				agents[i].hits);
	}
	fprintf(fp, "</table>\n");

	fprintf(fp, "<p><a href=\"os.html\">Back</a>\n");

	/* trailer */
	fprintf(fp, "</center></body>\n</html>\n");

	fclose(fp);
#endif

	if (!need_unknown)
		return;

	fp = fopen(UNKNOWN_FILE, "w");
	if (!fp) {
		perror(UNKNOWN_FILE);
		exit(1);
	}

	/* header */
	fprintf(fp, "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 "
		"Transitional//EN\">\n");
	fprintf(fp, "<html>\n<head>");
	fprintf(fp, "<title>Unknown agents for YOW</title>");
	fprintf(fp, "</head>\n<body bgcolor=\"#e8e8e8\">\n");
	fprintf(fp, "<center><h1>Unknown agents for YOW</h1>");
	fprintf(fp, "<p><table BORDER=2 CELLSPACING=1 CELLPADDING=1>\n");
	for (i = 0; i < n_unknown_os; ++i) {
		char *p = unknown_os[i].name;
		while (isspace(*p))
			++p;
		if (*p)
			fprintf(fp, "<tr><td>%s<td align=right>%d\n",
				unknown_os[i].name, unknown_os[i].hits);
		else
			fprintf(fp, "<tr><td>(empty)<td align=right>%d\n",
				unknown_os[i].hits);
	}
	fprintf(fp, "</table>\n");

	fprintf(fp, "<p><a href=\"os.html\">Back</a>\n");

	/* trailer */
	fprintf(fp, "</center></body>\n</html>\n");

	fclose(fp);

	fp = fopen(UNKNOWN_BROWSERS, "w");
	if (!fp) {
		perror(UNKNOWN_BROWSERS);
		exit(1);
	}

	/* header */
	fprintf(fp, "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 "
		"Transitional//EN\">\n");
	fprintf(fp, "<html>\n<head>");
	fprintf(fp, "<title>Unknown browsers for YOW</title>");
	fprintf(fp, "</head>\n<body bgcolor=\"#e8e8e8\">\n");
	fprintf(fp, "<center><h1>Unknown browsers for YOW</h1>");
	fprintf(fp, "<p><table BORDER=2 CELLSPACING=1 CELLPADDING=1>\n");
	for (i = 0; i < n_unknown_browsers; ++i) {
		char *p = unknown_browsers[i].name;
		while (isspace(*p))
			++p;
		if (*p)
			fprintf(fp, "<tr><td>%s<td align=right>%d\n",
				unknown_browsers[i].name, unknown_browsers[i].hits);
		else
			fprintf(fp, "<tr><td>(empty)<td align=right>%d\n",
				unknown_browsers[i].hits);
	}
	fprintf(fp, "</table>\n");

	fprintf(fp, "<p><a href=\"os.html\">Back</a>\n");

	/* trailer */
	fprintf(fp, "</center></body>\n</html>\n");

	fclose(fp);
}

#ifdef THTTPD
static int check_url(char *url)
{
	int i;

	if (*url != '/')
		printf("PROB 1: %s\n", url);

	for (i = 0; i < NUMURLS; ++i)
		if (strncmp(url, urllist[i].str, urllist[i].len) == 0)
			return urllist[i].len;

	return 0;
}
#endif

static void process_file(FILE *fp, char *fname)
{
	char line[4096], *p, *e, *url;
	int status, file, page, nline = 0;

	while (fgets(line, sizeof(line) - 1, fp)) {
		++nline;
		p = strchr(line, '[');
		e = strchr(line, ']');
		if (!p || !e) {
			printf("%s: DATE MISSING line %d\n", fname, nline);
			printf("\t%s", line);
			continue;
		}
		*e++ = '\0';
		if (parse_date(p + 1))
			continue;

		file = 0;
		page = 0;

		/* Isolate url */
		++e; /* skip space */
		assert(*e == '"');
		for (url = ++e; *e && *e != '"'; ++e)
			;
		*e++ = '\0';

		status = strtol(e, NULL, 10);
		if (status < 100 || status > 600) {
			if (status) /* Too many status 0s to report */
				printf("Bad status %d line %d\n",
					   status, nline);
			continue;
		}

		if (status == 200) {
			file = 1;

			/* Count pages */
			/* SAM This should parse better */
			if (strncmp(url, "GET ", 4) == 0) {
#ifdef THTTPD
				int n;

				url += 4;

				n = check_url(url);
				if (n == 0)
					printf("PROBS: %s\n", url);
				else
					url += n;
#else
				url += 4;
#endif

				p = strchr(url, ' ');
				if (p)
					*p = '\0';
				p = strrchr(url, '.');
				if (p) {
					if (strncmp(p, ".htm", 4) == 0)
						page = 1;
				} else if (!strstr(url, "style"))
					page = 1;
			} else if (strncmp(url, "HEAD ", 5) &&
				   strncmp(url, "POST ", 5))
				printf("PROBLEM %s\n", url);
		}

		/* Isolate agent */
		p = strrchr(e, '\n');
		if (!p) {
			printf("LINE TOO LONG\n");
			exit(1);
		}
		--p;
		if (*p != '"') {
			badline(line, p, nline);
			continue;
		}
		*p = '\0';
		for (--p; *p != '"'; --p)
			if (p == line)
				break;
		if (p == line) {
			badline(line, NULL, nline);
			continue;
		}
		++p;

		add_agent(p, file, page);
	}

}


static void add_agent(char *agent, int file, int page)
{
	int i;

	for (i = 0; i < n_agents; ++i)
		if (strcmp(agents[i].name, agent) == 0) {
			++agents[i].hits;
			agents[i].files += file;
			agents[i].pages += page;
			return;
		}

	if (n_agents == max_agents) {
		max_agents += 100;
		agents = realloc(agents,
				 max_agents * sizeof(struct name_count));
		if (!agents) {
			printf("Out of memory. %d agents\n", n_agents);
			exit(1);
		}
	}

	agents[n_agents].name  = must_strdup(agent);
	agents[n_agents].hits  = 1;
	agents[n_agents].files = file;
	agents[n_agents].pages = page;
	++n_agents;
}

static void add_unknown_os(struct name_count *agent)
{
	if (n_unknown_os == max_unknown_os) {
		max_unknown_os += 50;
		unknown_os = realloc(unknown_os,
				   max_unknown_os * sizeof(struct name_count));
		if (!unknown_os) {
			printf("Out of memory. Unknowns %d\n", n_unknown_os);
			exit(1);
		}
	}

	memcpy(&unknown_os[n_unknown_os], agent, sizeof(struct name_count));
	++n_unknown_os;
}

static void add_unknown_browser(struct name_count *agent)
{
	if (n_unknown_browsers == max_unknown_browsers) {
		max_unknown_browsers += 50;
		unknown_browsers = realloc(unknown_browsers,
				   max_unknown_browsers * sizeof(struct name_count));
		if (!unknown_browsers) {
			printf("Out of memory. Unknowns %d\n", n_unknown_browsers);
			exit(1);
		}
	}

	memcpy(&unknown_browsers[n_unknown_browsers], agent, sizeof(struct name_count));
	++n_unknown_browsers;
}

static void add_os(int group, char *name, struct name_count *agent)
{
	int i;

#if 0
	/* Do Windows grouping here */
	if (strncmp(name, "Windows ", 8) == 0) {
		/* These are all < 0.5% */
		char *w = name + 8;
		if (strcmp(w, "ME") == 0 ||
		    strcmp(w, "95") == 0 ||
			strcmp(w, "NT") == 0)
			name = "Windows Other";
	}
#endif

	os_hits  += agent->hits;
	os_files += agent->files;
	os_pages += agent->pages;

	if (group >= 0) {
		groups[group].hits  += agent->hits;
		groups[group].files += agent->files;
		groups[group].pages += agent->pages;
	}

	for (i = 0; i < n_os; ++i)
		if (strcmp(os[i].name, name) == 0) {
			os[i].hits  += agent->hits;
			os[i].files += agent->files;
			os[i].pages += agent->pages;
			return;
		}

	if (n_os == max_os) {
		max_os += 20;
		os = realloc(os,
			     max_os * sizeof(struct name_count));
		if (!os) {
			printf("Out of memory. oses = %d\n", n_os);
			exit(1);
		}
	}

	os[n_os].name  = must_strdup(name);
	os[n_os].hits  = agent->hits;
	os[n_os].files = agent->files;
	os[n_os].pages = agent->pages;
	os[n_os].group = group;
	++n_os;
}

static int isabot(char *line)
{
	int i;

	/* printf("Checking '%s'\n", line); */

	if (strcasestr(line, "bot") ||
	    strcasestr(line, "spider") ||
	    strcasestr(line, "crawler"))
		return 1;

	for (i = 0; i < nbots; ++i)
		if (strcasestr(line, bots[i]))
			return 1;
	return 0;
}


/* Returns 0 if unknown, 1 otherwise */
static int parse_agent(struct name_count *agent)
{
	char *line = agent->name;
	char *p;
	struct name_count *browser = NULL;

	totalhits  += agent->hits;
	totalfiles += agent->files;
	totalpages += agent->pages;

	/* Skip leading whitespace */
	while (isspace(*line))
		++line;
	/* Remove trailing whitespace */
	p = line + strlen(line) - 1;
	if (p > line)
		while (isspace(*p))
			*p-- = '\0';

	if (*line == '\0' || strcmp(line, "Mozilla/4.0 (compatible;)") == 0)
		browser = &browsers[EMPTY];
	else if (isabot(line))
		browser = &browsers[BOTS];
	/* Must put Opera before MSIE & Netscape */
	else if (strstr(line, "Opera"))
		browser = &browsers[OPERA];
	else if (strstr(line, "Chrome")) /* must come before safari */
		browser = &browsers[CHROME];
	else if (strstr(line, "Safari"))
		browser = &browsers[SAFARI];
	else if ((p = strstr(line, "MSIE"))) {
		for (p += 4; isspace(*p) || *p == '+'; ++p)
			;
		switch (*p) {
		default:
			printf("UNKNOWN MSIE %s\n", p);
			/* fall thru */
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			browser = &browsers[MSIE];
		}
	} else if ((p = strstr(line, "Mozilla")) &&
		   !strstr(line, "ompatible")) {
		p += 7;
		if (*p++ == '/')
			switch (*p) {
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
				browser = &browsers[NETSCAPE];
				break;
			default:
				browser = &browsers[NETSCAPE];
				printf("UNKNOWN MOZ %s (%d)\n",
				       line, agent->hits);
				break;
			}
		else {
			browser = &browsers[NETSCAPE];
			printf("UNKNOWN MOZILLA %s\n", line);
		}
	} else if (strstr(line, "Java") ||
			   strstr(line, "Python") ||
			   strstr(line, "Perl"))
		browser = &browsers[JAVA];
	else {
		browser = &browsers[OTHER_BROWSER];
		add_unknown_browser(agent);
	}

	if (browser) {
		browser->hits  += agent->hits;
		browser->files += agent->files;
		browser->pages += agent->pages;
	}

	/* Bots don't count in the OS count */
	if (browser == &browsers[BOTS] ||
		browser == &browsers[EMPTY] ||
		browser == &browsers[JAVA])
		return 0;

	/* Try to intuit the OS */
	p = strstr(line, "Win");
	if (p) {
again:
		if (strncmp(p, "Windows", 7) == 0)
			p += 7;
		else
			p += 3;
		if (*p == ' ' || *p == '_' || *p == '+')
			++p;

		if (strncmp(p, "NT", 2) == 0) {
			char *e;

			int major = strtol(p + 2, &e, 10);
			if (p != e && *e == '.' && isdigit(*(e + 1))) {
				++e;
				switch (major) {
				case 10:
					add_os(WINDOZE, "Windows 10", agent);
					return 1;
				case 6:
					switch (*e) {
					case '0':
						add_os(WINDOZE, "Windows Vista", agent);
						return 1;
					case '1':
						add_os(WINDOZE, "Windows 7", agent);
						return 1;
					case '2':
					case '3': /* 8.1 */
						add_os(WINDOZE, "Windows 8", agent);
						return 1;
					}
					break;
				case 5:
					switch (*e) {
					case '0':
						add_os(WINDOZE, "Windows 2000", agent);
						return 1;
					case '1':
						add_os(WINDOZE, "Windows XP", agent);
						return 1;
					case '2':
						add_os(WINDOZE, "Windows Server 2003", agent);
						return 1;
					}
					break;
				case 4:
					add_os(WINDOZE, "Windows NT", agent);
					return 1;
				}
			}
			printf("Windows NT unknown %s\n", line);
			add_os(WINDOZE, "Windows NT", agent);
			return 1;
		} else if (strncmp(p, "67", 2) == 0)
			/* Win67 not necessarily windows */
			add_os(OTHER, "Other", agent);
		else if (strncmp(p, "98", 2) == 0) {
			if (strstr(p, "Win 9x 4.9"))
				add_os(WINDOZE, "Windows ME", agent);
			else
				add_os(WINDOZE, "Windows 98", agent);
		} else if (strncmp(p, "95", 2) == 0)
			add_os(WINDOZE, "Windows 95", agent);
		else if (strncmp(p, "9x", 2) == 0) {
			if (strncmp(p, "9x 4.90", 7) == 0)
				add_os(WINDOZE, "Windows ME", agent);
			else
				add_os(WINDOZE, "Windows Other", agent);
		} else if (strncmp(p, "2000", 4) == 0)
			add_os(WINDOZE, "Windows 2000", agent);
		else if (strncmp(p, "3.1", 3) == 0)
			add_os(WINDOZE, "Windows Other", agent);
		else if (strncmp(p, "ME", 2) == 0 ||
			 strncmp(p, "Me", 2) == 0)
			add_os(WINDOZE, "Windows ME", agent);
		else if (strncmp(p, "CE", 2) == 0)
			add_os(WINDOZE, "Windows Other", agent);
		else if (strncmp(p, "XP", 2) == 0)
			add_os(WINDOZE, "Windows XP", agent);
		else if (strncmp(p, "-NT", 3) == 0)
			add_os(WINDOZE, "Windows NT", agent);
		else if (strstr(p, "Phone"))
			add_os(OTHER, "Mobile", agent);
		else if ((p = strstr(p, "Win")))
			goto again;
		else {
			add_os(WINDOZE, "Windows Other", agent); /* other */
			printf("windoze unknown: %s\n", line);
		}
	} else if (strstr(line, "Darwin"))
			add_os(APPLE, "Macintosh", agent);
	else if ((p = strstr(line, "Mac")) && strncmp(p, "Machine", 7)) {
		if (strstr(line, "OS X"))
			add_os(APPLE, "Macintosh", agent);
		else if (strstr(line, "Darwin"))
			add_os(APPLE, "Macintosh", agent);
		else if (strncmp(p, "Macintosh", 9) == 0 ||
			 strncmp(p, "Mac_PowerPC", 11) == 0 ||
			 strncmp(p, "Mac_PPC", 7) == 0)
			add_os(APPLE, "Macintosh", agent);
		else {
			add_os(APPLE, "Macintosh", agent);
			printf("Macintosh unknown: %s\n", line);
			return 0; /* Put in unknown page */
		}
	} else if (strstr(line, "AppleWebKit"))
		/* Assume Mac OS X */
		add_os(APPLE, "Macintosh", agent);

	/* Unix */
	else if (strstr(line, "Linux"))
		add_os(UNIX, "Linux", agent);
	else if (strstr(line, "linux"))
		add_os(UNIX, "Linux", agent);
	else if (strstr(line, "Solaris"))
		add_os(UNIX, "Solaris", agent);
	else if (strstr(line, "SunOS"))
		add_os(UNIX, "SunOS", agent);
	else if (strstr(line, "AIX"))
		add_os(UNIX, "AIX", agent);
	else if (strstr(line, "BSD"))
		add_os(UNIX, "BSD", agent);
	else if (strstr(line, "bsd"))
		add_os(UNIX, "BSD", agent);
	else if (strstr(line, "Unix"))
		add_os(UNIX, "Unix Other", agent);
	else if (strstr(line, "UNIX"))
		add_os(UNIX, "Unix Other", agent);
	else if (strstr(line, "X11"))
		add_os(UNIX, "X11", agent);
	else if (strncmp(line, "WebDownloader for X", 19) == 0)
		add_os(UNIX, "X11", agent);
	else if (strstr(line, "Red Hat"))
		add_os(UNIX, "Linux", agent);
	/* Some Unix only browsers */
	else if (strstr(line, "Konqueror"))
		add_os(UNIX, "Unix Other", agent);
	else if (strncmp(line, "Lynx", 4) == 0)
		add_os(UNIX, "Unix Other", agent);
	else if (strncmp(line, "Dillo", 4) == 0)
		add_os(UNIX, "Unix Other", agent);
	/* Unix */

	else if (strstr(line, "BlackBerry"))
		add_os(OTHER, "Mobile", agent); /* SAM ? */
	else if (strstr(line, "Android"))
		add_os(OTHER, "Mobile", agent); /* SAM ? */
	else if (strstr(line, "Java"))
		add_os(OTHER, "Java/Perl/Python", agent);
	else if (strstr(line, "libwww-perl"))
		add_os(OTHER, "Java/Perl/Python", agent);
	else if (strstr(line, "Python"))
		add_os(OTHER, "Java/Perl/Python", agent);
	else if (strstr(line, "Nintendo Wii"))
		add_os(OTHER, "Wii", agent);
	else if (strstr(line, "WebTV"))
		add_os(OTHER, "WebTV", agent);
	else if (strstr(line, "RISC OS"))
		add_os(OTHER, "RISC OS", agent);
	else if (strstr(line, "SAGEM"))
		add_os(OTHER, "SAGEM", agent);
	else if (strstr(line, "OS/2"))
		add_os(OTHER, "OS/2", agent);
	else if (strstr(line, "BEOS"))
		add_os(OTHER, "BeOS", agent);
	else if (strstr(line, "AmigaOS"))
		add_os(OTHER, "Amiga", agent);
	else if (strstr(line, "QNX"))
		add_os(OTHER, "QNX", agent);
	else if (strstr(line, "I-Opener"))
		add_os(OTHER, "QNX", agent);
	else if (strcasestr(line, "PlayStation"))
		add_os(OTHER, "PlayStation", agent);
	else if (strstr(line, "Google Desktop"))
		add_os(OTHER, "Google", agent);
	else if (strncmp(line, "Amiga", 5) == 0)
		add_os(OTHER, "Amiga", agent);
	else if (strstr(line, "UP.Browser")) /* Cell phone browser */
		add_os(OTHER, "Mobile", agent);
	else if (strstr(line, "Presto"))
		add_os(OTHER, "Mobile", agent);
	else if (strstr(line, "eCatch")) /* windows only */
		add_os(WINDOZE, "Windows Other", agent); /* other */
	else if (strstr(line, "Offline Explorer")) /* windows only */
		add_os(WINDOZE, "Windows Other", agent); /* other */
	else if (strstr(line, "MSIE"))
		add_os(WINDOZE, "Windows Other", agent); /* other */
	else if (strncmp(line, "DA 5.0", 6) == 0)
		add_os(WINDOZE, "Windows Other", agent); /* other */
	else if (strncmp(line, "Teleport Pro", 12) == 0) /* windows only */
		add_os(WINDOZE, "Windows Other", agent); /* other */
	else if (strcasestr(line, "facebook"))
		add_os(OTHER, "Facebook", agent);
	else {
		add_os(OTHER, "Unknown", agent);
		add_unknown_os(agent);
		return 0;
	}

	return 1;
}


static int hits_compare(const void *a, const void *b)
{
	const struct name_count *a1 = (struct name_count *)a;
	const struct name_count *b1 = (struct name_count *)b;
	if (b1->hits == a1->hits)
		/* break the tie with pages */
		return b1->pages - a1->pages;
	else
		return b1->hits - a1->hits;
}

static int files_compare(const void *a, const void *b)
{
	const struct name_count *a1 = (struct name_count *)a;
	const struct name_count *b1 = (struct name_count *)b;
	if (b1->files == a1->files)
		/* break the tie with hits */
		return b1->hits - a1->hits;
	else
		return b1->files - a1->files;
}

static int pages_compare(const void *a, const void *b)
{
	const struct name_count *a1 = (struct name_count *)a;
	const struct name_count *b1 = (struct name_count *)b;
	if (b1->pages == a1->pages)
		/* break the tie with hits */
		return b1->hits - a1->hits;
	else
		return b1->pages - a1->pages;
}


static void sort_oses(void)
{
	int i;

	switch (compare_type) {
	case COMPARE_HITS:
		qsort(groups, n_groups, sizeof(struct name_count),
		      hits_compare);
		qsort(os, n_os, sizeof(struct name_count),
		      hits_compare);
		qsort(agents, n_agents, sizeof(struct name_count),
		      hits_compare);
		qsort(browsers, N_BROWSERS, sizeof(struct name_count),
		      hits_compare);
		break;
	case COMPARE_FILES:
		qsort(groups, n_groups, sizeof(struct name_count),
		      files_compare);
		qsort(os, n_os, sizeof(struct name_count),
		      files_compare);
		qsort(agents, n_agents, sizeof(struct name_count),
		      files_compare);
		qsort(browsers, N_BROWSERS, sizeof(struct name_count),
		      files_compare);
		break;
	case COMPARE_PAGES:
		qsort(groups, n_groups, sizeof(struct name_count),
		      pages_compare);
		qsort(os, n_os, sizeof(struct name_count),
		      pages_compare);
		qsort(agents, n_agents, sizeof(struct name_count),
		      pages_compare);
		qsort(browsers, N_BROWSERS, sizeof(struct name_count),
		      pages_compare);
		break;
	}
	qsort(unknown_os, n_unknown_os, sizeof(struct name_count), hits_compare);
	qsort(unknown_browsers, n_unknown_browsers, sizeof(struct name_count), hits_compare);

	for (i = 0; i < N_BROWSERS; ++i)
		if (strncmp(browsers[i].name, "Bot", 3) == 0)
			bot_index = i;
		else if (strncmp(browsers[i].name, "Empty", 5) == 0)
			empty_index = i;
		else if (strncmp(browsers[i].name, "Script", 6) == 0)
			java_index = i;

	/* Drop empty groups */
	while (n_groups > 0 && groups[n_groups - 1].hits == 0) {
		printf("Dropping %s: empty\n", groups[n_groups - 1].name);
		--n_groups;
	}
}


static char *cur_time(time_t now)
{
	static char timestamp[32];

	/* convert to timestamp string */
	strftime(timestamp, sizeof(timestamp), "%d-%b-%Y %H:%M %Z",
		 localtime(&now));

	return timestamp;
}


static char *months[12] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};


static int parse_date(char *date)
{
	int tz;
	char month[8];
	struct tm tm;
	time_t this;

	/* SAM what about tz? */
	memset(&tm, 0, sizeof(tm));
	if (sscanf(date, "%d/%[^/]/%d:%d:%d:%d %d",
		   &tm.tm_mday, month, &tm.tm_year,
		   &tm.tm_hour, &tm.tm_min, &tm.tm_sec, &tz) != 7) {
		printf("Bad date: %s\n", date);
		return 1;
	}

	tm.tm_year -= 1900;

	for (tm.tm_mon = 0; tm.tm_mon < 12; ++tm.tm_mon)
		if (strcmp(months[tm.tm_mon], month) == 0) {
			this = mktime(&tm);
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

	printf("BAD MONTH %s\n", date);
	return 1;
}


static void addbot(char *bot)
{
	static int max;

	/* This case is handled by default */
	if (strcasestr(bot, "bot"))
		return;

	if (nbots >= max) {
		max += 10;
		bots = realloc(bots, max * sizeof(char *));
		if (!bots) {
			printf("Out of memory\n");
			exit(1);
		}
	}
	bots[nbots] = must_strdup(bot);
	if (!bots[nbots]) {
		printf("Out of memory\n");
		exit(1);
	}
	++nbots;
}
