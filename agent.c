/*
 * agent - produce the agent output file used in seanm.ca
 * Copyright (C) 2002  Sean MacLennan <seanm@seanm.ca>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>


/*
 * Done - keep track of first/last date
 * Done - make basedir configurable
 * Done - get rid of ignored
 * Done - the crawlers in a file
 * TODO - more dynamic allocation
 * TODO - deal with timezone
 * TODO - downcase the name for matching
 * TODO - file for OS matching
 */


// YOW - you probably want to change this
#define BASEDIR		"/var/www/seanm.ca/stats"

#define OS_STATS_FILE	"os.html"
#define UNKNOWN_FILE	"unknown.html"
#define ALL_FILE	"all.html"


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
	{ "/thetruthfulfew.com/", 0 }, { "/www.thetruthfulfew.com/", 0 }
};
#define NUMURLS (sizeof(urllist) / sizeof(struct urlstr))


char *defbots[] = {
	// Bots/Spiders/Crawlers
	"Googlebot",
	"FAST-WebCrawler",
	"Jeeves",
	"ia_archiver",
	"ArchitextSpider",
	"Openbot",
	"Slurp",
	"Scooter",
	"scooter",
	"cosmos",
	"Netcraft Web Server Survey",
	"larbin",
	"http://www.almaden.ibm.com/",
	"Mercator",
	"moget",
	"ZyBorg",
	"LexiBot",
	"ipium",
	"IPiumBot",
	"gazz",
	// spam tool
	"Microsoft URL Control",
	// Link Checkers
	"LinkWalker",
	"Link Sleuth", // Xenu
	// Web page validators
	"W3C_Validator",
	"W3C_CSS_Validator",
};
#define NBOTS	(sizeof(bots) / sizeof(char*))

char **bots;
int nbots = 0;

typedef struct {
	char *name;
	int hits;
	int files;
	int pages;
	int group;
	int unknown;
} NameCount;


#define MAX_AGENTS	3000
NameCount agents[MAX_AGENTS];
int n_agents = 0;

#define MAX_OS 100
NameCount os[MAX_OS];
int n_os = 0;

#define MAX_UNKNOWN 200
NameCount unknowns[MAX_UNKNOWN];
int n_unknown = 0;

#define NONE		-1
#define WINDOZE		0
#define UNIX		1
#define JAVA        2
#define OTHER		3
NameCount groups[] = {
	{ .name = "Microsoft" },
	{ .name = "Unix" },
	{ .name = "Java/Perl" },
	{ .name = "Other" }
};
#define N_GROUPS (sizeof(groups) / sizeof(NameCount))

#define OTHER_BROWSER		0
#define MSIE			1
#define NETSCAPE		2
#define BOTS			3
NameCount browsers[] = {
	{ .name = "Everybody Else(tm)" },
	{ .name = "Internet Explorer" },
	{ .name = "Mozilla" },
	{ .name = "Bots" },
};
#define N_BROWSERS (sizeof(browsers) / sizeof(NameCount))

/* The bot index *after* sorting */
int bot_index;


#define EXPLORER	0
#define MOZILLA		1
NameCount bgroups[] = {
	{ .name = "Internet Explorer" },
	{ .name  = "Mozilla" }
};
#define N_BGROUPS (sizeof(bgroups) / sizeof(NameCount))

int totalhits;
int totalfiles;
int totalpages;

int os_hits;
int os_files;
int os_pages;

int verbose;

time_t min_date = 0x7fffffff, max_date = 0;

void badline(char *line, char *p, int n)
{
	printf("BAD LINE %d\n", n);
	printf("%s", line);
	if(p) printf("%s", p);

//	exit(1);
}

void process_file(FILE *fp);
void add_agent(char *agent, int file, int page);
int  parse_agent(NameCount *agent);
void sort_oses();
char *cur_time();
int  parse_date(char *date);
void addbot(char *bot);


double percent_hits(int hits)
{
	return (double)hits * 100.0 / (double)totalhits;
}

double percent_files(int files)
{
	return (double)files * 100.0 / (double)totalfiles;
}

double percent_pages(int pages)
{
	return (double)pages * 100.0 / (double)totalpages;
}

double percent_browser_hits(int hits)
{
	return (double)hits * 100.0 /
		(double)(totalhits - browsers[bot_index].hits);
}

double percent_browser_files(int files)
{
	return (double)files * 100.0 /
		(double)(totalfiles - browsers[bot_index].files);
}

