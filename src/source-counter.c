/*  SBK Counter — a live number from an API: subscribers, members, whatever a
    creator's own endpoint returns.

    Three providers, and the third is the escape hatch:

      YouTube   an API key and a channel id. Subscribers, views or videos.
      Ghost     a site and an Admin API key. Members, or paid members only.
      JSON      any URL and a dot-path into the response. Everything else —
                Patreon through a proxy, a Cloudflare Worker, a sheet, a
                webhook's cached answer.

    The network never touches the graphics thread: sbk-net polls on a worker and
    this reads the last good value. A failed poll keeps the number that was
    there, because a subscriber count that blinks to zero mid-stream is worse
    than one that is thirty seconds old.

    A note on keys. OBS stores source settings in the scene collection as plain
    JSON, so a key typed here is readable by anyone you send that collection to.
    Read it from a file instead — the key field takes a path beginning with @ —
    and the collection stays shareable.  */

#include <CommonCrypto/CommonHMAC.h>
#include <string.h>

#include "sbk-common.h"
#include "sbk-text.h"
#include "sbk-stage.h"
#include "sbk-anim.h"
#include "sbk-net.h"

enum provider { PROV_YOUTUBE = 0, PROV_GHOST, PROV_JSON };

struct counter {
	obs_source_t *self;
	struct sbk_look look;
	struct sbk_anim anim;
	struct sbk_stage stage;
	gs_effect_t *card_fx;
	struct sbk_text label, figure, delta;
	struct sbk_net net;

	enum provider provider;
	char *s_label, *metric, *key, *channel, *site, *url, *path, *bearer;
	bool compact, show_goal, show_delta, show_dot;
	double goal;
	double interval;
	enum sbk_surface surf;
	uint32_t width, cx, cy;

	/* written on the worker thread, read on the graphics thread */
	pthread_mutex_t vlock;
	double value;
	bool have_value;

	double shown, first_value;
	bool have_first;
	float bar_y, bar_h, label_y, figure_y;
	float t;
	obs_hotkey_id refresh_id;
};

static const char *counter_name(void *u)
{
	UNUSED_PARAMETER(u);
	return obs_module_text("SBK.Counter");
}

/* ---- keys ----------------------------------------------------------------- */

/* "@/path/to/file" reads the key out of a file, so a shared scene collection
   never carries the secret. Anything else is the key itself. */
static char *read_key(const char *v)
{
	if (!v || !*v)
		return bstrdup("");
	if (v[0] != '@')
		return bstrdup(v);
	char *body = os_quick_read_utf8_file(v + 1);
	if (!body) {
		SBK_LOG(LOG_WARNING, "counter: cannot read the key file %s", v + 1);
		return bstrdup("");
	}
	/* a file written by a human ends in a newline */
	size_t n = strlen(body);
	while (n && (body[n - 1] == '\n' || body[n - 1] == '\r' || body[n - 1] == ' '))
		body[--n] = 0;
	char *out = bstrdup(body);
	bfree(body);
	return out;
}

