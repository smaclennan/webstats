#define _GNU_SOURCE /* for strcasestr */
#include "webstats.h"

#include <gd.h>
#include <gdfontmb.h>
#include <gdfonts.h>

#define TOP_TEN 10

/* Visits takes no more time on YOW. */
static int enable_visits;
static int enable_pages;
static int enable_topten;
static int width = 422;
static int offset = 35;

static struct site {
	char *name;
	int color;
	int hits;
	int pages;
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
static unsigned long total_pages;
static unsigned long total_size;
static unsigned long total_visits;

static char *outdir;
static char *outfile = "stats.html";
static char *outgraph = "pie.gif";

DB *pages;
static int max_url;

static struct {
	char *name;
	unsigned long size;
} top[TOP_TEN];
int n_top;

#define m(n)   (((double)(n)) / 1024.0 / 1024.0)

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
	fprintf(fp, "\n</center>\n</body>\n</html>\n");
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

	fprintf(fp, "<p><img src=\"pie.gif\" width=%d height=235 "
		"alt=\"Pie Charts\">\n\n",
		width);

	fprintf(fp, "<p><table WIDTH=\"%d%%\" BORDER=1 "
		"CELLSPACING=1 CELLPADDING=1",
		enable_pages ? 80 : 60);
	fprintf(fp, " summary=\"Satistics.\">\n");

	fputs("<tr><th>Site<th colspan=2>Hits", fp);
	if (enable_pages)
		fputs("<th colspan=2>Pages", fp);
	if (enable_visits)
		fputs("<th colspan=2>Visits", fp);
	fputs("<th colspan=2>Size (M)\n", fp);

	for (i = 0; i < n_sites; ++i) {
		if (sites[i].hits == 0)
			continue;
		fprintf(fp, "<tr><td>%s"
			"<td align=right>%d<td align=right>%.1f%%",
			sites[i].name,
			sites[i].hits,
			(double)sites[i].hits * 100.0 / (double)total_hits);
		if (enable_pages)
			fprintf(fp, "<td align=right>%d<td align=right>%.1f%%",
				sites[i].pages,
				(double)sites[i].pages * 100.0 / (double)total_pages);
		if (enable_visits)
			fprintf(fp, "<td align=right>%ld<td align=right>%.1f%%",
				sites[i].visits,
				(double)sites[i].visits * 100.0 /
				(double)total_visits);
		fprintf(fp, "<td align=right>%.1f<td align=right>%.1f%%\n",
			(double)sites[i].size / 1024.0,
			(double)sites[i].size * 100.0 / (double)total_size);
	}

	fprintf(fp, "<tr><td>Totals<td align=right>%ld<td>&nbsp;", total_hits);
	if (enable_pages)
		fprintf(fp, "<td align=right>%ld<td>&nbsp;", total_pages);
	if (enable_visits)
		fprintf(fp, "<td align=right>%ld<td>&nbsp;", total_visits);
	fprintf(fp, "<td align=right>%ld<td>&nbsp;\n", total_size / 1024);

	fprintf(fp, "</table>\n");

	while (includes) {
		add_include(includes->name, fp);
		includes = includes->next;
	}

	if (enable_topten) {
		fprintf(fp, "<p><table WIDTH=\"%d%%\" BORDER=1 "
			"CELLSPACING=1 CELLPADDING=1 Summary=\"Top Ten\">",
			enable_pages ? 80 : 60);
		fprintf(fp, "<th colspan=2>Top Ten</th>\n");

		for (i = 0; i < n_top; ++i) {
			double size = m(top[i].size);
			fprintf(fp, "<tr><td>%s<td align=right>%.1f\n", top[i].name, size);
		}

		fprintf(fp, "</table>\n");
	}

	out_trailer(fp);

	fclose(fp);
}