double percent_browser_pages(int pages)
{
	return (double)pages * 100.0 /
		(double)(totalpages - browsers[bot_index].pages);
}

double percent_os_hits(int hits)
{
	return (double)hits * 100.0 / (double)os_hits;
}

double percent_os_files(int files)
{
	return (double)files * 100.0 / (double)os_files;
}

double percent_os_pages(int pages)
{
	return (double)pages * 100.0 / (double)os_pages;
}

void out_text();
void out_html();

enum {
	COMPARE_HITS,
	COMPARE_FILES,
	COMPARE_PAGES,
} compare_type;


void usage()
{
	printf("usage: agent [-b basedir] [-f | -h] log_file ...\n");
	exit(1);
}


static void init_urllist(void)
{
	int i;

	for (i = 0; i < NUMURLS; ++i)
		urllist[i].len = strlen(urllist[i].str);
}


int main(int argc, char *argv[])
{
	FILE *fp;
	int arg, c, i;
	char *basedir = BASEDIR;
	char *botfile = "./botfile";
	char *outfile;


	compare_type = COMPARE_HITS;

	while((c = getopt(argc, argv, "b:c:fhp")) != -1)
		switch(c) {
		case 'b': basedir = optarg; break;
		case 'c': botfile = optarg; break;
		case 'f': compare_type = COMPARE_FILES; break;
		case 'h': compare_type = COMPARE_HITS; break;
		case 'p': compare_type = COMPARE_PAGES; break;
		case 'o': outfile = optarg; break;
		default:  usage();
		}

	if(optind == argc) usage();

	if(chdir(basedir)) {
		perror(basedir);
		exit(1);
	}

	init_urllist();

	if((fp = fopen(botfile, "r"))) {
		char line[1024], *p;

		while(fgets(line, sizeof(line), fp)) {
			if(*line == '#') continue;
			if((p = strchr(line, '\n'))) *p = '\0';
			addbot(line);
		}

		fclose(fp);
	}
	else {
		// setup some default bots
		printf("Using default bots (%d)\n", errno);
		for(i = 0; i < NBOTS; ++i)
			addbot(defbots[i]);
	}

	for(arg = optind; arg < argc; ++arg)
		if(strcmp(argv[arg], "-") == 0)
			process_file(stdin);
		else if(!(fp = fopen(argv[arg], "r")))
			perror(argv[arg]);
		else {
			process_file(fp);
			fclose(fp);
		}

	for(i = 0; i < n_agents; ++i)
		if(!parse_agent(&agents[i]))
			agents[i].unknown = 1;

	sort_oses();

	out_html();

	return 0;
}


static int greater(NameCount *group, NameCount *os)
{
	switch(compare_type) {
	case COMPARE_FILES:
		return group->files >= os->files;
	case COMPARE_PAGES:
		return group->pages >= os->pages;
	case COMPARE_HITS:
	default:
		return group->hits >= os->hits;
	}
}

