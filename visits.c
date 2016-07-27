#include "webstats.h"

int verbose;
static int clickthru;
static char *host;
static struct tm *yesterday;
static DB *ipdb;

static void process_log(struct log *log)
{
	if (ignore_ip(log->ip))
		return;

	if (host && strstr(log->host, host) == NULL)
		return;

	if (yesterday && !time_equal(yesterday, log->tm))
		return;

	if (isvisit(log, ipdb, clickthru) == 1)
		fputs(log->line, stdout);
}

int main(int argc, char *argv[])
{
	int i;

	while ((i = getopt(argc, argv, "h:ci:uvy")) != EOF)
		switch (i) {
		case 'h':
			host = optarg;
			break;
		case 'c':
			clickthru = 1;
			break;
		case 'i':
			add_ip_ignore(optarg);
			break;
		case 'u':
			ipdb = stats_db_open("ipdb");
			if (!ipdb) {
				printf("Unable to open ip db\n");
				exit(1);
			}
			break;
		case 'y':
			yesterday = calc_yesterday();
			break;
		case 'v':
			++verbose;
			break;
		default:
			puts("Sorry!");
			exit(1);
		}

	if (optind == argc)
		parse_logfile(NULL, process_log);
	else
		for (i = optind; i < argc; ++i) {
			if (verbose)
				printf("Parsing %s...\n", argv[i]);
			parse_logfile(argv[i], process_log);
		}

	if (ipdb)
		stats_db_close(ipdb, "ipdb");

	return 0;
}
