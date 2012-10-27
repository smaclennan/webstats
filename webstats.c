#include "webstats.h"

#include <sys/utsname.h>

#include <gd.h>
#include <gdfontmb.h>
#include <gdfonts.h>

#define TOP_TEN 10

/* Visits takes no more time on YOW. */
static int enable_visits;
static int enable_pages;
static int enable_topten;
static int enable_daily;
static int draw_3d;
static int width = 422;
static int offset = 35;

static struct tm *yesterday;
static unsigned long y_hits;
static unsigned long y_size;

static char host[32];
static int default_host;

static int today; /* today as a yday */

static struct site {
	char *name;
	int color;
	int dark;
	unsigned long hits;
	unsigned long pages;
	unsigned long size;
	unsigned long arc;
	unsigned long visits;
	DB *ipdb;
} sites[] = {
	{ "seanm.ca", 0xff0000, 0x900000 }, /* must be first! */
	{ "rippers.ca", 0x000080,  0x000050 },
	{ "m38a1.ca", 0xffa500, 0xcf7500 },
	/* { "emacs", 0xffa500 }, */
	/* { "ftp.seanm.ca", 0x00ff00 }, */
};
static int n_sites = sizeof(sites) / sizeof(struct site);

struct list {
	char *name;
	struct list *next;
};

static struct point {
	int x, y;
	struct point *prev, *next;
} *points;

int verbose;

static struct list *includes;

static unsigned long total_hits;
static unsigned long total_pages;
static unsigned long total_size;
static unsigned long total_visits;

static char *outdir;
static char *outfile = "stats.html";
static char *outgraph = "pie.gif";

static DB *pages;
static int max_url;

static struct {
	char *name;
	unsigned long size;
} top[TOP_TEN];
static int n_top;

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

