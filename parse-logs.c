#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#include <zlib.h>

#include <gd.h>
#include <gdfontmb.h>
#include <gdfonts.h>

#define ENABLE_VISITS
#ifdef ENABLE_VISITS
#include <db.h>
#include <arpa/inet.h>

static DB *db_open(char *fname);
static int db_put(DB *db, char *ip);
static void db_close(char *fname, DB *db);

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

/* SAM DBG */
static unsigned ftp_pages;
static unsigned talkbass;
static unsigned talkbass_size;
static unsigned talkbass_misses;

/* SAM */

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
#define N_SITES (sizeof(sites) / sizeof(struct site))

struct list {
	char *name;
	struct list *next;
};

static int verbose;

static struct list *others;

static time_t min_date = 0x7fffffff, max_date;

static char *outdir;
static char *outfile = "stats.html";
static char *outgraph = "pie.gif";


static int parse_date(struct tm *tm, char *month);


static void add_others(char *name)
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
	l->next = others;
	others = l;
}

static int isbrowser(char *who)
{
	if (strcasestr(who, "bot") ||
	    strcasestr(who, "spider") ||
	    strcasestr(who, "crawl") ||
	    strcasestr(who, "link")) {
		if (verbose > 1)
			puts(who);
		return 0;
	}

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
		if (i == 2) { /* ftp.seanm.ca */
			++ftp_pages;
			if (strstr(refer, "talkbass")) {
				++talkbass;
				talkbass_size += size;
			}
		}
		if (db_put(sites[i].ipdb, ip) == 0) {
			++sites[i].visits;
			if (verbose)
				printf("%s: %s\n", sites[i].name, ip);
		}
	} else if (i == 2 && strstr(refer, "talkbass"))
		++talkbass_misses;
}

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
	struct list *l;
	int len, i, http;
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
			printf("%d: Error %s", lineno, line);
			continue;
		}
		if (*how != '"') {
			printf("%d: Error %s", lineno, line);
			continue;
		}
		memmove(how, how + 1, 10);

		parse_date(&tm, month);

		puts(url);


		/* Unqualified lines */
		if (*host == '-') {
#if 0
			puts(line);
#endif
			update_site(0, size, status, ip, url, who, refer);
			continue;
		}

		for (i = 1; i < N_SITES; ++i)
			if (strstr(host, sites[i].name)) {
				update_site(i, size, status, ip, url, who, refer);
				break;
			}

		if (i < N_SITES)
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

	my_fclose(fp);
}

int main(int argc, char *argv[])
{
	int i;

	while ((i = getopt(argc, argv, "d:g:o:v")) != EOF)
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
		default:
			puts("Sorry!");
			exit(1);
		}

	for (i = 0; i < N_SITES; ++i) {
		sites[i].ipdb = db_open(sites[i].name);
		if (!sites[i].ipdb) {
			printf("Unable to open db\n");
			exit(1);
		}
	}

	/* preload some know others */
	add_others("seanm.dyndns.org");
	add_others("216.138.233.67");
	add_others("toronto-hs-216-138-233-67.s-ip.magma.ca");
	add_others("m38a1.ca");

	if (optind == argc)
		parse_logfile(NULL);
	else
		for (i = optind; i < argc; ++i) {
			if (verbose)
				printf("Parsing %s...\n", argv[i]);
			parse_logfile(argv[i]);
		}

	for (i = 0; i < N_SITES; ++i)
		db_close(sites[i].name, sites[i].ipdb);

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

#ifdef ENABLE_VISITS
static DB *db_open(char *fname)
{
	char dbname[128];
	DB *db;

	snprintf(dbname, sizeof(dbname), "/dev/shm/%s", fname);

	if (db_create(&db, NULL, 0)) {
		printf("db_create failed\n");
		return NULL;
	}

	if (db->open(db, NULL, dbname, NULL, DB_HASH, DB_CREATE | DB_TRUNCATE, 0664)) {
		printf("db_open failed\n");
		return NULL;
	}

	return db;
}

static int db_put(DB *db, char *ip)
{
	DBT key, data;
	int rc;
	struct in_addr addr;

	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));

	inet_aton(ip, &addr);
	key.data = &addr;
	key.size = sizeof(addr);

	rc = db->put(db, NULL, &key, &data, DB_NOOVERWRITE);
	if (rc) {
		if (rc == -1)
			perror("put");
		else if (rc != DB_KEYEXIST)
			printf("HUH? %d\n", rc);
	}

	return rc;
}

static void db_close(char *fname, DB *db)
{
	char dbname[128];

	db->close(db, 0);

	snprintf(dbname, sizeof(dbname), "/dev/shm/%s", fname);
	unlink(dbname);
}
#endif


/*
 * Local Variables:
 * compile-command: "gcc -O3 -Wall parse-logs.c -o parse-logs -ldb -lz"
 * End:
 */
