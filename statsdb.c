#include "webstats.h"

DB *stats_db_open(char *fname)
{
	char dbname[128];
	void *db;

	if (strchr(fname, '/'))
		snprintf(dbname, sizeof(dbname), "%s", fname);
	else
		snprintf(dbname, sizeof(dbname), "/dev/shm/%s", fname);

	if (db_open(dbname, DB_CREATE | DB_TRUNCATE, &db))
		return NULL;

	return (DB *)db;
}

void stats_db_close(DB *db, char *fname)
{
	db_close((void *)db);

	if (fname) {
		char dbname[128];

		snprintf(dbname, sizeof(dbname), "/dev/shm/%s", fname);
		unlink(dbname);
	}
}
