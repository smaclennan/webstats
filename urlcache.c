#include "webstats.h"

/* For now a simple linked list. Later maybe a hash? */
static struct urlcache {
	const char *url;
	struct urlcache *next;
} *urls, *utail;

static const char *urlcache_add(const char *url)
{
	struct urlcache *u = calloc(1, sizeof(struct urlcache));

	if (!u || !(u->url = strdup(url))) {
		puts("urlcache: Out of memory.");
		exit(1);
	}

	if (urls)
		utail->next = u;
	else
		urls = u;
	utail = u;

	return u->url;
}

const char *urlcache_get(const char *url)
{
	struct urlcache *u;

	if (!urls) {
		urlcache_add("/");
		urlcache_add("/favicon.ico");
	}

	for (u = urls; u; u = u->next)
		if (strcmp(u->url, url) == 0)
			return u->url;

	return urlcache_add(url);
}
