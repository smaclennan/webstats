#include "webstats.h"

static int lineno;
static int max;

time_t min_date = 0x7fffffff, max_date;


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

void parse_logfile(char *logfile, void (*func)(struct log *log))
{
	struct log log;
	char line[4096], url[4096], refer[4096], who[4096];
	int len;
	gzFile fp = my_fopen(logfile);
	if (!fp) {
		perror(logfile);
		exit(1);
	}

	memset(&log, 0, sizeof(log));

	while (my_gets(line, sizeof(line), fp)) {
		char ip[20], host[20], month[8], sstr[20], method[20];
		char *s, *e;
		int n, where;
		int status;
		unsigned long size;
		struct tm tm;

		++lineno;
		len = strlen(line);
		if (len > max) {
			max = len;
			if (len == sizeof(line) - 1) {
				printf("PROBLEMS 0\n");
				gzgets(fp, line, sizeof(line));
				continue;
			}
		}

		/* Don't count local access. */
		if (strncmp(line, "192.168.", 8) == 0)
			continue;

		memset(&tm, 0, sizeof(tm));
		n = sscanf(line,
			   "%s %s - [%d/%[^/]/%d:%d:%d:%d %*d] "
			   "\"%s %s HTTP/1.%*d\" %d %s \"%n",
			   ip, host,
			   &tm.tm_mday, month, &tm.tm_year,
			   &tm.tm_hour, &tm.tm_min, &tm.tm_sec,
			   method, url, &status, sstr, &where);

		if (n == 10) {
			/* sscanf \"%[^\"]\" cannot handle an empty string. */
			*url = '\0';
			if (sscanf(line,
				   "%s %s - [%d/%[^/]/%d:%d:%d:%d %*d] "
				   "\"\" %d %s \"%n",
				   ip, host,
				   &tm.tm_mday, month, &tm.tm_year,
				   &tm.tm_hour, &tm.tm_min, &tm.tm_sec,
				   &status, sstr, &where) != 10) {
				printf("%d: Error [8] %s", lineno, line);
				continue;
			}
		} else if (n != 12) {
			printf("%d: Error [%d] %s", lineno, n, line);
			continue;
		}

		/* This handles a '-' in the size field */
		size = strtol(sstr, NULL, 10);

		/* People seem to like to embed quotes in the refer
		 * and who strings :( */
		s = line + where;
		e = strchr(s, '"');
		while (e && *(e + 1) != ' ')
			e = strchr(e + 1, '"');
		if (!e) {
			printf("%d: Error %s", lineno, line);
			continue;
		}

		*e = '\0';
		snprintf(refer, sizeof(refer), "%s", s);

		/* Warning the who will contains the quotes. */
		snprintf(who, sizeof(who), "%s", e + 2);

		log.time = parse_date(&tm, month);

		log.lineno = lineno;
		log.ip = ip;
		log.host = host;
		log.tm = &tm;
		log.method = method;
		log.url = url;
		log.status = status;
		log.size = size;
		log.refer = refer;
		log.who = who;

		if (func)
			(*func)(&log);
	}

	my_fclose(fp);
}

static char *months[12] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

time_t parse_date(struct tm *tm, char *month)
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
			return this; /* success */
		}

	printf("BAD MONTH %s\n", month);
	exit(1);
}

