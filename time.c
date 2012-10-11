#include "webstats.h"

static time_t start, end;
static time_t real_min = 0x7fffffff, real_max;

char *cur_time(time_t now)
{
	static char timestamp[32];

	/* convert to timestamp string */
	strftime(timestamp, sizeof(timestamp),
		 "%b %d %Y %H:%M %Z", localtime(&now));

	return timestamp;
}

char *cur_date(time_t now)
{
	static char timestamp[32];

	/* convert to timestamp string */
	strftime(timestamp, sizeof(timestamp), "%b %d %Y", localtime(&now));

	return timestamp;
}

int days(void)
{
	return ((max_date - min_date) / 60 / 60 + 23) / 24;
}

void init_range(int days)
{
	time_t now = time(NULL);
	struct tm *tm = localtime(&now);

	tm->tm_hour = 23;
	tm->tm_min = 59;
	tm->tm_sec = 59;
	--tm->tm_mday;
	end = mktime(tm);

	tm->tm_hour = 0;
	tm->tm_min = 0;
	tm->tm_sec = 0;
	tm->tm_mday -= days - 1;
	start = mktime(tm);
}

int in_range(struct log *log)
{
	if (start == 0)
		return 1;
	else if (log->time >= start && log->time <= end) {
		if (log->time > real_max)
			real_max = log->time;
		if (log->time < real_min)
			real_min = log->time;
		return 1;
	} else
		return 0;
}

void range_fixup(void)
{
	if (start) {
		/* Correct these */
		min_date = real_min;
		max_date = real_max;
	}
}

struct tm *calc_yesterday(void)
{
	static struct tm ytm;
	time_t yesterday, now = time(NULL);
	struct tm *tm = localtime(&now);

	tm->tm_mday--;
	yesterday = mktime(tm);
	tm = localtime(&yesterday);

	memcpy(&ytm, tm, sizeof(ytm));
	return &ytm;
}