void out_html()
{
	FILE *fp;
	int i, n, cur_group = 0;
	int need_unknown = 0;

	if(!(fp = fopen(OS_STATS_FILE, "w"))) {
		perror(OS_STATS_FILE);
		exit(1);
	}

	// header
	fprintf(fp,
			"<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\">\n");
	fprintf(fp, "<html lang=\"en\">\n<head>\n");
	fprintf(fp, "<title>Agent Statistics for YOW</title>\n");
	fprintf(fp, "<link rel=\"stylesheet\" href=\"style-sheet\">\n");
	fprintf(fp, "</head>\n");
	fprintf(fp, "<body BGCOLOR=\"#E8E8E8\" TEXT=\"#000000\" "
			"LINK=\"#0000FF\" VLINK=\"#FF0000\">\n");
	fprintf(fp, "<h2>Agent Statistics for yow</h2>\n");
	fprintf(fp, "<small><strong>\n");
	// Warning: cur_time has a local static for buffer
	fprintf(fp, "Summary Period: %s", cur_time(min_date));
	fprintf(fp, " to %s<br>\n", cur_time(max_date));
	fprintf(fp, "Generated %s<br>\n", cur_time(time(NULL)));
	switch(compare_type) {
	case COMPARE_HITS: fprintf(fp, "Sorted by hits.<br>\n"); break;
	case COMPARE_FILES: fprintf(fp, "Sorted by files.<br>\n"); break;
	case COMPARE_PAGES: fprintf(fp, "Sorted by pages.<br>\n"); break;
	}
	fprintf(fp, "</strong></small>\n<hr>\n");
	fprintf(fp, "<center>\n");

	fprintf(fp, "<p><table WIDTH=\"80%%\" BORDER=2 CELLSPACING=1 CELLPADDING=1\n");
	fprintf(fp, " summary=\"Summary of statistics for the period.\">\n");
	fprintf(fp, "<tr><th colspan=2>Summary\n");
	fprintf(fp, "<tr><td class=text>Total Hits<td class=n>%d\n", totalhits);
	fprintf(fp, "<tr><td class=text>Total Files<td class=n>%d\n", totalfiles);
	fprintf(fp, "<tr><td class=text>Total Pages<td class=n>%d\n", totalpages);
	fprintf(fp, "<tr><td class=text>Total Unique Agents<td class=n>%d\n",
			n_agents);
	fprintf(fp, "<tr><td class=text>Total Unknown Agents<td class=n>%d\n",
			n_unknown);
	fprintf(fp, "<tr><td class=text>Total Unique OSes<td class=n>%d\n",
			n_os);
	fprintf(fp, "</table>\n");

	fprintf(fp, "<p><table WIDTH=\"80%%\" BORDER=2 CELLSPACING=1 CELLPADDING=1\n");
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
		browsers[bot_index].hits, percent_hits(browsers[bot_index].hits),
		browsers[bot_index].files, percent_files(browsers[bot_index].files),
		browsers[bot_index].pages, percent_pages(browsers[bot_index].pages),
		browsers[bot_index].name);
	fprintf(fp, "</table>\n");

	fprintf(fp, "<p><table WIDTH=\"80%%\" BORDER=2 CELLSPACING=1 CELLPADDING=1\n");
	fprintf(fp, " summary=\"Detailed statistics about browsers used.\">\n");
	fprintf(fp, "<tr><th colspan=8>Browsers\n");
	fprintf(fp, "<tr><th class=day>#<th class=hits colspan=2>Hits\n"
			"<th class=files colspan=2>Files\n"
			"<th class=pages colspan=2>Pages\n"
			"<th class=name>Browser\n");
	for(i = 0; i < N_BROWSERS; ++i) {
		if (i == bot_index)
			continue;
		if(browsers[i].hits == 0)
			continue;
		fprintf(fp, "<tr><td class=day>%d"
				"<td class=n>%d<td>%.0f%%"
				"<td class=n>%d<td>%.0f%%"
				"<td class=n>%d<td>%.0f%%\n"
				"<td class=text>%s\n",
				i + 1,
				browsers[i].hits, percent_browser_hits(browsers[i].hits),
				browsers[i].files, percent_browser_files(browsers[i].files),
				browsers[i].pages, percent_browser_pages(browsers[i].pages),
				browsers[i].name);
	}
	fprintf(fp, "</table>\n");

	fprintf(fp, "<p><table WIDTH=\"80%%\" BORDER=2 CELLSPACING=1 CELLPADDING=1\n");
	fprintf(fp, " summary=\"Detailed statistics about operating systems used.\">\n");
    fprintf(fp, "<tr><th colspan=8>Operating Systems\n");
	fprintf(fp, "<tr><th class=day>#<th class=hits colspan=2>Hits"
			"<th class=files colspan=2>Files"
			"<th class=pages colspan=2>Pages"
			"<th class=name>OS\n");
	cur_group = 0;
	for(i = 0, n = 1; i < n_os; ++i, ++n) {
		while(cur_group < N_GROUPS && greater(&groups[cur_group], &os[i])) {
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
			fprintf(fp, "<td class=text>%s\n", groups[cur_group].name);
			++cur_group;
		}
//		if(percent_pages(os[i].pages) >= 1.0)
		{
			fprintf(fp, "<tr><td class=day>%d"
					"<td class=n>%d<td>%.1f%%"
					"<td class=n>%d<td>%.1f%%"
					"<td class=n>%d<td>%.1f%%\n",
					n,
					os[i].hits, percent_hits(os[i].hits),
					os[i].files, percent_files(os[i].files),
					os[i].pages, percent_pages(os[i].pages));
			if(strcmp(os[i].name, "Unknown") == 0) {
				need_unknown = 1;
				fprintf(fp, "<td class=text><a href=\"unknown.html\">Unknown</a>\n");
			}
			else
				fprintf(fp, "<td class=text>%s\n", os[i].name);
		}
	}

	// Just in case
	while(cur_group < N_GROUPS) {
		fprintf(fp, "<tr bgcolor=\"#D0D0E0\"><td class=day>%d"
				"<td class=n>%d<td>%.1f%%",
				n, groups[cur_group].hits,
				percent_hits(groups[cur_group].hits));
		fprintf(fp, "<td class=n>%d<td>%.1f%%\n",
				groups[cur_group].files,
				percent_files(groups[cur_group].files));
		fprintf(fp, "<td class=n>%d<td>%.1f%%\n",
				groups[cur_group].pages,
				percent_pages(groups[cur_group].pages));
		fprintf(fp, "<td class=text>%s\n", groups[cur_group].name);
		++cur_group;
		if(groups[cur_group].name == NULL) ++cur_group; // skip NONE
	}

	fprintf(fp, "</table>\n\n");

	fprintf(fp, "<hr>\n");
	fprintf(fp, "<small><a href=\"index.html\">[Back]</a>"
			"&nbsp;<a href=\"agent-help.html\">[Help]</a></small>\n");

	// trailer
	fprintf(fp, "</center></body>\n</html>\n");

	fclose(fp);

