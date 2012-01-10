#ifndef _WEBSTATS_H_
#define _WEBSTATS_H_

#include <db.h>

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

void parse_logfile(char *logfile, void (*func)(struct log *log));
time_t parse_date(struct tm *tm, char *month);


/* time functions */
char *cur_time(time_t now);
char *cur_date(time_t now);
int days(void);

void init_range(int days);
int in_range(struct log *log);
void range_fixup(void);


/* Helpful db functions. */
void print(char *key, void *data, int len);
void print_count(char *key, void *data, int len);
DB *db_open(char *fname);
int db_get_data(DB *db, char *key, void *data, int len);
int db_put_data(DB *db, char *key, void *data, int len, int flags);
int db_update_count(DB *db, char *str, unsigned long i);
int db_walk(DB *db, void (*func)(char *key, void *data, int len));
void db_close(char *fname, DB *db);

static inline int db_put(DB *db, char *str)
{
	return db_put_data(db, str, NULL, 0, DB_NOOVERWRITE);
}

#endif
