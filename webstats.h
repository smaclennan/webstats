#ifndef _WEBSTATS_H_
#define _WEBSTATS_H_

#include <db.h>

extern time_t min_date, max_date;

struct log {
	int lineno;
	char *ip;
	char *host;
	struct tm *tm;
	char *method;
	char *url;
	int status;
	unsigned long size;
	char *refer;
	char *who;
};

void parse_logfile(char *logfile, void (*func)(struct log *log));
int parse_date(struct tm *tm, char *month);


/* Helpful db functions. */
void print(char *key, char *data);
void print_count(char *key, char *data);
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
