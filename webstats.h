#ifndef _WEBSTATS_H_
#define _WEBSTATS_H_

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>
#include <zlib.h>
#include <db.h>

#include <samlib.h>

extern int verbose;
extern time_t min_date, max_date;

struct log {
	int lineno;
	const char *line;
	char *ip;
	char *host;
	struct tm *tm;
	time_t time;
	char *method;
	char *url;
	int status;
	unsigned long size;
	char *refer;
	char *who;
};

int parse_logfile(char *logfile, void (*func)(struct log *log));
void parse_gopher_log(char *logfile, void (*func)(struct log *log));
void dump_log(struct log *log);


/* time functions */
char *cur_time(time_t now);
char *cur_date(time_t now);
int days(void);
struct tm *calc_yesterday(void);
struct tm *calc_yesterdays(int back);
int time_equal(struct tm *a, struct tm *b);

void init_range(int days);
int in_range(struct log *log);
void range_fixup(void);

int ignore_ip(char *ip);
void add_ip_ignore(char *ip);

/* helpful is* functions */
int isbot(char *who);
int isbrowser(char *who);
int ispage(struct log *log);
int isdefault(struct log *log);
int isvisit(struct log *log, DB *ipdb, int clickthru);
int get_default_host(char *host, int len);

/* Helpful db functions. */
DB *stats_db_open(char *fname);
void stats_db_close(DB *db, char *fname);
int db_update_count(DB *db, char *str, unsigned long i);

#endif