#if 0
	if(!(fp = fopen(ALL_FILE, "w"))) {
		perror(ALL_FILE);
		exit(1);
	}

	// header
	fprintf(fp, "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\">\n");
	fprintf(fp, "<html>\n<head>");
	fprintf(fp, "<title>Unknown agents for YOW</title>");
	fprintf(fp, "</head>\n<body bgcolor=\"#e8e8e8\">\n");
	fprintf(fp, "<center><h1>Unknown agents for YOW</h1>");
	fprintf(fp, "<p><table WIDTH=510 BORDER=2 CELLSPACING=1 CELLPADDING=1>\n");
	fprintf(fp, "<tr><th colspan=2>Unknown\n");
	for(i = 0; i < n_agents; ++i) {
		char *p = agents[i].name;
		while(isspace(*p)) ++p;
		if(*p)
			fprintf(fp, "<tr><td bgcolor=\"#%s\">&nbsp;<td>%s<td align=right>%d\n",
					agents[i].unknown ? "ff0000" : "4fa83f",
					agents[i].name, agents[i].hits);
		else
			fprintf(fp, "<tr><td bgcolor=\"#ff0000\">&nbsp;<td>(empty)<td align=right>%d\n",
					agents[i].hits);
	}
	fprintf(fp, "</table>\n");

	fprintf(fp, "<p><a href=\"os.html\">Back</a>\n");

	// trailer
	fprintf(fp, "</center></body>\n</html>\n");

	fclose(fp);
#endif

	if(!need_unknown) return;

	if(!(fp = fopen(UNKNOWN_FILE, "w"))) {
		perror(UNKNOWN_FILE);
		exit(1);
	}

	// header
	fprintf(fp, "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\">\n");
	fprintf(fp, "<html>\n<head>");
	fprintf(fp, "<title>Unknown agents for YOW</title>");
	fprintf(fp, "</head>\n<body bgcolor=\"#e8e8e8\">\n");
	fprintf(fp, "<center><h1>Unknown agents for YOW</h1>");
	fprintf(fp, "<p><table BORDER=2 CELLSPACING=1 CELLPADDING=1>\n");
	for(i = 0; i < n_unknown; ++i) {
		char *p = unknowns[i].name;
		while(isspace(*p)) ++p;
		if(*p)
			fprintf(fp, "<tr><td>%s<td align=right>%d\n",
					unknowns[i].name, unknowns[i].hits);
		else
			fprintf(fp, "<tr><td>(empty)<td align=right>%d\n",
					unknowns[i].hits);
	}
	fprintf(fp, "</table>\n");

	fprintf(fp, "<p><a href=\"os.html\">Back</a>\n");

	// trailer
	fprintf(fp, "</center></body>\n</html>\n");

	fclose(fp);
}

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