static void out_header(FILE *fp)
{
	/* Header proper */
	fprintf(fp,
		"<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\">\n"
		"<html lang=\"en\">\n"
		"<head>\n"
		"  <title>Statistics for %s</title>\n"
		"  <meta http-equiv=\"Content-type\" content=\"text/html;charset=utf-8\">\n"
		"  <style type=\"text/css\"> <!-- body { margin: 0 10%%; } --> </style>\n"
		"</head>\n", host);

	/* Body */
	fprintf(fp, "<body BGCOLOR=\"#E8E8E8\">\n");
	fprintf(fp, "<h2>Statistics for %s</h2>\n", host);
	fprintf(fp, "<small><strong>\n");
	/* Warning: cur_time/date has a local static for buffer */
	fprintf(fp, "Summary Period: %s", cur_date(min_date));
	fprintf(fp, " to %s (%d days)<br>\n", cur_date(max_date), days());
	fprintf(fp, "Generated %s<br>\n", cur_time(time(NULL)));
	if (yesterday)
		fprintf(fp, "Yesterday had %lu hits for %.1fM\n",
			y_hits, m(y_size));
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

static inline void out_count(unsigned long count, unsigned long total, FILE *fp)
{
	fprintf(fp, "<td align=right>%lu<td align=right>%.1f%%",
		count, (double)count * 100.0 / (double)total);
}

static void out_html(char *fname)
{
	int i;
	FILE *fp = fopen(fname, "w");
	if (!fp) {
		perror(fname);
		return;
	}

	out_header(fp);

	if (outgraph)
		fprintf(fp, "<p><img src=\"%s\" width=%d height=235 "
			"alt=\"Pie Charts\">\n\n",
			outgraph, width);

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
		fprintf(fp, "<tr><td>%s", sites[i].name);
		out_count(sites[i].hits, total_hits, fp);
		if (enable_pages)
			out_count(sites[i].pages, total_pages, fp);
		if (enable_visits)
			out_count(sites[i].visits, total_visits, fp);
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

	if (enable_daily)
		fprintf(fp, "<p><img src=\"daily.gif\" alt=\"Daily Graph\">\n");

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
			fprintf(fp, "<tr><td>%s<td align=right>%.1f\n",
				top[i].name, size);
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

static void dump_site(struct site *site, FILE *fp)
{
	fprintf(fp, "%-20s%6ld  %3.1f%%\t%6ld  %3.1f%%",
		site->name, site->hits,
		(double)site->hits * 100.0 / (double)total_hits,
		site->size / 1024,
		(double)site->size * 100.0 / (double)total_size);
	if (enable_visits)
		fprintf(fp, "\t%6ld  %3.1f%%",
			site->visits,
			(double)site->visits * 100.0 / (double)total_visits);
	fputc('\n', fp);
}

static void out_txt(char *fname)
{
	int i;
	FILE *fp = fopen(fname, "w");
	if (!fp) {
		perror(fname);
		return;
	}

	fprintf(fp, "Statistics for %s\n", host);
	fprintf(fp, "Summary Period: %s", cur_date(min_date));
	fprintf(fp, " to %s (%d days)\n", cur_date(max_date), days());
	fprintf(fp, "Generated %s\n\n", cur_time(time(NULL)));

	fputs("Site\t\t\t Hits\t\t     Size", fp);
	if (enable_visits)
		fputs("\t     Visits", fp);
	fputc('\n', fp);

	out_hr(fp);

	for (i = 0; i < n_sites; ++i)
		if (sites[i].hits)
			dump_site(&sites[i], fp);

	out_hr(fp);
	fprintf(fp, "%-20s%6ld      \t%6ld",
		"Totals", total_hits, total_size / 1024);
	if (enable_visits)
		fprintf(fp, "\t\t%6ld", total_visits);
	fputc('\n', fp);

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
	int i, s, e;

	if (draw_3d) {
		int j;

		s = 0;
		for (i = 0; i < n_sites; ++i) {
			if (sites[i].arc == 0)
				continue;

			color = getcolor(im, sites[i].dark);

			e = s + sites[i].arc;

			for (j = 10; j > 0; j--)
				gdImageFilledArc(im, cx, cy + j, size, size / 2,
						 s, e, color, gdArc);

			s = e;
		}

		/* correct for arc > 90 and < 180 */
		if (sites[0].arc > 90 && sites[0].arc < 180) {
			color = getcolor(im, sites[0].dark);

			for (j = 1; j < 10; ++j)
				gdImageFilledArc(im, cx, cy + j, size, size / 2,
						 0, sites[0].arc, color, gdArc);
		}
	}

	s = 0;
	for (i = 0; i < n_sites; ++i) {
		if (sites[i].arc == 0)
			continue;

		color = getcolor(im, sites[i].color);

		e = s + sites[i].arc;

		if (draw_3d)
			gdImageFilledArc(im, cx, cy, size, size / 2,
					 s, e, color, gdArc);
		else
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

	if (outgraph == NULL)
		return;

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
	for (i = 0; i < n_sites; ++i)
		if (sites[i].hits) {
			color = getcolor(im, sites[i].color);
			gdImageString(im, gdFontSmall, x, 220,
				      (unsigned char *)sites[i].name, color);
			x += 100;
		}

	for (tarc = 0, i = n_sites - 1; i > 0; --i) {
		sites[i].arc = sites[i].hits * 360 / total_hits;
		tarc += sites[i].arc;
	}

	/* Compensate the first arc */
	sites[0].arc = 360 - tarc;

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

#define ROUND 5000000
#define D_X 50
#define D_XDELTA 15
#define D_WIDTH (D_X + (32 - 1) * D_XDELTA + 20)

#define D_Y_HEIGHT 200
#define D_Y (D_Y_HEIGHT + 20)
#define D_HEIGHT (D_Y + 20)
#define D_Y_4 (D_Y_HEIGHT / 4)

#define D_MAXSTR_X 20
#define D_MAXSTR_Y (D_Y - D_Y_HEIGHT)

static DB *ddb;
static int max_daily, n_daily;

static void add_point(int x, int y)
{
	static struct point *tail;
	struct point *new = calloc(1, sizeof(struct point));
	if (!new) {
		puts("Out of memory!\n");
		exit(1);
	}

	new->x = x;
	new->y = y;

	if (points) {
		new->prev = tail;
		tail->next = new;
		tail = new;
	} else
		points = tail = new;
}

static void one_daily(char *key, void *data, int len)
{
	static int expected = 400;
	static int dx = D_X;
	int yday;

	char *p = strchr(key, '-');
	if (!p) {
		printf("Invalid timestr %s\n", key);
		return;
	}
	yday = strtol(p + 1, NULL, 10);

	if (yday == today)
		return;

	while (expected < yday) {
		++expected;
		++n_daily;
		dx += D_XDELTA;
	}

	/* Just add the data for now... we will correct later */
	add_point(dx, *(int *)data);

	expected = yday + 1;
	dx += D_XDELTA;
	++n_daily;

	if (*(unsigned long *)data > max_daily)
		max_daily = *(unsigned long *)data;
}

static void out_daily(void)
{
	struct point *point;
	int color, dcolor, width, dy;
	unsigned long daily_total = 0;
	char maxstr[10];
	gdImagePtr daily_im;

	if (!enable_daily)
		return;

	db_walk(ddb, one_daily);
	max_daily = ((max_daily + ROUND - 1) / ROUND) * ROUND;
	width = n_daily * D_XDELTA + D_X - D_XDELTA;

	if (!points) {
		puts("NO POINTS!");
		return;
	}

	/* Now correct the y points. */
	for (point = points; point; point = point->next) {
		double factor;
		factor = (double)point->y / (double)max_daily * D_Y_HEIGHT;
		daily_total += point->y;
		point->y = D_Y - factor;
	}

	/* Create the image */
	daily_im = gdImageCreate(D_WIDTH, D_HEIGHT);
	color = gdImageColorAllocate(daily_im, 0xff, 0xff, 0xff);
	gdImageColorTransparent(daily_im, color);

	/* Draw the 25% lines */
	color = gdImageColorAllocate(daily_im, 0xc0, 0xc0, 0xc0);
	for (dy = D_Y_4 + 20; dy < D_Y_HEIGHT; dy += D_Y_4)
		gdImageLine(daily_im, D_X, dy, width, dy, color);

	/* Draw the average */
	color = gdImageColorAllocate(daily_im, 0, 0, 0xff);
	double avg = (double)daily_total / (double)n_daily;
	double factor = avg / (double)max_daily * D_Y_HEIGHT;
	int y = D_Y - factor;
	gdImageLine(daily_im, D_X, y, width, y, color);
	gdImageLine(daily_im, D_X, y - 1, width, y - 1, color);
	snprintf(maxstr, sizeof(maxstr), "%dM", (unsigned)avg / 1000000);
	gdImageString(daily_im, gdFontMediumBold,
		      D_MAXSTR_X, D_Y - factor - 7,
		      (unsigned char *)maxstr, color);

	/* Draw lines */
	dcolor = gdImageColorAllocate(daily_im, 0xff, 0, 0);
	for (point = points->next; point; point = point->next)
		gdImageLine(daily_im,
			    point->prev->x, point->prev->y,
			    point->x, point->y,
			    dcolor);

	/* Draw box */
	color = gdImageColorAllocate(daily_im, 0, 0, 0);
	gdImageLine(daily_im, D_X, D_Y, width, D_Y, color);
	gdImageLine(daily_im, D_X, D_Y, D_X, D_Y - D_Y_HEIGHT, color);
	gdImageLine(daily_im, width, D_Y, width, D_Y - D_Y_HEIGHT, color);
	gdImageLine(daily_im, D_X, D_Y - D_Y_HEIGHT, width, D_Y - D_Y_HEIGHT,
		    color);

	/* Add the size scale */
	snprintf(maxstr, sizeof(maxstr), "%dM", max_daily / 1000000);
	gdImageString(daily_im, gdFontMediumBold,
		      D_MAXSTR_X, D_MAXSTR_Y,
		      (unsigned char *)maxstr, color);

	/* Draw dots last */
	for (point = points; point; point = point->next)
		gdImageFilledArc(daily_im, point->x, point->y,
				 5, 5, 0, 360, dcolor, gdArc);

	/* Save to file. */
	char *fname = filename("daily.gif", NULL);
	FILE *fp = fopen(fname, "wb");
	if (!fp) {
		perror(fname);
		exit(1);
	}
	gdImageGif(daily_im, fp);
	fclose(fp);

	/* Destroy it */
	gdImageDestroy(daily_im);

	db_close(ddb, "daily.db");
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

static void update_site(struct site *site, struct log *log)
{
	++site->hits;
	site->size += log->size;

	if (enable_daily) {
		char timestr[16];

		snprintf(timestr, sizeof(timestr), "%04d/%02d/%02d-%03d",
			 log->tm->tm_year + 1900,
			 log->tm->tm_mon, log->tm->tm_mday,
			 log->tm->tm_yday);

		db_update_count(ddb, timestr, log->size);
	}

	if (enable_pages && ispage(log))
		++site->pages;

	if (enable_visits && isvisit(log))
		if (db_put(site->ipdb, log->ip) == 0) {
			++site->visits;
			if (verbose)
				printf("%s: %s\n", site->name, log->ip);
		}

	if (enable_topten) {
		char url[256];
		int len;

		len = snprintf(url, sizeof(url), "%s%s", site->name, log->url);
		if (len > max_url)
			max_url = len;
		db_update_count(pages, url, log->size);
	}
}

static void process_log(struct log *log)
{
	int i;

	if (!in_range(log))
		return;

	if (yesterday &&
	    yesterday->tm_mon == log->tm->tm_mon &&
	    yesterday->tm_mday == log->tm->tm_mday) {
		++y_hits;
		y_size += log->size;
	}

	for (i = 1; i < n_sites; ++i)
		if (strstr(log->host, sites[i].name)) {
			update_site(&sites[i], log);
			return;
		}

	/* lighttpd defaults to `default_host' for everything else */
	update_site(&sites[default_host], log);
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

static void set_today(void)
{
	time_t now = time(NULL);
	struct tm *tm = localtime(&now);
	today = tm->tm_yday;
}

static void get_hostname(void)
{
	struct utsname uts;

	if (uname(&uts) == 0)
		snprintf(host, sizeof(host), uts.nodename);
	else
		strcpy(host, "yow");
}

static void set_default_host(void)
{	/* lighttpd specific */
	char def[128];

	if (get_default_host(def, sizeof(def))) {
			int i;
			for (i = 0; i < n_sites; ++i)
				if (strcmp(sites[i].name, def) == 0) {
					default_host = i;
					break;
				}

			/* default host gets red */
			if (default_host != 0) {
				sites[0].color = sites[default_host].color;
				sites[0].dark = sites[default_host].dark;
				sites[default_host].color = 0xff0000;
				sites[default_host].dark = 0x900000;
			}
	}
}

static void usage(char *prog, int rc)
{
	char *p = strrchr(prog, '/');
	if (p)
		prog = p + 1;

	printf("usage: %s [-htvDV] [-d outdir] [-g outgraph]"
	       " [-o outfile] [-r range]\n"
	       "\t\t[-I include] [logfile ...]\nwhere:"
	       "\t-h help\n"
	       "\t-t enable top ten\n"
	       "\t-v verbose\n"
	       "\t-D enable dailies\n"
	       "\t-V enable visits\n"
	       "Note: range is time in days\n"
	       "      \"-g none\" disables the graphs\n"
	       , prog);

	exit(rc);
}

int main(int argc, char *argv[])
{
	int i, had_hits;

	while ((i = getopt(argc, argv, "3d:g:ho:r:tvyDI:V")) != EOF)
		switch (i) {
		case '3':
			draw_3d = 1;
			break;
		case 'd':
			outdir = optarg;
			break;
		case 'g':
			if (strcmp(optarg, "none") == 0)
				outgraph = NULL;
			else
				outgraph = optarg;
			break;
		case 'h':
			usage(argv[0], 0);
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
		case 'y':
			yesterday = calc_yesterday();
			break;
		case 'D':
			enable_daily = 1;
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
			usage(argv[0], 1);
		}

	if (optind == argc) {
		puts("I need a logfile to parse!");
		exit(1);
	}

	get_hostname();

	set_default_host();

	if (enable_topten) {
		pages = db_open("pages");
		if (!pages)
			exit(1);
	}

	if (enable_visits)
		for (i = 0; i < n_sites; ++i) {
			sites[i].ipdb = db_open(sites[i].name);
			if (!sites[i].ipdb) {
				printf("Unable to open ip db\n");
				exit(1);
			}
		}

	if (enable_daily) {
		ddb = db_open("daily.db");
		if (!ddb) {
			printf("Unable to open daily db\n");
			exit(1);
		}
		set_today();
	}

	for (i = optind; i < argc; ++i) {
		if (verbose)
			printf("Parsing %s...\n", argv[i]);
		parse_logfile(argv[i], process_log);
	}

	if (enable_visits)
		for (i = 0; i < n_sites; ++i)
			db_close(sites[i].ipdb, sites[i].name);

	range_fixup();

	if (enable_topten) {
		setup_sort();
		db_walk(pages, sort_pages);
		db_close(pages, "pages");
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

	for (had_hits = i = 0; i < n_sites; ++i)
		if (sites[i].hits)
			++had_hits;

	if (had_hits > 1) /* graph of 100% for one site is boring */
		out_graphs();
	out_daily();
	out_html(filename(outfile, NULL));
	out_txt(filename(outfile, ".txt"));

	return 0;
}
