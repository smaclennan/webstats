#include "webstats.h"

int verbose;
static char *host;

static void process_log(struct log *log)
{
	if (ignore_ip(log->ip))
		return;

	if (host && strstr(log->host, host) == NULL)
		return;

	if (isvisit(log, NULL))
		puts(log->line);
}

int main(int argc, char *argv[])
{
	int i;

	while ((i = getopt(argc, argv, "h:i:v")) != EOF)
		switch (i) {
		case 'h':
			host = optarg;
			break;
		case 'i':
			add_ignore(optarg);
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

	return 0;
}