void process_file(FILE *fp)
{
	char line[4096], *p, *e, *url;
	int status, file, page, nline = 0;

	// We only want the last string - the agent
	// I lied, we want the date too...
	// I lied again, we want the GET (not HEAD) and status
	while(fgets(line, sizeof(line) - 1, fp)) {
		++nline;
		if(!(p = strchr(line, '[')) ||
		   !(e = strchr(p, ']'))) {
			printf("DATE MISSING line %d\n", nline);
			printf("\t%s", line);
			continue;
		}
		*e++ = '\0';
		if(parse_date(p + 1)) continue;

		file = 0;
		page = 0;

		// Isolate url
		++e; // skip space
		assert(*e == '"');
		for(url = ++e; *e && *e != '"'; ++e) ;
		*e++ = '\0';

		if((status = strtol(e, NULL, 10)) < 100 || status > 600) {
			printf("Bad status %d line %d\n", status, nline);
			continue;
		}

		if(status == 200) {
			file = 1;

			// Count pages
			// SAM This should parse better
			if(strncmp(url, "GET ", 4) == 0) {
				int n;

				url += 4;

				n = check_url(url);
				if (n == 0)
					printf("PROBS: %s\n", url);
				else
					url += n;

				if((p = strchr(url, ' '))) *p = '\0';
				if((p = strrchr(url, '.'))) {
					if(strncmp(p, ".htm", 4) == 0)
						page = 1;
				}
				else if(!strstr(url, "style"))
					page = 1;
			}
			else if(strncmp(url, "HEAD ", 5))
				printf("PROBLEM %s\n", url);
		}

		// Isolate agent
		if(!(p = strrchr(e, '\n'))) {
			printf("LINE TOO LONG\n");
			exit(1);
		}
		--p;
		if(*p != '"') { badline(line, p, nline); continue; }
		*p = '\0';
		for(--p; *p != '"'; --p)
			if(p == line) break;
		if(p == line) { badline(line, NULL, nline); continue; }
		++p;

		add_agent(p, file, page);
	}

}


void add_agent(char *agent, int file, int page)
{
	int i;

	for(i = 0; i < n_agents; ++i)
		if(strcmp(agents[i].name, agent) == 0) {
			++agents[i].hits;
			agents[i].files += file;
			agents[i].pages += page;
			return;
		}

	agents[n_agents].name  = strdup(agent);
	agents[n_agents].hits  = 1;
	agents[n_agents].files = file;
	agents[n_agents].pages = page;
	++n_agents;
	assert(n_agents < MAX_AGENTS);
}


void add_os(int group, char *name, NameCount *agent)
{
	int i;

	// Do Windows grouping here
	if(strncmp(name, "Windows ", 8) == 0) {
		if(strcmp(name, "Windows XP") &&
		   strcmp(name, "Windows Vista"))
			name = "Windows Other";
	}

	os_hits  += agent->hits;
	os_files += agent->files;
	os_pages += agent->pages;

	if(group >= 0) {
		groups[group].hits  += agent->hits;
		groups[group].files += agent->files;
		groups[group].pages += agent->pages;
	}

	for(i = 0; i < n_os; ++i)
		if(strcmp(os[i].name, name) == 0) {
			os[i].hits  += agent->hits;
			os[i].files += agent->files;
			os[i].pages += agent->pages;
			return;
		}

	os[n_os].name  = strdup(name);
	os[n_os].hits  = agent->hits;
	os[n_os].files = agent->files;
	os[n_os].pages = agent->pages;
	os[n_os].group = group;
	++n_os;
	assert(n_os < MAX_OS);
}

int isabot(char *line)
{
	int i;

	// printf("Checking '%s'\n", line);

	if (strcasestr(line, "bot") ||
	    strcasestr(line, "spider") ||
	    strcasestr(line, "crawler"))
		return 1;

	for(i = 0; i < nbots; ++i)
		if(strcasestr(line, bots[i]))
			return 1;
	return 0;
}


