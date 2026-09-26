#include <curl/curl.h>
#include <jansson.h>
#include <string.h>
#include <time.h>

#include <util/platform.h>
#include <util/threading.h>

#include "sbk-common.h"
#include "sbk-net.h"

#define MIN_INTERVAL 15.0    /* nobody needs a subscriber count faster than this */
#define MAX_BODY (1 << 20)   /* 1 MB: a stats endpoint that answers with more is not one */

struct buf {
	char *data;
	size_t len;
};

static size_t on_write(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	struct buf *b = userdata;
	size_t add = size * nmemb;
	if (b->len + add > MAX_BODY)
		return 0;
	char *grown = brealloc(b->data, b->len + add + 1);
	if (!grown)
		return 0;
	b->data = grown;
	memcpy(b->data + b->len, ptr, add);
	b->len += add;
	b->data[b->len] = 0;
	return add;
}

static void do_request(struct sbk_net *n)
{
	pthread_mutex_lock(&n->lock);
	if (n->prepare && !n->prepare(n->param, n)) {
		pthread_mutex_unlock(&n->lock);
		return;
	}
	char *url = bstrdup(n->url ? n->url : "");
	char *auth = n->auth ? bstrdup(n->auth) : NULL;
	pthread_mutex_unlock(&n->lock);

	if (!*url) {
		bfree(url);
		bfree(auth);
		return;
	}

	CURL *c = curl_easy_init();
	if (!c) {
		bfree(url);
		bfree(auth);
		return;
	}
	struct buf b = {0};
	struct curl_slist *headers = NULL;
	headers = curl_slist_append(headers, "Accept: application/json");
	headers = curl_slist_append(headers, "User-Agent: Swarnil-Broadcast-Kit/1.0 (OBS plugin)");
	if (auth) {
		struct dstr h = {0};
		dstr_printf(&h, "Authorization: %s", auth);
		headers = curl_slist_append(headers, h.array);
		dstr_free(&h);
	}
	curl_easy_setopt(c, CURLOPT_URL, url);
	curl_easy_setopt(c, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, on_write);
	curl_easy_setopt(c, CURLOPT_WRITEDATA, &b);
	curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(c, CURLOPT_MAXREDIRS, 3L);
	/* a request that hangs must not keep the thread from shutting down when
	   OBS quits, so both timeouts are short */
	curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 6L);
	curl_easy_setopt(c, CURLOPT_TIMEOUT, 12L);
	curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);

	CURLcode res = curl_easy_perform(c);
	long http = 0;
	curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &http);

	bool ok = false;
	char err[160] = {0};
	if (res != CURLE_OK) {
		snprintf(err, sizeof(err), "%s", curl_easy_strerror(res));
	} else if (http < 200 || http >= 300) {
		snprintf(err, sizeof(err), "HTTP %ld", http);
	} else if (!b.data) {
		snprintf(err, sizeof(err), "empty response");
	} else {
		ok = n->parse ? n->parse(n->param, b.data, b.len) : true;
		if (!ok)
			snprintf(err, sizeof(err), "unexpected response shape");
	}

	pthread_mutex_lock(&n->lock);
	n->ok = ok;
	n->http = http;
	n->last_ns = os_gettime_ns();
	if (ok)
		n->ever_ok = true;
	snprintf(n->err, sizeof(n->err), "%s", err);
	pthread_mutex_unlock(&n->lock);

	if (!ok)
		SBK_LOG(LOG_INFO, "poll failed (%s) — keeping the last good value", err[0] ? err : "?");

	curl_slist_free_all(headers);
	curl_easy_cleanup(c);
	bfree(b.data);
	bfree(url);
	bfree(auth);
}

static void *worker(void *arg)
{
	struct sbk_net *n = arg;
	os_set_thread_name("sbk-net");
	/* a small stagger, so six counters added at once do not all fire on the
	   same tick when OBS loads a collection */
	os_sleep_ms(200 + (int)((uintptr_t)n % 800));

	while (true) {
		pthread_mutex_lock(&n->lock);
		if (n->stop) {
			pthread_mutex_unlock(&n->lock);
			break;
		}
		bool go = n->dirty || !n->ever_ok || n->last_ns == 0 ||
			  (double)(os_gettime_ns() - n->last_ns) / 1e9 >= n->interval;
		n->dirty = false;
		pthread_mutex_unlock(&n->lock);

		if (go)
			do_request(n);

		pthread_mutex_lock(&n->lock);
		if (!n->stop) {
			/* wake on a settings change, otherwise look again in a
			   second — the interval itself is checked above */
			struct timespec ts;
			clock_gettime(CLOCK_REALTIME, &ts);
			ts.tv_sec += 1;
			pthread_cond_timedwait(&n->wake, &n->lock, &ts);
		}
		bool stop = n->stop;
		pthread_mutex_unlock(&n->lock);
		if (stop)
			break;
	}
	return NULL;
}

