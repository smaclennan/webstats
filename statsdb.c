#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "webstats.h"

void print(char *key, char *data)
{
	puts(key);
}

void print_count(char *key, char *data)
{
	printf("%s %lu\n", key, *(unsigned long *)data);
}

DB *db_open(char *fname)
{
	char dbname[128];
	DB *db;

	snprintf(dbname, sizeof(dbname), "/dev/shm/%s", fname);

	if (db_create(&db, NULL, 0)) {
		printf("db_create failed\n");
		return NULL;
	}

	if (db->open(db, NULL, dbname, NULL,
		     DB_HASH, DB_CREATE | DB_TRUNCATE, 0664)) {
		printf("db_open failed\n");
		return NULL;
	}

	return db;
}

int db_put(DB *db, char *str)
{
	DBT key, data;
	int rc;

	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));

	key.data = str;
	key.size = strlen(str) + 1;

	rc = db->put(db, NULL, &key, &data, DB_NOOVERWRITE);
	if (rc) {
		if (rc == -1)
			perror("put");
		else if (rc != DB_KEYEXIST)
			printf("HUH? %d\n", rc);
	}

	return rc;
}

int db_walk(DB *db, void (*func)(char *key, char *data))
{
	DBT key, data;
	int rc;
	DBC *dbc;

	rc = db->cursor(db, NULL, &dbc, 0);
	if (rc) {
		printf("Cursor create returned %d\n", rc);
		return rc;
	}

	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));

	while ((rc = dbc->c_get(dbc, &key, &data, DB_NEXT)) == 0)
		func((char *)key.data, (char *)data.data);

	if (rc != DB_NOTFOUND)
		printf("c_get returned %d\n", rc);
	else
		rc = 0;

	dbc->c_close(dbc);

	return rc;
}

int db_put_count(DB *db, char *str)
{
	DBT key, data;
	unsigned long count;
	int rc;

	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));

	key.data = str;
	key.size = strlen(str) + 1;

	rc = db->get(db, NULL, &key, &data, 0);
	if (rc == 0) {
		if (data.size != sizeof(count)) {
			printf("PROBLEMS WITH SIZE: %d expected %d\n",
			       data.size, sizeof(count));
			exit(1);
		}
		count = *(unsigned long *)data.data + 1;
	} else
		count = 1;

	data.data = &count;
	data.size = sizeof(count);

	rc = db->put(db, NULL, &key, &data, 0);
	if (rc) {
		if (rc == -1)
			perror("put");
		else
			printf("HUH? %d\n", rc);
	}

	return rc;
}

void db_close(char *fname, DB *db)
{
	char dbname[128];

	db->close(db, 0);

	snprintf(dbname, sizeof(dbname), "/dev/shm/%s", fname);
	unlink(dbname);
}