// Returns 0 if unknown, 1 otherwise
int parse_agent(NameCount *agent)
{
	char *line = agent->name;
	char *p;
	NameCount *browser = NULL;

	totalhits  += agent->hits;
	totalfiles += agent->files;
	totalpages += agent->pages;

	if (isabot(line))
		browser = &browsers[BOTS];
	// Must put Opera before MSIE & Netscape
	else if(strstr(line, "Opera")) {
#if 0
		browser = &browsers[OPERA];
#else
		browser = &browsers[OTHER_BROWSER];
#endif
	} else if((p = strstr(line, "MSIE"))) {
		for(p += 4; isspace(*p) || *p == '+'; ++p) ;
		switch(*p) {
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
			browser = &browsers[MSIE];
			break;
		default:
			browser = &browsers[MSIE];
			printf("UNKNOWN MSIE %s\n", p);
			break;
		}
	} else if((p = strstr(line, "Mozilla")) && !strstr(line, "ompatible")) {
		p += 7;
		if(*p++ == '/')
			switch(*p) {
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
				printf("UNKNOWN MOZ %s (%d)\n", line, agent->hits);
				break;
			}
		else {
			browser = &browsers[NETSCAPE];
			printf("UNKNOWN MOZILLA %s\n", line);
		}
	} else
		browser = &browsers[OTHER_BROWSER];

	if(browser) {
		browser->hits  += agent->hits;
		browser->files += agent->files;
		browser->pages += agent->pages;
	}

	/* Bots don't count in the OS count */
	if (browser == &browsers[BOTS])
		return 0;

	// Try to intuit the OS
	if((p = strstr(line, "Win"))) {
	again:
		if(strncmp(p, "Windows", 7) == 0)
			p += 7;
		else
			p += 3;
		if(*p == ' ' || *p == '_' || *p == '+') ++p;

		if(strncmp(p, "67", 2) == 0)
			// Win67 not necessarily windows
			add_os(OTHER, "Other", agent);
		else if(strncmp(p, "98", 2) == 0) {
			// I got this from analog
			if(strstr(p, "Win 9x 4.9"))
				add_os(WINDOZE, "Windows ME", agent);
			else
				add_os(WINDOZE, "Windows 98", agent);
		}
		else if(strncmp(p, "95", 2) == 0)
			add_os(WINDOZE, "Windows 95", agent); // 95
		else if(strncmp(p, "9x", 2) == 0) {
			if(strncmp(p, "9x 4.90", 7) == 0)
				add_os(WINDOZE, "Windows ME", agent);
			else
				add_os(WINDOZE, "Windows Other", agent); // other
		}
		else if(strncmp(p, "NT", 2) == 0) {
			p += 2;
			if(*p == ' ') ++p;
			if(*p == '6') // Technically 6.0
				add_os(WINDOZE, "Windows Vista", agent);
			else if(strncmp(p, "5.1", 3) == 0)
				add_os(WINDOZE, "Windows XP", agent);
			else if(*p == '5')
				add_os(WINDOZE, "Windows 2000", agent);
			else
				add_os(WINDOZE, "Windows NT", agent);
		}
		else if(strncmp(p, "2000", 4) == 0)
			add_os(WINDOZE, "Windows 2000", agent);
		else if(strncmp(p, "3.1", 3) == 0)
			// Not enough to be worth keeping seperate
			add_os(WINDOZE, "Windows Other", agent); // 3.1
		else if(strncmp(p, "ME", 2) == 0 ||
				strncmp(p, "Me", 2) == 0)
			add_os(WINDOZE, "Windows ME", agent);
		else if(strncmp(p, "CE", 2) == 0)
			// Not enough to be worth keeping seperate
			add_os(WINDOZE, "Windows Other", agent); // CE
		else if(strncmp(p, "XP", 2) == 0)
			add_os(WINDOZE, "Windows XP", agent);
		else if(strncmp(p, "-NT", 3) == 0)
			add_os(WINDOZE, "Windows NT", agent);
		else if (strncmp(p, "6.0", 3) == 0)
			add_os(WINDOZE, "Windows Vista", agent);
		else if((p = strstr(p, "Win")))
			goto again;
		else {
			// Not enough to be worth keeping seperate
			add_os(WINDOZE, "Windows Other", agent); // other
			printf("windoze unknown: %s\n", line);
			// SAM no return 0; // Put in unknown page
		}
	}
	else if((p = strstr(line, "Mac")) && strncmp(p, "Machine", 7)) {
		if(strstr(line, "OS X"))
			add_os(UNIX, "Macintosh", agent);
		else if(strstr(line, "Darwin"))
			add_os(UNIX, "Macintosh", agent);
		else if(strncmp(p, "Macintosh", 9) == 0 ||
		   strncmp(p, "Mac_PowerPC", 11) == 0 ||
		   strncmp(p, "Mac_PPC", 7) == 0)
			add_os(NONE, "Macintosh", agent);
		else {
			add_os(NONE, "Macintosh", agent);
			printf("Macintosh unknown: %s\n", line);
			return 0; // Put in unknown page
		}
	}

	// Unix
	else if((p = strstr(line, "Linux")))
		add_os(UNIX, "Linux", agent);
	else if((p = strstr(line, "linux")))
		add_os(UNIX, "Linux", agent);
	else if((p = strstr(line, "Solaris")))
		add_os(UNIX, "Solaris", agent);
	else if((p = strstr(line, "SunOS")))
		add_os(UNIX, "SunOS", agent);
	else if((p = strstr(line, "AIX")))
		add_os(UNIX, "AIX", agent);
	else if((p = strstr(line, "BSD")))
		add_os(UNIX, "BSD", agent);
	else if((p = strstr(line, "bsd")))
		add_os(UNIX, "BSD", agent);
	else if((p = strstr(line, "Unix")))
		add_os(UNIX, "Unix Other", agent);
	else if((p = strstr(line, "UNIX")))
		add_os(UNIX, "Unix Other", agent);
	else if((p = strstr(line, "X11")))
		add_os(UNIX, "X11", agent);
	else if(strncmp(line, "WebDownloader for X", 19) == 0)
		add_os(UNIX, "X11", agent);
	else if(strstr(line, "Red Hat"))
		add_os(UNIX, "Linux", agent);
	// Some Unix only browsers
	else if(strstr(line, "Konqueror"))
		add_os(UNIX, "Unix Other", agent);
	else if(strncmp(line, "Lynx", 4) == 0)
		add_os(UNIX, "Unix Other", agent);
	else if(strncmp(line, "Dillo", 4) == 0)
		add_os(UNIX, "Unix Other", agent);
	// Unix

	else if((p = strstr(line, "Java")))
		add_os(JAVA, "Java", agent);
        else if((p = strstr(line, "libwww-perl")))
		add_os(JAVA, "Perl", agent);
	else if((p = strstr(line, "WebTV")))
		add_os(OTHER, "WebTV", agent);
	else if((p = strstr(line, "RISC OS")))
		add_os(OTHER, "RISC OS", agent); // RISC OS
	else if((p = strstr(line, "SAGEM")))
		add_os(OTHER, "SAGEM", agent); // SAGEM
	else if((p = strstr(line, "OS/2")))
		add_os(OTHER, "OS/2", agent); // OS/2
	else if((p = strstr(line, "BEOS"))) // BeOS
		add_os(OTHER, "BeOS", agent);
	else if((p = strstr(line, "BeOS"))) // BeOS
		add_os(OTHER, "BeOS", agent);
	else if((p = strstr(line, "AmigaOS"))) // Amiga
		add_os(OTHER, "Amiga", agent);
	else if((p = strstr(line, "QNX"))) // QNX
		add_os(OTHER, "QNX", agent);
	else if((p = strstr(line, "I-Opener"))) // QNX - odds are it is running Linux
		add_os(OTHER, "QNX", agent);
	else if((p = strstr(line, "UdmSearch"))) {
		// ignore add_os(NONE, "Search Engine", agent);
	} else if((p = strstr(line, "PlayStation")))
		add_os(OTHER, "PlayStation", agent);
	else if((p = strstr(line, "Google Desktop")))
		add_os(OTHER, "Google", agent);
	else if(strncmp(line, "Amiga", 5) == 0)
		add_os(OTHER, "Amiga", agent);
	else if(strstr(line, "UP.Browser")) // Cell phone browser
		add_os(OTHER, "Other", agent);
	else if(strstr(line, "eCatch")) // windows only
		add_os(WINDOZE, "Windows Other", agent); // other
	else if(strstr(line, "Offline Explorer")) // windows only
		add_os(WINDOZE, "Windows Other", agent); // other
	else if(strstr(line, "MSIE"))
		add_os(WINDOZE, "Windows Other", agent); // other
	else if(strncmp(line, "DA 5.0", 6) == 0)
		add_os(WINDOZE, "Windows Other", agent); // other
	else if(strncmp(line, "Teleport Pro", 12) == 0) // windows only
		add_os(WINDOZE, "Windows Other", agent); // other
	else {
		add_os(NONE, "Unknown", agent);
		if(n_unknown >= MAX_UNKNOWN)
			printf("Too many unknowns!!!\n");
		else
			memcpy(&unknowns[n_unknown], agent, sizeof(NameCount));
		++n_unknown;
		return 0;
	}

	return 1;
}


