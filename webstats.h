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

extern int verbose;
extern time_t min_date, max_date;

struct log {
	int lineno;
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
time_t parse_date(struct tm *tm, char *month);


/* time functions */
char *cur_time(time_t now);
char *cur_date(time_t now);
int days(void);
struct tm *calc_yesterday(void);
int time_equal(struct tm *a, struct tm *b);

void init_range(int days);
int in_range(struct log *log);
void range_fixup(void);

int ignore_ip(char *ip);
void add_ignore(char *ip);

/* helpful is* functions */
int isbot(char *who);
int isbrowser(char *who);
int ispage(struct log *log);
int isvisit(struct log *log, DB *ipdb);
int get_default_host(char *host, int len);

/* Helpful db functions. */
void print(char *key, void *data, int len);
void print_count(char *key, void *data, int len);
DB *db_open(char *fname);
int db_get_data(DB *db, char *key, void *data, int len);
int db_put_data(DB *db, char *key, void *data, int len, int flags);
int db_update_count(DB *db, char *str, unsigned long i);
int db_walk(DB *db, void (*func)(char *key, void *data, int len));
void db_close(DB *db, char *fname);

static inline int db_put(DB *db, char *str)
{
	return db_put_data(db, str, NULL, 0, DB_NOOVERWRITE);
}

#endif
