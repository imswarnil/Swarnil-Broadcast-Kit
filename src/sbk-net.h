#pragma once

#include <obs-module.h>
#include <pthread.h>
#include <stdbool.h>

/*  A polled HTTPS GET on a worker thread.

    Every live number in the kit — a subscriber count, a member total, whatever
    a creator's own endpoint returns — comes through here. The rules that matter:

      · the graphics thread never waits. It reads the last good value under a
        mutex and draws it; the network is somewhere else entirely.
      · a failed request keeps the last good value on screen and backs off, so
        a flaky connection mid-stream shows a slightly stale number rather than
        a dash or, worse, a zero.
      · nothing is requested faster than the interval, and the interval has a
        floor. An overlay that hammers someone's API quota is a broken overlay.

    The worker calls `parse` with the response body. Whatever it stores there is
    read back under `lock`.  */

struct sbk_net;

/* Runs on the worker thread with the response body. Return false to count the
   poll as failed (a 200 that is not the JSON you expected). */
typedef bool (*sbk_net_parse_t)(void *param, const char *body, size_t len);

/* Runs on the worker thread just before each request, so a provider that has
   to sign its request — Ghost's admin JWT expires every few minutes — can
   rebuild the URL and headers. Return false to skip this round. */
typedef bool (*sbk_net_prepare_t)(void *param, struct sbk_net *n);

struct sbk_net {
	pthread_t thread;
	pthread_mutex_t lock;
	pthread_cond_t wake;
	bool running, stop, dirty;

	/* set under lock by the owner, or by prepare on the worker */
	char *url;
	char *auth;      /* the whole Authorization header value, or NULL */
	double interval; /* seconds */

	sbk_net_prepare_t prepare;
	sbk_net_parse_t parse;
	void *param;

	/* what the owner reads to say how it is going */
	bool ok;         /* the last poll succeeded */
	bool ever_ok;    /* any poll has ever succeeded */
	long http;
	char err[160];
	uint64_t last_ns;
};

void sbk_net_init(struct sbk_net *n, void *param, sbk_net_prepare_t prepare, sbk_net_parse_t parse);
void sbk_net_free(struct sbk_net *n);
/* Set where to go. Safe to call from the UI thread on every settings change;
   an unchanged url and interval is a no-op, so it will not restart the poll. */
void sbk_net_set(struct sbk_net *n, const char *url, const char *auth, double interval);
/* Poll now rather than at the next interval. */
void sbk_net_refresh(struct sbk_net *n);

/* Helpers the providers share. */
char *sbk_json_string(const char *body, size_t len, const char *path); /* bfree() it */
bool sbk_json_number(const char *body, size_t len, const char *path, double *out);
/* "a.b.c" walks objects; "a.0.b" walks an array by index. */
char *sbk_url_escape(const char *s); /* bfree() it */