int hits_compare(const void *a, const void *b)
{
	const NameCount *a1 = (NameCount*)a;
	const NameCount *b1 = (NameCount*)b;
	if(b1->hits == a1->hits)
		// break the tie with pages
		return b1->pages - a1->pages;
	else
		return b1->hits - a1->hits;
}

int files_compare(const void *a, const void *b)
{
	const NameCount *a1 = (NameCount*)a;
	const NameCount *b1 = (NameCount*)b;
	if(b1->files == a1->files)
		// break the tie with hits
		return b1->hits - a1->hits;
	else
		return b1->files - a1->files;
}

int pages_compare(const void *a, const void *b)
{
	const NameCount *a1 = (NameCount*)a;
	const NameCount *b1 = (NameCount*)b;
	if(b1->pages == a1->pages)
		// break the tie with hits
		return b1->hits - a1->hits;
	else
		return b1->pages - a1->pages;
}


void sort_oses(void)
{
	int i;

	switch(compare_type) {
	case COMPARE_HITS:
		qsort(groups, N_GROUPS, sizeof(NameCount), hits_compare);
		qsort(os, n_os, sizeof(NameCount), hits_compare);
		qsort(agents, n_agents, sizeof(NameCount), hits_compare);
		qsort(browsers, N_BROWSERS, sizeof(NameCount), hits_compare);
		qsort(bgroups, N_BGROUPS, sizeof(NameCount), hits_compare);
		break;
	case COMPARE_FILES:
		qsort(groups, N_GROUPS, sizeof(NameCount), files_compare);
		qsort(os, n_os, sizeof(NameCount), files_compare);
		qsort(agents, n_agents, sizeof(NameCount), files_compare);
		qsort(browsers, N_BROWSERS, sizeof(NameCount), files_compare);
		qsort(bgroups, N_BGROUPS, sizeof(NameCount), files_compare);
		break;
	case COMPARE_PAGES:
		qsort(groups, N_GROUPS, sizeof(NameCount), pages_compare);
		qsort(os, n_os, sizeof(NameCount), pages_compare);
		qsort(agents, n_agents, sizeof(NameCount), pages_compare);
		qsort(browsers, N_BROWSERS, sizeof(NameCount), pages_compare);
		qsort(bgroups, N_BGROUPS, sizeof(NameCount), pages_compare);
		break;
	}
	qsort(unknowns, n_unknown, sizeof(NameCount), hits_compare);

	for (i = 0; i < N_BROWSERS; ++i)
		if (strncmp(browsers[i].name, "Bot", 3) == 0)
			bot_index = i;
}