void sbk_net_init(struct sbk_net *n, void *param, sbk_net_prepare_t prepare, sbk_net_parse_t parse)
{
	pthread_mutex_init(&n->lock, NULL);
	pthread_cond_init(&n->wake, NULL);
	n->param = param;
	n->prepare = prepare;
	n->parse = parse;
	n->interval = 60.0;
	n->running = pthread_create(&n->thread, NULL, worker, n) == 0;
	if (!n->running)
		SBK_LOG(LOG_WARNING, "could not start the poll thread");
}

void sbk_net_free(struct sbk_net *n)
{
	if (n->running) {
		pthread_mutex_lock(&n->lock);
		n->stop = true;
		pthread_cond_broadcast(&n->wake);
		pthread_mutex_unlock(&n->lock);
		pthread_join(n->thread, NULL);
		n->running = false;
	}
	pthread_cond_destroy(&n->wake);
	pthread_mutex_destroy(&n->lock);
	bfree(n->url);
	bfree(n->auth);
	n->url = NULL;
	n->auth = NULL;
}

void sbk_net_set(struct sbk_net *n, const char *url, const char *auth, double interval)
{
	if (interval < MIN_INTERVAL)
		interval = MIN_INTERVAL;
	pthread_mutex_lock(&n->lock);
	bool changed = (!n->url && url) || (n->url && !url) || (n->url && url && strcmp(n->url, url) != 0);
	if (changed) {
		bfree(n->url);
		n->url = url ? bstrdup(url) : NULL;
		n->dirty = true;
		n->ever_ok = false;
		n->last_ns = 0;
	}
	bfree(n->auth);
	n->auth = auth ? bstrdup(auth) : NULL;
	n->interval = interval;
	if (changed)
		pthread_cond_broadcast(&n->wake);
	pthread_mutex_unlock(&n->lock);
}

void sbk_net_refresh(struct sbk_net *n)
{
	pthread_mutex_lock(&n->lock);
	n->dirty = true;
	pthread_cond_broadcast(&n->wake);
	pthread_mutex_unlock(&n->lock);
}

/* ---- JSON ----------------------------------------------------------------- */

/* Walk "a.b.0.c": a key in an object, or an index in an array. */
static json_t *walk(json_t *root, const char *path)
{
	if (!root || !path || !*path)
		return root;
	json_t *node = root;
	const char *p = path;
	char key[128];
	while (*p && node) {
		const char *dot = strchr(p, '.');
		size_t len = dot ? (size_t)(dot - p) : strlen(p);
		if (len >= sizeof(key))
			return NULL;
		memcpy(key, p, len);
		key[len] = 0;
		if (json_is_array(node)) {
			char *end = NULL;
			long idx = strtol(key, &end, 10);
			node = (end && !*end && idx >= 0) ? json_array_get(node, (size_t)idx) : NULL;
		} else {
			node = json_object_get(node, key);
		}
		if (!dot)
			break;
		p = dot + 1;
	}
	return node;
}

bool sbk_json_number(const char *body, size_t len, const char *path, double *out)
{
	json_error_t e;
	json_t *root = json_loadb(body, len, 0, &e);
	if (!root)
		return false;
	json_t *node = walk(root, path);
	bool ok = false;
	if (json_is_number(node)) {
		*out = json_number_value(node);
		ok = true;
	} else if (json_is_string(node)) {
		/* the YouTube Data API returns its counts as strings */
		char *end = NULL;
		double v = strtod(json_string_value(node), &end);
		if (end && end != json_string_value(node)) {
			*out = v;
			ok = true;
		}
	}
	json_decref(root);
	return ok;
}

char *sbk_json_string(const char *body, size_t len, const char *path)
{
	json_error_t e;
	json_t *root = json_loadb(body, len, 0, &e);
	if (!root)
		return NULL;
	json_t *node = walk(root, path);
	char *out = NULL;
	if (json_is_string(node)) {
		out = bstrdup(json_string_value(node));
	} else if (json_is_number(node)) {
		struct dstr s = {0};
		dstr_printf(&s, "%g", json_number_value(node));
		out = bstrdup(s.array);
		dstr_free(&s);
	}
	json_decref(root);
	return out;
}

char *sbk_url_escape(const char *s)
{
	CURL *c = curl_easy_init();
	if (!c)
		return bstrdup(s ? s : "");
	char *e = curl_easy_escape(c, s ? s : "", 0);
	char *out = bstrdup(e ? e : "");
	if (e)
		curl_free(e);
	curl_easy_cleanup(c);
	return out;
}
