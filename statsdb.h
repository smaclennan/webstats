#ifndef _STATSDB_H_
#define _STATSDB_H_

#include <db.h>

void print(char *key, char *data);
void print_count(char *key, char *data);
DB *db_open(char *fname);
int db_put(DB *db, char *str);
int db_walk(DB *db, void (*func)(char *key, char *data));
int db_put_count(DB *db, char *str);
void db_close(char *fname, DB *db);

#endif