// Taken from webalizer
char *cur_time(time_t now)
{
	static char timestamp[32]; // SAM
	int local_time = 1; // SAM

   /* convert to timestamp string */
   if (local_time)
      strftime(timestamp,sizeof(timestamp),"%d-%b-%Y %H:%M %Z",
            localtime(&now));
   else
      strftime(timestamp,sizeof(timestamp),"%d-%b-%Y %H:%M GMT",
            gmtime(&now));

   return timestamp;
}


char *months[12] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};


int parse_date(char *date)
{
	int tz;
	char month[8];
	struct tm tm;
	time_t this;

	// SAM what about tz?
	memset(&tm, 0, sizeof(tm));
	if(sscanf(date, "%d/%[^/]/%d:%d:%d:%d %d",
			  &tm.tm_mday, month, &tm.tm_year,
			  &tm.tm_hour, &tm.tm_min, &tm.tm_sec, &tz) != 7) {
		printf("Bad date: %s\n", date);
		return 1;
	}

	tm.tm_year -= 1900;

	for(tm.tm_mon = 0; tm.tm_mon < 12; ++tm.tm_mon)
		if(strcmp(months[tm.tm_mon], month) == 0) {
			if((this = mktime(&tm)) == (time_t)-1) {
				perror("mktime");
				exit(1);
			}
			if(this > max_date) {
				max_date = this;
			}
			if(this < min_date) {
				min_date = this;
			}
			return 0; // success
		}

	printf("BAD MONTH %s\n", date);
	return 1;
}


void addbot(char *bot)
{
	static int max = 0;

	/* This case is handled by default */
	if (strcasestr(bot, "bot"))
		return;

	if(nbots >= max) {
		max += 10;
		if(!(bots = realloc(bots, max * sizeof(char*)))) {
			printf("Out of memory\n");
			exit(1);
		}
	}
	if(!(bots[nbots] = strdup(bot))) {
		printf("Out of memory\n");
		exit(1);
	}
	++nbots;
}


/*
 * Local Variables:
 * compile-command: "gcc -O3 -Wall agent.c -o agent"
 * End:
 */