static void b64url(struct dstr *out, const uint8_t *data, size_t len)
{
	static const char T[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
	for (size_t i = 0; i < len; i += 3) {
		uint32_t v = (uint32_t)data[i] << 16;
		size_t have = 1;
		if (i + 1 < len) { v |= (uint32_t)data[i + 1] << 8; have++; }
		if (i + 2 < len) { v |= data[i + 2]; have++; }
		char c[4] = {T[(v >> 18) & 63], T[(v >> 12) & 63], T[(v >> 6) & 63], T[v & 63]};
		dstr_ncat(out, c, have + 1); /* no padding: JWT is base64url without it */
	}
}

static int hexval(char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

/* Ghost's Admin API key is "<id>:<hex secret>", and it wants a short-lived
   HS256 token signed with that secret. Five minutes, per Ghost's own docs. */
static char *ghost_token(const char *admin_key)
{
	if (!admin_key || !*admin_key)
		return NULL;
	const char *colon = strchr(admin_key, ':');
	if (!colon)
		return NULL;
	size_t id_len = (size_t)(colon - admin_key);
	const char *hex = colon + 1;
	size_t hex_len = strlen(hex);
	if (!id_len || hex_len < 2 || hex_len % 2)
		return NULL;

	uint8_t *secret = bmalloc(hex_len / 2);
	for (size_t i = 0; i < hex_len / 2; i++) {
		int hi = hexval(hex[i * 2]), lo = hexval(hex[i * 2 + 1]);
		if (hi < 0 || lo < 0) {
			bfree(secret);
			return NULL;
		}
		secret[i] = (uint8_t)((hi << 4) | lo);
	}

	struct dstr header = {0}, payload = {0}, signing = {0};
	struct dstr hjson = {0}, pjson = {0};
	dstr_printf(&hjson, "{\"alg\":\"HS256\",\"typ\":\"JWT\",\"kid\":\"%.*s\"}", (int)id_len, admin_key);
	long long now = (long long)time(NULL);
	dstr_printf(&pjson, "{\"iat\":%lld,\"exp\":%lld,\"aud\":\"/admin/\"}", now, now + 300);
	b64url(&header, (const uint8_t *)hjson.array, hjson.len);
	b64url(&payload, (const uint8_t *)pjson.array, pjson.len);
	dstr_printf(&signing, "%s.%s", header.array, payload.array);

	uint8_t mac[CC_SHA256_DIGEST_LENGTH];
	CCHmac(kCCHmacAlgSHA256, secret, hex_len / 2, signing.array, signing.len, mac);
	struct dstr sig = {0};
	b64url(&sig, mac, sizeof(mac));

	struct dstr token = {0};
	dstr_printf(&token, "Ghost %s.%s", signing.array, sig.array);
	char *out = bstrdup(token.array);

	bfree(secret);
	dstr_free(&hjson); dstr_free(&pjson); dstr_free(&header);
	dstr_free(&payload); dstr_free(&signing); dstr_free(&sig); dstr_free(&token);
	return out;
}

/* ---- the poll ------------------------------------------------------------- */

static bool counter_prepare(void *param, struct sbk_net *n)
{
	struct counter *c = param;
	if (c->provider != PROV_GHOST)
		return true;
	/* the token expires, so it is minted for every request rather than once */
	char *key = read_key(c->key);
	char *tok = ghost_token(key);
	bfree(key);
	bfree(n->auth);
	n->auth = tok; /* NULL is a valid "no header", and the poll will 401 and say so */
	return tok != NULL;
}

static bool counter_parse(void *param, const char *body, size_t len)
{
	struct counter *c = param;
	const char *path = c->path && *c->path ? c->path : NULL;
	double v = 0.0;
	bool ok = false;

	switch (c->provider) {
	case PROV_YOUTUBE: {
		struct dstr p = {0};
		dstr_printf(&p, "items.0.statistics.%s", c->metric && *c->metric ? c->metric : "subscriberCount");
		ok = sbk_json_number(body, len, p.array, &v);
		dstr_free(&p);
		break;
	}
	case PROV_GHOST:
		ok = sbk_json_number(body, len, "meta.pagination.total", &v);
		break;
	default:
		ok = sbk_json_number(body, len, path ? path : "count", &v);
		break;
	}
	if (!ok)
		return false;

	pthread_mutex_lock(&c->vlock);
	c->value = v;
	c->have_value = true;
	pthread_mutex_unlock(&c->vlock);
	return true;
}

static void rebuild_url(struct counter *c)
{
	struct dstr url = {0};
	char *auth = NULL;

	switch (c->provider) {
	case PROV_YOUTUBE: {
		char *key = read_key(c->key);
		char *ch = sbk_url_escape(c->channel);
		char *k = sbk_url_escape(key);
		if (*ch && *k)
			dstr_printf(&url, "https://www.googleapis.com/youtube/v3/channels?part=statistics&id=%s&key=%s",
				    ch, k);
		bfree(key); bfree(ch); bfree(k);
		break;
	}
	case PROV_GHOST: {
		/* the site, with any trailing slash taken off, plus the members
		   endpoint. limit=1 because only the total is wanted, and asking
		   for one member is far cheaper than asking for all of them. */
		struct dstr site = {0};
		dstr_copy(&site, c->site && *c->site ? c->site : "");
		while (site.len && site.array[site.len - 1] == '/')
			dstr_resize(&site, site.len - 1);
		if (site.len) {
			bool paid = c->metric && astrcmpi(c->metric, "paid") == 0;
			if (paid)
				dstr_printf(&url, "%s/ghost/api/admin/members/?limit=1&filter=status:paid", site.array);
			else
				dstr_printf(&url, "%s/ghost/api/admin/members/?limit=1", site.array);
		}
		dstr_free(&site);
		break;
	}
	default: {
		dstr_copy(&url, c->url ? c->url : "");
		char *b = read_key(c->bearer);
		if (*b) {
			struct dstr h = {0};
			dstr_printf(&h, "Bearer %s", b);
			auth = bstrdup(h.array);
			dstr_free(&h);
		}
		bfree(b);
		break;
	}
	}

	sbk_net_set(&c->net, url.array ? url.array : "", auth, c->interval);
	bfree(auth);
	dstr_free(&url);
}

/* ---- the source ----------------------------------------------------------- */

static void on_refresh(void *data, obs_hotkey_id id, obs_hotkey_t *hk, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hk);
	if (pressed)
		sbk_net_refresh(&((struct counter *)data)->net);
}

static void counter_update(void *data, obs_data_t *s)
{
	struct counter *c = data;
	sbk_look_read(&c->look, s);
	sbk_anim_read(&c->anim, s, 4.0f * sbk_u(&c->look));

	const char *prov = obs_data_get_string(s, "provider");
	c->provider = astrcmpi(prov, "ghost") == 0 ? PROV_GHOST : astrcmpi(prov, "json") == 0 ? PROV_JSON : PROV_YOUTUBE;

#define SET(field, key)                                     \
	do {                                                \
		bfree(c->field);                            \
		c->field = bstrdup(obs_data_get_string(s, key)); \
	} while (0)
	SET(s_label, "label");
	SET(metric, "metric");
	SET(key, "key");
	SET(channel, "channel");
	SET(site, "site");
	SET(url, "url");
	SET(path, "path");
	SET(bearer, "bearer");
#undef SET

	c->compact = obs_data_get_bool(s, "compact");
	c->show_goal = obs_data_get_bool(s, "show_goal");
	c->show_delta = obs_data_get_bool(s, "show_delta");
	c->show_dot = obs_data_get_bool(s, "show_dot");
	c->goal = obs_data_get_double(s, "goal");
	c->interval = obs_data_get_double(s, "interval");
	c->surf = sbk_surface_from(obs_data_get_string(s, "variant"));
	c->width = (uint32_t)obs_data_get_int(s, "width");
	rebuild_url(c);
}

static void *counter_create(obs_data_t *s, obs_source_t *source)
{
	struct counter *c = bzalloc(sizeof(*c));
	c->self = source;
	pthread_mutex_init(&c->vlock, NULL);
	c->card_fx = sbk_load_effect("effects/card.effect");
	sbk_stage_init(&c->stage);
	sbk_net_init(&c->net, c, counter_prepare, counter_parse);
	c->refresh_id = obs_hotkey_register_source(source, "SBK.Counter.Refresh", "Broadcast Kit: refresh the counter",
						   on_refresh, c);
	counter_update(c, s);
	sbk_anim_play(&c->anim);
	return c;
}

static void counter_destroy(void *data)
{
	struct counter *c = data;
	if (c->refresh_id != OBS_INVALID_HOTKEY_ID)
		obs_hotkey_unregister(c->refresh_id);
	sbk_net_free(&c->net);
	pthread_mutex_destroy(&c->vlock);
	sbk_text_free(&c->label);
	sbk_text_free(&c->figure);
	sbk_text_free(&c->delta);
	sbk_stage_free(&c->stage);
	sbk_free_effect(&c->card_fx);
	bfree(c->s_label); bfree(c->metric); bfree(c->key); bfree(c->channel);
	bfree(c->site); bfree(c->url); bfree(c->path); bfree(c->bearer);
	bfree(c);
}

/* 12400 → "12.4k" so a six-figure count still fits the card */
static void human(char *buf, size_t n, double v, bool compact)
{
	double a = fabs(v);
	if (compact && a >= 1000000.0)
		snprintf(buf, n, "%.1fM", v / 1000000.0);
	else if (compact && a >= 10000.0)
		snprintf(buf, n, "%.1fk", v / 1000.0);
	else {
		/* thousands separators, because 104829 is unreadable at a glance */
		char raw[32];
		snprintf(raw, sizeof(raw), "%.0f", fabs(v));
		size_t len = strlen(raw), out = 0;
		if (v < 0 && out + 1 < n)
			buf[out++] = '-';
		for (size_t i = 0; i < len && out + 2 < n; i++) {
			if (i && (len - i) % 3 == 0)
				buf[out++] = ',';
			buf[out++] = raw[i];
		}
		buf[out] = 0;
	}
}

static void counter_tick(void *data, float seconds)
{
	struct counter *c = data;
	c->t += seconds;
	sbk_anim_tick(&c->anim, seconds);
	const struct sbk_look *l = &c->look;
	const float u = sbk_u(l);
	const bool bare = c->surf == SBK_SURF_NONE;

	pthread_mutex_lock(&c->vlock);
	double value = c->value;
	bool have = c->have_value;
	pthread_mutex_unlock(&c->vlock);

	if (have && !c->have_first) {
		c->first_value = value;
		c->have_first = true;
		c->shown = value;
	}
	/* count up to a new value rather than snapping: a jump reads as a glitch,
	   and the climb is the bit an audience enjoys */
	if (have)
		c->shown += (value - c->shown) * sbk_clampf(seconds * 3.0f, 0.0f, 1.0f);

	char figs[48];
	if (have)
		human(figs, sizeof(figs), c->shown + 0.5, c->compact);
	else
		snprintf(figs, sizeof(figs), "—");

	struct dstr lab = {0};
	dstr_copy(&lab, c->s_label ? c->s_label : "");
	sbk_caps(&lab);
	sbk_text_set_full(&c->label, lab.array ? lab.array : "", l->face, "SemiBold", (int)(4.0f * u),
			  sbk_surface_ink(c->surf, l, true), 0, bare);
	dstr_free(&lab);
	sbk_text_set_full(&c->figure, figs, l->mono, "Bold", (int)(11.0f * u), sbk_surface_ink(c->surf, l, false), 0, bare);

	char delta[32] = {0};
	if (c->show_delta && c->have_first) {
		double d = value - c->first_value;
		if (d > 0.5)
			snprintf(delta, sizeof(delta), "+%.0f", d);
		else if (d < -0.5)
			snprintf(delta, sizeof(delta), "%.0f", d);
	}
	sbk_text_set_full(&c->delta, delta, l->mono, "Bold", (int)(4.5f * u),
			  delta[0] == '+' ? sbk_vec(SBK_OK) : sbk_vec(SBK_REC), 0, bare);

	float pad_x = bare ? 0.0f : 6.0f * u, pad_y = bare ? 0.0f : 5.0f * u;
	c->label_y = pad_y;
	c->figure_y = pad_y + (float)sbk_text_h(&c->label) + 1.0f * u;
	float y = c->figure_y + (float)sbk_text_h(&c->figure);
	c->bar_h = 2.0f * u;
	if (c->show_goal && c->goal > 0.0) {
		y += 3.0f * u;
		c->bar_y = y;
		y += c->bar_h;
	} else {
		c->bar_y = 0.0f;
	}
	c->cx = c->width;
	c->cy = (uint32_t)(y + pad_y + 0.5f);
	UNUSED_PARAMETER(pad_x);
}

static uint32_t counter_width(void *d) { return ((struct counter *)d)->cx; }
static uint32_t counter_height(void *d) { return ((struct counter *)d)->cy; }

static void counter_render(void *data, gs_effect_t *unused)
{
	UNUSED_PARAMETER(unused);
	struct counter *c = data;
	if (!c->card_fx || !sbk_stage_begin(&c->stage, c->cx, c->cy))
		return;
	const struct sbk_look *l = &c->look;
	const float u = sbk_u(l);
	float pad_x = c->surf == SBK_SURF_NONE ? 0.0f : 6.0f * u;

	sbk_surface_draw(c->card_fx, c->surf, l, 0, 0, (float)c->cx, (float)c->cy, 3.0f * u);

	sbk_text_draw(&c->label, pad_x, c->label_y);
	sbk_text_draw(&c->figure, pad_x, c->figure_y);
	if (c->delta.text.len)
		sbk_text_draw(&c->delta, pad_x + (float)sbk_text_w(&c->figure) + 2.5f * u,
			      c->figure_y + (float)sbk_text_h(&c->figure) - (float)sbk_text_h(&c->delta) - 1.0f * u);

	/* a quiet lamp for the poll: green once it has data, amber while the
	   last request failed, grey before the first answer */
	if (c->show_dot) {
		pthread_mutex_lock(&c->net.lock);
		bool ok = c->net.ok, ever = c->net.ever_ok;
		pthread_mutex_unlock(&c->net.lock);
		float d = 1.75f * u;
		struct vec4 lamp = !ever ? sbk_vec(SBK_OFF) : (ok ? sbk_vec(SBK_OK) : sbk_vec(0xFF33C4FFu));
		float pulse = 0.5f + 0.5f * sinf(c->t * 6.2831853f / SBK_PULSE_SECS);
		sbk_dot(c->card_fx, (float)c->cx - pad_x - d, c->label_y + d * 0.2f, d, lamp, ok ? 0.0f : 1.5f * u,
			0.3f + 0.4f * pulse);
	}

	if (c->bar_y > 0.0f) {
		float w = (float)c->cx - pad_x * 2.0f;
		struct vec4 track = c->surf == SBK_SURF_ACCENT ? sbk_alpha(l->on_accent, 0.25f) : l->wash;
		sbk_fill(c->card_fx, pad_x, c->bar_y, w, c->bar_h, c->bar_h * 0.5f, track);
		float k = sbk_clampf((float)(c->shown / c->goal), 0.0f, 1.0f);
		if (k > 0.001f)
			sbk_fill(c->card_fx, pad_x, c->bar_y, w * k, c->bar_h, c->bar_h * 0.5f, l->accent);
	}

	sbk_stage_end(&c->stage);
	struct sbk_anim_out a = sbk_anim_eval(&c->anim);
	sbk_stage_present(&c->stage, a.alpha, a.dx, a.dy);
}

static void counter_show(void *d) { sbk_anim_on_show(&((struct counter *)d)->anim); }
static void counter_enum(void *d, obs_source_enum_proc_t cb, void *p)
{
	struct counter *c = d;
	sbk_text_enum(&c->label, c->self, cb, p);
	sbk_text_enum(&c->figure, c->self, cb, p);
	sbk_text_enum(&c->delta, c->self, cb, p);
}

static bool on_provider_changed(obs_properties_t *props, obs_property_t *p, obs_data_t *s)
{
	UNUSED_PARAMETER(p);
	const char *prov = obs_data_get_string(s, "provider");
	bool yt = astrcmpi(prov, "youtube") == 0;
	bool gh = astrcmpi(prov, "ghost") == 0;
	bool js = astrcmpi(prov, "json") == 0;
	obs_property_set_visible(obs_properties_get(props, "yt"), yt);
	obs_property_set_visible(obs_properties_get(props, "gh"), gh);
	obs_property_set_visible(obs_properties_get(props, "js"), js);
	return true;
}

static obs_properties_t *counter_properties(void *data)
{
	UNUSED_PARAMETER(data);
	obs_properties_t *p = obs_properties_create();
	obs_property_t *prov = obs_properties_add_list(p, "provider", "Where the number comes from",
						       OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(prov, "YouTube", "youtube");
	obs_property_list_add_string(prov, "Ghost — members", "ghost");
	obs_property_list_add_string(prov, "Any JSON endpoint", "json");
	obs_property_set_modified_callback(prov, on_provider_changed);

	obs_properties_t *yt = obs_properties_create();
	obs_properties_add_text(yt, "channel", "Channel ID (starts with UC…)", OBS_TEXT_DEFAULT);
	obs_property_t *ytk = obs_properties_add_text(yt, "key", "API key", OBS_TEXT_PASSWORD);
	obs_property_set_long_description(ytk,
		"console.cloud.google.com → a project → enable the YouTube Data API v3 → Credentials → "
		"API key. Restrict it to that one API. Begin the field with @ to read the key out of a "
		"file instead, e.g. @/Users/you/.youtube-key");
	obs_property_t *ytm = obs_properties_add_list(yt, "metric", "Count", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(ytm, "Subscribers", "subscriberCount");
	obs_property_list_add_string(ytm, "Views", "viewCount");
	obs_property_list_add_string(ytm, "Videos", "videoCount");
	obs_properties_add_text(yt, "yt_note",
				"YouTube rounds public subscriber counts (1.23k shows as 1230). That is the "
				"API's own behaviour, not the kit's.", OBS_TEXT_INFO);
	obs_properties_add_group(p, "yt", "YouTube", OBS_GROUP_NORMAL, yt);

	obs_properties_t *gh = obs_properties_create();
	obs_properties_add_text(gh, "site", "Site URL (https://example.com)", OBS_TEXT_DEFAULT);
	obs_property_t *ghk = obs_properties_add_text(gh, "key", "Admin API key", OBS_TEXT_PASSWORD);
	obs_property_set_long_description(ghk,
		"Ghost Admin → Settings → Integrations → Add custom integration. Copy the Admin API key, "
		"which looks like <id>:<hex secret>. Begin the field with @ to read it from a file.");
	obs_property_t *ghm = obs_properties_add_list(gh, "metric", "Count", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(ghm, "All members", "all");
	obs_property_list_add_string(ghm, "Paid members", "paid");
	obs_properties_add_group(p, "gh", "Ghost", OBS_GROUP_NORMAL, gh);

	obs_properties_t *js = obs_properties_create();
	obs_properties_add_text(js, "url", "URL", OBS_TEXT_DEFAULT);
	obs_property_t *jp = obs_properties_add_text(js, "path", "Path to the number", OBS_TEXT_DEFAULT);
	obs_property_set_long_description(jp,
		"Dots walk objects and array indexes: \"count\", \"data.total\", \"items.0.stats.followers\".");
	obs_properties_add_text(js, "bearer", "Bearer token (optional)", OBS_TEXT_PASSWORD);
	obs_properties_add_group(p, "js", "JSON endpoint", OBS_GROUP_NORMAL, js);

	obs_properties_add_text(p, "label", "Label", OBS_TEXT_DEFAULT);
	obs_properties_add_bool(p, "compact", "Shorten big numbers (12.4k)");
	obs_properties_add_bool(p, "show_delta", "Show the change since this source started");
	obs_properties_add_bool(p, "show_dot", "Show the connection lamp");
	obs_properties_add_bool(p, "show_goal", "Show a goal bar");
	obs_properties_add_float(p, "goal", "Goal", 1.0, 1000000000.0, 1.0);
	obs_properties_add_float_slider(p, "interval", "Check every (seconds)", 15.0, 900.0, 5.0);
	sbk_surface_list(p, "variant", "Variant");
	obs_properties_add_int(p, "width", "Width", 160, 1920, 2);
	sbk_look_props(p, true);
	sbk_anim_props(p);
	obs_properties_add_text(p, "hint",
				"Keys typed here are saved in the scene collection as plain text. Begin a key "
				"field with @ and a file path to keep it out. Bind \"Broadcast Kit: refresh the "
				"counter\" under Settings → Hotkeys to fetch on demand.",
				OBS_TEXT_INFO);
	return p;
}

static void counter_defaults(obs_data_t *s)
{
	obs_data_set_default_string(s, "provider", "youtube");
	obs_data_set_default_string(s, "metric", "subscriberCount");
	obs_data_set_default_string(s, "label", "Subscribers");
	obs_data_set_default_string(s, "path", "count");
	obs_data_set_default_bool(s, "compact", true);
	obs_data_set_default_bool(s, "show_delta", true);
	obs_data_set_default_bool(s, "show_dot", true);
	obs_data_set_default_bool(s, "show_goal", false);
	obs_data_set_default_double(s, "goal", 1000.0);
	obs_data_set_default_double(s, "interval", 60.0);
	obs_data_set_default_string(s, "variant", "card");
	obs_data_set_default_int(s, "width", 420);
	sbk_look_defaults(s);
	sbk_anim_defaults(s, "fade");
}

struct obs_source_info sbk_counter_info = {
	.id = "sbk_counter",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
	.get_name = counter_name,
	.create = counter_create,
	.destroy = counter_destroy,
	.update = counter_update,
	.get_defaults = counter_defaults,
	.get_properties = counter_properties,
	.get_width = counter_width,
	.get_height = counter_height,
	.video_tick = counter_tick,
	.video_render = counter_render,
	.show = counter_show,
	.enum_active_sources = counter_enum,
	.icon_type = OBS_ICON_TYPE_TEXT,
};
