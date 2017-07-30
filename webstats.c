#include "webstats.h"

#include <sys/utsname.h>

#include <gd.h>
#include <gdfontmb.h>
#include <gdfonts.h>

/* Visits takes no more time on YOW. */
static int enable_visits;
static int enable_daily;
static int draw_3d;
static int width = 422;
static int offset = 35;
static int center_body;

static char host[32];
static int default_host;
static int show_bots;

static char *one_site;

static int today; /* today as a yday */

struct stats {
	unsigned long hits;
	unsigned long size;
	unsigned long visits;
	unsigned long visit_hits;
};

static struct tm *yesterday;
static struct stats ystats;

static struct stats total;

/* Optimized for size on a 32-bit system */
struct visit {
	char ip[16];
	time_t last_visit;
	unsigned good : 1;
	unsigned bot : 1;
	unsigned yesterday : 1;
	unsigned count : 29;
	struct visit *next;
};

static struct site {
	char *name;
	int color;
	int dark;
	int clickthru;
	unsigned long arc;
	struct stats stats;
	struct stats ystats;
	struct visit *visits;
} sites[] = {
#if 0
	{ "seanm.ca",	0xff0000, 0x900000, 0 }, /* must be first! */
//	{ "rippers.ca",	0x000080, 0x000050, 1 },
	{ "sam-i-am",   0xffffff, 0x000000 },
#else
	{ "seanm.ca",	0xff0000, 0x900000, 0 }, /* must be first! */
#endif
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

static unsigned long bots;

static char *outdir;
static char *outfile = "stats.html";
static char *outgraph = "pie.gif";

#define m(n)   (((double)(n)) / 1024.0 / 1024.0)
#define k(n)   (((double)(n)) / 1024.0)

static int db_update_long(void *dbh, const char *keystr, long update)
{
	return db_put_raw(dbh, keystr, strlen(keystr), &update, sizeof(long), 0);
}

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

static void out_header(FILE *fp, int had_hits)
{
	/* Header proper */
	fprintf(fp,
			"<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\">\n"
			"<html lang=\"en\">\n"
			"<head>\n"
			"  <title>Statistics for %s</title>\n"
			"  <meta http-equiv=\"Content-type\" content=\"text/html;charset=utf-8\">\n"
			"  <style type=\"text/css\">\n"
			"    <!--\n"
			"    body { margin: 0 10%%; }\n"
			"    table, th, td { border: 1px solid black; border-collapse: collapse; padding: 5 15; }\n"
			"    td { text-align: right; }\n"
			"    td.name { text-align: left; }\n"
			"    -->\n"
			"  </style>\n"
			"</head>\n", host);

	/* Body */
	fprintf(fp, "<body BGCOLOR=\"#C0C0C0\">\n");
	fprintf(fp, "<h2>Statistics for %s</h2>\n", host);
	fprintf(fp, "<small><strong>\n");
	/* Warning: cur_time/date has a local static for buffer */
	fprintf(fp, "Summary Period: %s", cur_date(min_date));
	fprintf(fp, " to %s (%d days)<br>\n", cur_date(max_date), days());
	fprintf(fp, "Generated %s\n", cur_time(time(NULL)));
	if (yesterday) {
		if (enable_visits)
			fprintf(fp, "<br>Yesterday had %lu hits, %lu visits, %lu visit hits, for %.1fM\n",
					ystats.hits, ystats.visits, ystats.visit_hits, m(ystats.size));
		else
			fprintf(fp, "<br>Yesterday had %lu hits for %.1fM\n",
					ystats.hits, m(ystats.size));
	}
	if (had_hits == 1) {
		if (enable_visits)
			fprintf(fp, "<br>Total %lu hits, %lu visits, %lu visit hits, for %.1fM\n",
					total.hits, total.visits, total.visit_hits, k(total.size));
		else
			fprintf(fp, "<br>Total %lu hits for %.1fM\n",
					total.hits, k(total.size));
	}
	if (show_bots)
		fprintf(fp, "<br>Bots %.0f%%\n", (double)bots * 100.0 / (double)total.hits);
	fprintf(fp, "</strong></small>\n");
	if (center_body) {
		if (had_hits != 1)
			fprintf(fp, "<hr>\n");
		fprintf(fp, "<center>\n\n");
	}
}

static void out_trailer(FILE *fp)
{
	/* trailer */
	if (center_body)
		fprintf(fp, "\n</center>\n</body>\n</html>\n");
	else
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

static inline void out_count(unsigned long count, unsigned long total, FILE *fp)
{
	if (count)
		fprintf(fp, "<td>%lu<td>%.0f%%",
			count, (double)count * 100.0 / (double)total);
	else
		fprintf(fp, "<td>0<td>0%%");
}

static void add_yesterday(FILE *fp)
{
	int i;

	fprintf(fp, "<tr><th colspan=%d>Yesterday\n", 5 + (enable_visits * 2));

	for (i = 0; i < n_sites; ++i) {
		if (sites[i].ystats.hits == 0)
			continue;
		fprintf(fp, "<tr><td class=\"name\">%s", sites[i].name);
		out_count(sites[i].ystats.hits, ystats.hits, fp);
		if (enable_visits)
			out_count(sites[i].ystats.visits, ystats.visits, fp);
		fprintf(fp, "<td>%.1f<td>%.0f%%\n",
			(double)sites[i].ystats.size / 1024.0 / 1024.0,
			(double)sites[i].ystats.size * 100.0 / (double)ystats.size);
	}

	fprintf(fp, "<tr><td>Totals<td>%ld<td>&nbsp;", ystats.hits);
	if (enable_visits)
		fprintf(fp, "<td>%ld<td>&nbsp;", ystats.visits);
	fprintf(fp, "<td>%.1f<td>&nbsp;\n", (double)ystats.size / 1024.0 / 1024.0);

//	fprintf(fp, "</table>\n");
}

static void out_html(char *fname, int had_hits)
{
	int i;
	FILE *fp = fopen(fname, "w");
	if (!fp) {
		perror(fname);
		return;
	}

	out_header(fp, had_hits);

	if (outgraph)
		fprintf(fp, "<p><img src=\"%s\" width=%d height=235 "
			"alt=\"Pie Charts\">\n\n",
			outgraph, width);

	if (had_hits > 1) {
		fprintf(fp, "<p><table summary=\"Satistics.\">\n");

		fputs("<tr><th>Site<th colspan=2>Hits", fp);
		if (enable_visits)
			fputs("<th colspan=2>Visits", fp);
		fputs("<th colspan=2>Size (M)\n", fp);

		for (i = 0; i < n_sites; ++i) {
			if (sites[i].stats.hits == 0)
				continue;
			fprintf(fp, "<tr><td class=\"name\">%s", sites[i].name);
			out_count(sites[i].stats.hits, total.hits, fp);
			if (enable_visits)
				out_count(sites[i].stats.visits, total.visits, fp);
			fprintf(fp, "<td>%.1f<td>%.0f%%\n",
				(double)sites[i].stats.size / 1024.0,
				(double)sites[i].stats.size * 100.0 / (double)total.size);
		}

		fprintf(fp, "<tr><td>Totals<td>%ld<td>&nbsp;", total.hits);
		if (enable_visits)
			fprintf(fp, "<td>%ld<td>&nbsp;", total.visits);
		fprintf(fp, "<td>%.1f<td>&nbsp;\n", (double)total.size / 1024.0);

		if (yesterday && had_hits > 1)
			add_yesterday(fp);

		fprintf(fp, "</table>\n");
	}

	if (enable_daily)
		fprintf(fp, "<p><img src=\"daily.gif\" alt=\"Daily Graph\">\n");

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
	if (enable_visits)
		fputs("----------------", fp);
	fputs("\n", fp);
}

static void dump_site(struct site *site, struct stats *stats, struct stats *totals, FILE *fp)
{
	fprintf(fp, "%-20s%6ld  %3.0f%%",
			site->name, stats->hits,
			(double)stats->hits * 100.0 / (double)totals->hits);
	if (enable_visits)
		fprintf(fp, "\t%6ld  %3.0f%%",
				stats->visits,
				(double)stats->visits * 100.0 / (double)totals->visits);
	if (totals == &ystats)
		fprintf(fp, "\t%5.1f  %3.0f%%\n",
				(double)stats->size / 1024.0 / 1024.0,
				(double)stats->size * 100.0 / (double)totals->size);
	else
		fprintf(fp, "\t%5.1f  %3.0f%%\n",
				(double)stats->size / 1024.0,
				(double)stats->size / (double)totals->size);
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
	fprintf(fp, "Generated %s\n", cur_time(time(NULL)));
	if (yesterday) {
		if (enable_visits)
			fprintf(fp, "Yesterday had %lu hits, %lu visits, for %.1fM\n",
					ystats.hits, ystats.visits, m(ystats.size));
		else
			fprintf(fp, "Yesterday had %lu hits for %.1fM\n",
					ystats.hits, m(ystats.size));
	}
	if (show_bots)
		fprintf(fp, "Bots %.0f%%\n", (double)bots * 100.0 / (double)total.hits);
	fputs("\n", fp);

	fputs("Site\t\t\t Hits", fp);
	if (enable_visits)
		fputs("\t\t     Visits", fp);
	fputs("\t     Size\n", fp);

	out_hr(fp);

	for (i = 0; i < n_sites; ++i)
		if (sites[i].stats.hits)
			dump_site(&sites[i], &sites[i].stats, &total, fp);

	out_hr(fp);

	fprintf(fp, "%-20s%6ld      ", "Totals", total.hits);
	if (enable_visits)
		fprintf(fp, "\t%6ld\t", total.visits);
	fprintf(fp, "\t%5.1f\n", (double)total.size / 1024.0);

	if (yesterday) {
		fprintf(fp, "\n%38s\n", "Yesterday");
		out_hr(fp);

		for (i = 0; i < n_sites; ++i)
			if (sites[i].ystats.hits)
				dump_site(&sites[i], &sites[i].ystats, &ystats, fp);

		out_hr(fp);
		fprintf(fp, "%-20s%6ld      ", "Totals", ystats.hits);
		if (enable_visits)
			fprintf(fp, "\t%6ld", ystats.visits);
		fprintf(fp, "\t\t%5.1f\n", (double)ystats.size / 1024.0 / 1024.0);
	}

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
		if (sites[i].stats.hits) {
			color = getcolor(im, sites[i].color);
			gdImageString(im, gdFontSmall, x, 220,
				      (unsigned char *)sites[i].name, color);
			x += 100;
		}

	for (tarc = 0, i = n_sites - 1; i > 0; --i) {
		sites[i].arc = sites[i].stats.hits * 360 / total.hits;
		tarc += sites[i].arc;
	}

	/* Compensate the first arc */
	sites[0].arc = 360 - tarc;

	draw_pie(im, 100, 100, 198);

	/* Calculate the size arcs */
	for (tarc = i = 0; i < n_sites; ++i) {
		sites[i].arc = sites[i].stats.size * 360 / total.size;
		tarc += sites[i].arc;
	}

	/* Compensate the first arc */
	sites[0].arc += 360 - tarc;

	draw_pie(im, 320, 100, 198);

	if (enable_visits) {
		/* Calculate the visit arcs */
		for (tarc = i = 0; i < n_sites; ++i) {
			sites[i].arc = sites[i].stats.visits * 360 / total.visits;
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

//#define ROUND 4000000
#define ROUND 1000000
#define D_X 50
#define D_XDELTA 15
#define D_WIDTH (D_X + (32 - 1) * D_XDELTA + 20)

#define D_Y_HEIGHT 200
#define D_Y (D_Y_HEIGHT + 20)
#define D_HEIGHT (D_Y + 20)
#define D_Y_4 (D_Y_HEIGHT / 4)

#define D_MAXSTR_X 15
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

static int one_daily(const char *key, void *data, int len)
{
	static int expected = 400;
	static int dx = D_X;
	int yday;

	char *p = strchr(key, '-');
	if (!p) {
		printf("Invalid timestr %s\n", key);
		return 0;
	}
	yday = strtol(p + 1, NULL, 10);

	if (yday == today)
		return 0;

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

	return 0;
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
	color = gdImageColorAllocate(daily_im, 0xb0, 0xb0, 0xb0);
	for (dy = D_Y_4 + 20; dy < D_Y_HEIGHT; dy += D_Y_4)
		gdImageLine(daily_im, D_X, dy, width, dy, color);

	/* Draw the average */
	color = gdImageColorAllocate(daily_im, 0, 0, 0xff);
	double avg = (double)daily_total / (double)n_daily;
	double factor = avg / (double)max_daily * D_Y_HEIGHT;
	int y = D_Y - factor;
	gdImageLine(daily_im, D_X, y, width, y, color);
	gdImageLine(daily_im, D_X, y - 1, width, y - 1, color);
	snprintf(maxstr, sizeof(maxstr), "%.1fM", (double)avg / 1000000.0);
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

static void count_visits(struct site *site)
{
	struct visit *v;

	for (v = site->visits; v; v = v->next)
		if (v->bot)
			bots += v->count;
		else if (v->good) {
			site->stats.visits++;
			site->stats.visit_hits += v->count;

			if (v->yesterday) {
				site->ystats.visits++;
				site->ystats.visit_hits += v->count;
			}
		}

	total.visits += site->stats.visits;
	total.visit_hits += site->stats.visit_hits;
	ystats.visits += site->ystats.visits;
	ystats.visit_hits += site->ystats.visit_hits;
}

static void add_visit(struct site *site, struct log *log, int is_yesterday)
{
	struct visit *v;

	for (v = site->visits; v; v = v->next)
		if (strcmp(v->ip, log->ip) == 0) {
			if (abs(v->last_visit - log->time) < VISIT_TIMEOUT)
				/* Add to current visit */
				goto update_visit;
			else
				break; /* new visit */
		}

	v = calloc(1, sizeof(struct visit));
	if (!v) {
		puts("add_visit: Out of memory");
		exit(1);
	}

	snprintf(v->ip, sizeof(v->ip), "%s", log->ip);

	/* Newest visits must be at the head */
	v->next = site->visits;
	site->visits = v;

update_visit:
	/* favicon.ico does not count in good status */
	if (valid_status(log->status) && strcmp(log->url, "/favicon.ico")) {
		v->good = 1;
		++v->count;
	}
	if (is_yesterday)
		v->yesterday = 1;

	/* If they ask for robots.txt, assume it is a bot. */
	if (strcmp(log->url, "/robots.txt") == 0)
		v->bot = 1;

	v->last_visit = log->time;
}

static void update_site(struct site *site, struct log *log)
{
	int is_yesterday = time_equal(yesterday, log->tm);
	int ip_ignore = ignore_ip(log->ip);

	if (ip_ignore == 1)
		return; /* local ignore */

	if (one_site && strcmp(site->name, one_site))
		return;

	++site->stats.hits;
	site->stats.size += log->size;

	if (ip_ignore)
		return;

	if (is_yesterday) {
		++ystats.hits;
		ystats.size += log->size;

		++site->ystats.hits;
		site->ystats.size += log->size;
	}

	if (enable_daily) {
		char timestr[16];

		snprintf(timestr, sizeof(timestr), "%04d/%02d/%02d-%03d",
			 log->tm->tm_year + 1900,
			 log->tm->tm_mon, log->tm->tm_mday,
			 log->tm->tm_yday);

		db_update_long(ddb, timestr, log->size);
	}

	if (isbot(log)) {
		++bots;
		return;
	}

	if (enable_visits) {
		if (strcmp(log->method, "GET") && strcmp(log->method, "HEAD"))
			return;

		if (site->clickthru && isdefault(log))
			return;

		add_visit(site, log, is_yesterday);
	}
}

static void process_log(struct log *log)
{
	int i;

	if (!in_range(log))
		return;

	for (i = 0; i < n_sites; ++i)
		if (i != default_host && strstr(log->host, sites[i].name)) {
			update_site(&sites[i], log);
			return;
		}

	/* lighttpd defaults to `default_host' for everything else */
	update_site(&sites[default_host], log);
}

static void set_today(void)
{
	time_t now = time(NULL);
	struct tm *tm = gmtime(&now);
	today = tm->tm_yday;
}

static void get_hostname(void)
{
	struct utsname uts;

	if (uname(&uts) == 0)
		snprintf(host, sizeof(host), "%s", uts.nodename);
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

static void send_email(const char *addr)
{
	char cmd[256];
	time_t now = time(NULL);
	struct tm *tm = localtime(&now);

	snprintf(cmd, sizeof(cmd), "mail -s 'YOW stats for %d/%d/%d' %s < %s/stats.txt",
			 tm->tm_year + 1900, tm->tm_mon, tm->tm_mday, addr,
			 outdir ? outdir : ".");
	system(cmd);
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
	char *email = NULL;

	while ((i = getopt(argc, argv, "3bcd:e:g:hi:n:o:r:s:vyDEI:V")) != EOF)
		switch (i) {
		case '3':
			draw_3d = 1;
			break;
		case 'b':
			show_bots = 1;
			break;
		case 'c':
			center_body = 1;
			break;
		case 'd':
			outdir = optarg;
			break;
		case 'e':
			email = optarg;
			break;
		case 'g':
			if (strcmp(optarg, "none") == 0)
				outgraph = NULL;
			else
				outgraph = optarg;
			break;
		case 'h':
			usage(argv[0], 0);
		case 'i':
			add_ip_ignore(optarg);
			break;
		case 'n':
			i = strtol(optarg, NULL, 0);
			if (i < n_sites)
				n_sites = i;
			break;
		case 'o':
			outfile = optarg;
			break;
		case 'r':
			init_range(strtol(optarg, NULL, 10));
			break;
		case 's':
			one_site = optarg;
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
		case 'E':
			show_bots = 1;
			enable_daily = 1;
			enable_visits = 1;
			width = 642;
			offset = 150;
			yesterday = calc_yesterday();
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

	if (enable_daily) {
		ddb = stats_db_open("daily.db");
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
			count_visits(&sites[i]);

	range_fixup();

	/* Calculate the totals */
	for (i = 0; i < n_sites; ++i) {
		total.hits += sites[i].stats.hits;
		sites[i].stats.size /= 1024; /* convert to k */
		total.size += sites[i].stats.size;
	}
	/* Make sure there are no /0 errors */
	if (total.hits == 0)
		total.hits = 1;
	if (total.visits == 0)
		total.visits = 1;
	if (total.size == 0)
		total.size = 1;

	for (had_hits = i = 0; i < n_sites; ++i)
		if (sites[i].stats.hits)
			++had_hits;

	if (had_hits <= 1)
		outgraph = NULL; /* graph of 100% for one site is boring */

	out_graphs();
	out_daily();
	out_html(filename(outfile, NULL), had_hits);
	out_txt(filename(outfile, ".txt"));

	if (enable_daily)
		stats_db_close(ddb, "daily.db");

	if (email)
		send_email(email);

	return 0;
}
