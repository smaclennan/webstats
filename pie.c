#include "webstats.h"

#include <sys/utsname.h>

#include <gd.h>
#include <gdfontmb.h>
#include <gdfonts.h>

static int draw_3d = 1;
static int width = 422;
static int offset = 35;

static struct site {
	char *name;
	int color;
	int dark;
	unsigned long arc;
} sites[] = {
	{ "seanm.ca",	0xff0000, 0x900000 }, /* must be first! */
	{ "m38a1.ca",	0x8d9e83, 0x7d8e73 },
	{ "emacs",	0xffa500, 0xcf7500 },
	{ "rippers.ca",	0x000080, 0x000050 },
	/* { "ftp.seanm.ca", 0x00ff00 }, */
};
static int n_sites = sizeof(sites) / sizeof(struct site);

int verbose;


static char *outdir;
static char *outgraph = "pie.gif";

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

static inline void out_count(unsigned long count, unsigned long total, FILE *fp)
{
	fprintf(fp, "<td align=right>%lu<td align=right>%.1f%%",
		count, (double)count * 100.0 / (double)total);
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
			color = getcolor(im, sites[i].dark);

			e = s + 90;

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
		color = getcolor(im, sites[i].color);

		e = s + 90;

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
	int i, color;
	int x;

	if (outgraph == NULL)
		return;

	gdImagePtr im = gdImageCreate(width, 235);
	color = gdImageColorAllocate(im, 0xc0, 0xc0, 0xc0); /* background */
	gdImageColorTransparent(im, color);

	color = gdImageColorAllocate(im, 0, 0, 0); /* text */

	gdImageString(im, gdFontMediumBold, 87, 203,
		      (unsigned char *)"Hits", color);
	gdImageString(im, gdFontMediumBold, 305, 203,
		      (unsigned char *)"Bytes", color);
	gdImageString(im, gdFontMediumBold, 522, 203,
		      (unsigned char *)"Visits", color);

	x = offset;
	for (i = 0; i < n_sites; ++i) {
			color = getcolor(im, sites[i].color);
			gdImageString(im, gdFontSmall, x, 220,
				      (unsigned char *)sites[i].name, color);
			x += 100;
		}

	draw_pie(im, 100, 100, 198);

	draw_pie(im, 320, 100, 198);

	draw_pie(im, 540, 100, 198);

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

int main(int argc, char *argv[])
{
	out_graphs();

	return 0;
}

/*
 * Local Variables:
 * compile-command: "gcc -O3 -Wall pie.c -o pie -lgd"
 * End:
 */
