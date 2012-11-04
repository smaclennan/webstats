#include "webstats.h"

void print(char *key, void *data, int len)
{
	puts(key);
}

void print_count(char *key, void *data, int len)
{
	printf("%s %lu\n", key, *(unsigned long *)data);
}

DB *db_open(char *fname)
{
	char dbname[128];
	DB *db;

	if (*fname == '/')
		snprintf(dbname, sizeof(dbname), "%s", fname);
	else
		snprintf(dbname, sizeof(dbname), "/dev/shm/%s", fname);

	if (db_create(&db, NULL, 0)) {
		printf("db_create failed\n");
		return NULL;
	}

	if (db->open(db, NULL, dbname, NULL,
		     DB_BTREE, DB_CREATE | DB_TRUNCATE, 0664)) {
		printf("db_open failed\n");
		return NULL;
	}

	return db;
}

int db_get_data(DB *db, char *key, void *data, int len)
{
	DBT db_key, db_data;
	int rc;

	memset(&db_key, 0, sizeof(db_key));
	memset(&db_data, 0, sizeof(db_data));

	db_key.data = key;
	db_key.size = strlen(key) + 1;

	rc = db->get(db, NULL, &db_key, &db_data, 0);
	if (rc)
		return -1;

	if (data) {
		if (db_data.size > len) {
			printf("Warning: Data field too big: %d > %d\n",
			       db_data.size, len);
			db_data.size = len;
		}

		memcpy(data, db_data.data, db_data.size);
	}

	return db_data.size;
}

int db_put_data(DB *db, char *key, void *data, int len, int flags)
{
	DBT db_key, db_data;
	int rc;

	memset(&db_key, 0, sizeof(db_key));
	memset(&db_data, 0, sizeof(db_data));

	db_key.data = key;
	db_key.size = strlen(key) + 1;

	db_data.data = data;
	db_data.size = len;

	rc = db->put(db, NULL, &db_key, &db_data, flags);
	if (rc) {
		if (rc == -1)
			perror("put");
		else if (rc != DB_KEYEXIST)
			printf("HUH? %d\n", rc);
	}

	return rc;
}

int db_update_count(DB *db, char *str, unsigned long i)
{
	unsigned long count;
	int len = db_get_data(db, str, &count, sizeof(count));

	if (len > 0)
		i += count;

	return db_put_data(db, str, &i, sizeof(i), 0);
}

int db_walk(DB *db, void (*func)(char *key, void *data, int len))
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
		func((char *)key.data, data.data, data.size);

	if (rc != DB_NOTFOUND)
		printf("c_get returned %d\n", rc);
	else
		rc = 0;

	dbc->c_close(dbc);

	return rc;
}

void db_close(DB *db, char *fname)
{
	db->close(db, 0);

	if (fname) {
		char dbname[128];

		snprintf(dbname, sizeof(dbname), "/dev/shm/%s", fname);
		unlink(dbname);
	}
}
