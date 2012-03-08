#include "webstats.h"


/* Most I have seen is 4282 */
#define MAXLINE 4096

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
	char *s;

	if (fp == stdin)
		s = fgets(line, size, fp);
	else
		s = gzgets(fp, line, size);

	if (s) {
		if (strchr(s, '\n') == NULL) {
			puts("Line too long");
			my_gets(line, size, fp);
			return NULL;
		}
	}

	return s;
}

static inline void my_fclose(FILE *fp)
{
	if (fp != stdin)
		gzclose(fp);
}

void parse_logfile(char *logfile, void (*func)(struct log *log))
{
	struct log log;
	char line[MAXLINE], url[MAXLINE], refer[MAXLINE], who[MAXLINE];
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

		++log.lineno;

		/* Don't count local access. */
		if (strncmp(line, "192.168.", 8) == 0)
			continue;

		/* This first chunk cannot fail */
		memset(&tm, 0, sizeof(tm));
		n = sscanf(line,
			   "%s %s - [%d/%[^/]/%d:%d:%d:%d %*d] %n",
			   ip, host,
			   &tm.tm_mday, month, &tm.tm_year,
			   &tm.tm_hour, &tm.tm_min, &tm.tm_sec, &where);
		if (n != 8) {
			printf("%d: Error Internal %s", log.lineno, line);
			continue;
		} else
			s = line + where;

		/* The URL line can be ugly - check normal case first */
		n = sscanf(s, "\"%s %s HTTP/1.%*d\" %d %s \"%n",
			   method, url, &status, sstr, &where);
		if (n != 4) {
			*url = '\0';
			/* Check for empty URL */
			n = sscanf(s, "\"%s HTTP/1.%*d\" %d %s \"%n",
				   method, &status, sstr, &where);
			if (n != 3) {
				/* Check for empty string */
				n = sscanf(s, "\"\" %d %s \"%n",
					   &status, sstr, &where);
				if (n != 2) {
					printf("%d: Error %s", log.lineno, line);
					continue;
				}
			}
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
			printf("%d: Error %s", log.lineno, line);
			continue;
		}

		*e = '\0';
		snprintf(refer, sizeof(refer), "%s", s);

		/* Warning the who will contains the quotes. */
		snprintf(who, sizeof(who), "%s", e + 2);

		log.time = parse_date(&tm, month);

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

/* Gopher logfile is more limited. Plus, there is no concept of
 * virtual sites. */
void parse_gopher_log(char *logfile, void (*func)(struct log *log))
{
	char line[MAXLINE], url[MAXLINE];
	struct log log;
	gzFile fp = gzopen(logfile, "rb");
	if (!fp) {
		perror(logfile);
		exit(1);
	}

	memset(&log, 0, sizeof(log));

	while (gzgets(fp, line, sizeof(line))) {
		char ip[20], month[8];
		int status;
		unsigned long size;
		struct tm tm;

		++log.lineno;

		memset(&tm, 0, sizeof(tm));
		if (sscanf(line,
			   "%s - - [%d/%[^/]/%d:%d:%d:%d %*d] "
			   "\"%[^\"]\" %d %lu",
			   ip,
			   &tm.tm_mday, month, &tm.tm_year,
			   &tm.tm_hour, &tm.tm_min, &tm.tm_sec,
			   url, &status, &size) != 10) {
			printf("%d: Error %s", log.lineno, line);
			continue;
		}

		log.time = parse_date(&tm, month);

		log.ip = ip;
		log.tm = &tm;
		log.url = url;
		log.status = status;
		log.size = size;

		if (func)
			(*func)(&log);
	}

	gzclose(fp);
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