static void out_hr(FILE *fp)
{
	fputs("-----------------------------------------------------", fp);
	if (enable_visits)
		fputs("----------------", fp);
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

	fputs("Site\t\t\t Hits\t\t     Size\n", fp);
	out_hr(fp);

	for (i = 0; i < n_sites; ++i) {
		if (sites[i].hits == 0)
			continue;
		fprintf(fp, "%-20s%6d  %3.1f%%\t%6ld  %3.1f%%\n",
			sites[i].name, sites[i].hits,
			(double)sites[i].hits * 100.0 / (double)total_hits,
			sites[i].size / 1024,
			(double)sites[i].size * 100.0 / (double)total_size);
	}

	out_hr(fp);
	fprintf(fp, "%-20s%6ld      \t%6ld\n",
		"Totals", total_hits, total_size / 1024);

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

static void out_graphs(void)
{
	FILE *fp;
	char *fname;
	int i, tarc, color;
	int x;

	gdImagePtr im = gdImageCreate(width, 235);
	color = gdImageColorAllocate(im, 0xff, 0xff, 0xff); /* background */
	gdImageColorTransparent(im, color);

	color = gdImageColorAllocate(im, 0, 0, 0); /* text */

	gdImageString(im, gdFontMediumBold, 87, 203,
		      (unsigned char *)"Hits", color);
	gdImageString(im, gdFontMediumBold, 305, 203,
		      (unsigned char *)"Bytes", color);
	if (enable_visits)
		gdImageString(im, gdFontMediumBold, 522, 203,
			      (unsigned char *)"Visits", color);
	else
		gdImageString(im, gdFontMediumBold, 522, 203,
			      (unsigned char *)"Pages", color);

	x = offset;
	for (i = 0; i < n_sites; ++i) {
		color = getcolor(im, sites[i].color);
		gdImageString(im, gdFontSmall, x, 220,
			      (unsigned char *)sites[i].name, color);
		x += 100;
	}

	for (tarc = i = 0; i < n_sites; ++i) {
		sites[i].arc = sites[i].hits * 360 / total_hits;
		tarc += sites[i].arc;
	}

	/* Compensate the first arc */
	sites[0].arc += 360 - tarc;

	draw_pie(im, 100, 100, 198);

	/* Calculate the size arcs */
	for (tarc = i = 0; i < n_sites; ++i) {
		sites[i].arc = sites[i].size * 360 / total_size;
		tarc += sites[i].arc;
	}

	/* Compensate the first arc */
	sites[0].arc += 360 - tarc;

	draw_pie(im, 320, 100, 198);

	if (enable_visits) {
		/* Calculate the visit arcs */
		for (tarc = i = 0; i < n_sites; ++i) {
			sites[i].arc = sites[i].visits * 360 / total_visits;
			tarc += sites[i].arc;
		}

		/* Compensate the first arc */
		sites[0].arc += 360 - tarc;

		draw_pie(im, 540, 100, 198);
	} else if (enable_pages) {
		/* Calculate the pages arcs */
		for (tarc = i = 0; i < n_sites; ++i) {
			sites[i].arc = sites[i].pages * 360 / total_pages;
			tarc += sites[i].arc;
		}

		/* Compensate the first arc */
		sites[0].arc += 360 - tarc;

		draw_pie(im, 540, 100, 198);
	}

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

	strcpy(fname, url);

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

static void update_site(struct site *site, struct log *log, int whence)
{
	++site->hits;
	site->size += log->size;

	if (enable_pages || enable_visits)
		if (log->status == 200 && ispage(log->url) && isbrowser(log->who)) {
			++site->pages;
			if (enable_visits && db_put(site->ipdb, log->ip) == 0) {
				++site->visits;
				if (verbose)
					printf("%s: %s\n", site->name, log->ip);
			}
		}

	if (enable_topten) {
		char url[256];
		int len;

#if 1
		/* Ignore rippers.ca */
		if (strcmp(site->name, "rippers.ca") == 0)
			return;

		/* Ignore html */
		if (ispage(log->url))
			return;
#endif

		len = snprintf(url, sizeof(url), "%s%s", site->name, log->url);
		if (len > max_url)
			max_url = len;
		db_update_count(pages, url, log->size);
	}
}

static void process_log(struct log *log)
{
	struct list *l;
	int i;

	if (!in_range(log))
		return;

	/* Unqualified lines */
	if (*log->host == '-') {
		update_site(&sites[0], log, 0);
		return;
	}

	for (i = 1; i < n_sites; ++i)
		if (strstr(log->host, sites[i].name)) {
			update_site(&sites[i], log, 1);
			return;
		}

	if (strcmp(log->host, "yow") == 0 ||
	    strcmp(log->host, "localhost") == 0 ||
	    strncmp(log->host, "192.168.", 8) == 0 ||
	    strcmp(log->host, "127.0.0.1") == 0)
		return; /* don't count locals */

	/* lighttpd defaults to seanm.ca for everything else */
	update_site(&sites[0], log, 2);

#if 1
	if (strstr(log->host, "seanm.ca") == NULL) {
		for (l = others; l; l = l->next)
			if (strcmp(l->name, log->host) == 0)
				break;

		if (!l) {
			add_others(log->host);
			puts(log->host);
		}
	}
#endif
}

static void setup_sort(void)
{
	int i;

	++max_url; /* Room for NULL */
	for (i = 0; i < TOP_TEN; ++i) {
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
			if (n_top < TOP_TEN)
				++n_top;
			return;
		}

	if (n_top < TOP_TEN) {
		strcpy(top[n_top].name, key);
		top[n_top].size = size;
		++n_top;
	}
}

int main(int argc, char *argv[])
{
	int i;

	while ((i = getopt(argc, argv, "d:g:o:r:tvI:V")) != EOF)
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
		case 'p':
			enable_pages = 1;
			width = 642;
			offset = 150;
			break;
		case 'r':
			init_range(strtol(optarg, NULL, 10));
			break;
		case 't':
			enable_topten = 1;
			break;
		case 'v':
			++verbose;
			break;
		case 'I':
			add_list(optarg, &includes);
			break;
		case 'V':
			enable_visits = 1;
			width = 642;
			offset = 150;
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
	add_others("216.138.233.67:80");
	add_others("toronto-hs-216-138-233-67.s-ip.magma.ca");
	add_others("m38a1.ca");

	if (enable_topten) {
		pages = db_open("pages");
		if (!pages)
			exit(1);
	}

	if (enable_visits)
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
		parse_logfile(argv[i], process_log);
	}

	if (enable_visits)
		for (i = 0; i < n_sites; ++i)
			db_close(sites[i].name, sites[i].ipdb);

	range_fixup();

	if (enable_topten) {
		setup_sort();
		db_walk(pages, sort_pages);
		db_close("pages", pages);
	}

	/* Calculate the totals */
	for (i = 0; i < n_sites; ++i) {
		total_hits += sites[i].hits;
		total_pages += sites[i].pages;
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

	out_graphs();
	out_html(filename(outfile, NULL));
	out_txt(filename(outfile, ".txt"));

	return 0;
}
