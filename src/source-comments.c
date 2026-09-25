/*  SBK Comments — questions and comments on screen, for a lesson or a Q&A.

    Three places they can come from:

      Written here   a list you type, one per line as "Name: the question".
                     For a course this is the useful one — the questions are
                     written before the lesson and appear on cue.
      YouTube        the live chat of a broadcast. Two calls: the video says
                     which chat it has, then the chat is polled. The API hands
                     back a polling interval and the kit honours it, because
                     ignoring it is how you burn a day's quota before lunch.
      JSON           any endpoint returning a list, with dot-paths to the name
                     and the text.

    Whatever the source, the panel shows the newest few and cycles through them
    on a timer, so a long question is readable rather than flashing past.  */

#include "sbk-common.h"
#include "sbk-text.h"
#include "sbk-stage.h"
#include "sbk-anim.h"
#include "sbk-net.h"

#define MAX_COMMENTS 40
#define MAX_SHOWN 4

enum comment_src { CS_MANUAL = 0, CS_YOUTUBE, CS_JSON };

struct comment {
	char *who, *what;
};

struct panel {
	obs_source_t *self;
	struct sbk_look look;
	struct sbk_anim anim;
	struct sbk_stage stage;
	gs_effect_t *card_fx;
	struct sbk_net net;

	/* rows are rebuilt from the pool every time the selection changes */
	struct sbk_text who[MAX_SHOWN], what[MAX_SHOWN], title;
	float row_y[MAX_SHOWN], row_h[MAX_SHOWN];
	int shown;

	enum comment_src src;
	char *s_title, *manual, *key, *video, *url, *path_list, *path_who, *path_what;
	int show_count;
	double rotate_s, interval;
	bool avatars, newest_first;
	enum sbk_surface surf;
	uint32_t width, cx, cy;

	/* written on the worker, read on the graphics thread */
	pthread_mutex_t lock;
	struct comment pool[MAX_COMMENTS];
	int n_pool;
	/* YouTube needs the chat id before it can poll, and a page token after */
	char *chat_id, *page_token;
	bool asked_for_chat;

	int offset;
	float since_turn;
};

static const char *panel_name(void *u)
{
	UNUSED_PARAMETER(u);
	return obs_module_text("SBK.Comments");
}

static void clear_pool(struct panel *p)
{
	for (int i = 0; i < p->n_pool; i++) {
		bfree(p->pool[i].who);
		bfree(p->pool[i].what);
		p->pool[i].who = p->pool[i].what = NULL;
	}
	p->n_pool = 0;
}

/* Keep the newest MAX_COMMENTS. A chat that has been running for an hour must
   not grow this without bound. */
static void push_comment(struct panel *p, const char *who, const char *what)
{
	if (!what || !*what)
		return;
	if (p->n_pool == MAX_COMMENTS) {
		bfree(p->pool[0].who);
		bfree(p->pool[0].what);
		memmove(&p->pool[0], &p->pool[1], sizeof(p->pool[0]) * (MAX_COMMENTS - 1));
		p->n_pool--;
	}
	p->pool[p->n_pool].who = bstrdup(who ? who : "");
	p->pool[p->n_pool].what = bstrdup(what);
	p->n_pool++;
}

/* "Name: the question" — the colon is optional, and a line without one is all
   message and no name, which is a reasonable thing to want. */
static void parse_manual(struct panel *p)
{
	pthread_mutex_lock(&p->lock);
	clear_pool(p);
	const char *s = p->manual ? p->manual : "";
	while (*s) {
		const char *nl = strchr(s, '\n');
		size_t len = nl ? (size_t)(nl - s) : strlen(s);
		while (len && (s[len - 1] == '\r' || s[len - 1] == ' '))
			len--;
		if (len) {
			char *line = bstrdup_n(s, len);
			char *colon = strchr(line, ':');
			if (colon) {
				*colon = 0;
				const char *msg = colon + 1;
				while (*msg == ' ')
					msg++;
				push_comment(p, line, msg);
			} else {
				push_comment(p, "", line);
			}
			bfree(line);
		}
		if (!nl)
			break;
		s = nl + 1;
	}
	pthread_mutex_unlock(&p->lock);
}

/* ---- the network ------------------------------------------------------------ */

static char *read_key_file(const char *v)
{
	if (!v || !*v)
		return bstrdup("");
	if (v[0] != '@')
		return bstrdup(v);
	char *body = os_quick_read_utf8_file(v + 1);
	if (!body)
		return bstrdup("");
	size_t n = strlen(body);
	while (n && (body[n - 1] == '\n' || body[n - 1] == '\r' || body[n - 1] == ' '))
		body[--n] = 0;
	char *out = bstrdup(body);
	bfree(body);
	return out;
}

static bool panel_prepare(void *param, struct sbk_net *n)
{
	struct panel *p = param;
	if (p->src != CS_YOUTUBE)
		return true;

	char *key = read_key_file(p->key);
	char *k = sbk_url_escape(key);
	bfree(key);
	if (!*k) {
		bfree(k);
		return false;
	}

	struct dstr url = {0};
	if (!p->chat_id) {
		/* phase one: ask the video which chat it has */
		char *v = sbk_url_escape(p->video ? p->video : "");
		dstr_printf(&url, "https://www.googleapis.com/youtube/v3/videos?part=liveStreamingDetails&id=%s&key=%s",
			    v, k);
		bfree(v);
		p->asked_for_chat = true;
	} else {
		char *c = sbk_url_escape(p->chat_id);
		dstr_printf(&url,
			    "https://www.googleapis.com/youtube/v3/liveChat/messages?liveChatId=%s"
			    "&part=snippet,authorDetails&maxResults=50&key=%s",
			    c, k);
		if (p->page_token && *p->page_token) {
			char *t = sbk_url_escape(p->page_token);
			dstr_catf(&url, "&pageToken=%s", t);
			bfree(t);
		}
		bfree(c);
		p->asked_for_chat = false;
	}
	bfree(k);

	bfree(n->url);
	n->url = bstrdup(url.array);
	dstr_free(&url);
	return true;
}

static bool panel_parse(void *param, const char *body, size_t len)
{
	struct panel *p = param;

	if (p->src == CS_YOUTUBE && p->asked_for_chat) {
		char *id = sbk_json_string(body, len, "items.0.liveStreamingDetails.activeLiveChatId");
		if (!id) {
			SBK_LOG(LOG_INFO, "comments: that video has no active live chat");
			return false;
		}
		pthread_mutex_lock(&p->lock);
		bfree(p->chat_id);
		p->chat_id = id;
		pthread_mutex_unlock(&p->lock);
		SBK_LOG(LOG_INFO, "comments: live chat found");
		sbk_net_refresh(&p->net); /* go straight on to the first poll */
		return true;
	}

	pthread_mutex_lock(&p->lock);
	if (p->src == CS_YOUTUBE) {
		char *tok = sbk_json_string(body, len, "nextPageToken");
		bfree(p->page_token);
		p->page_token = tok;
		/* the API says how often it wants to be asked; obeying it is the
		   difference between a day's quota lasting a day and lasting an hour */
		double ms = 0;
		if (sbk_json_number(body, len, "pollingIntervalMillis", &ms) && ms > 0)
			p->net.interval = fmax(3.0, ms / 1000.0);

		for (int i = 0; i < 50; i++) {
			struct dstr a = {0}, m = {0};
			dstr_printf(&a, "items.%d.authorDetails.displayName", i);
			dstr_printf(&m, "items.%d.snippet.displayMessage", i);
			char *who = sbk_json_string(body, len, a.array);
			char *what = sbk_json_string(body, len, m.array);
			dstr_free(&a);
			dstr_free(&m);
			if (!what) {
				bfree(who);
				break;
			}
			push_comment(p, who, what);
			bfree(who);
			bfree(what);
		}
	} else {
		const char *list = p->path_list && *p->path_list ? p->path_list : "";
		clear_pool(p);
		for (int i = 0; i < MAX_COMMENTS; i++) {
			struct dstr a = {0}, m = {0};
			if (*list) {
				dstr_printf(&a, "%s.%d.%s", list, i, p->path_who);
				dstr_printf(&m, "%s.%d.%s", list, i, p->path_what);
			} else {
				dstr_printf(&a, "%d.%s", i, p->path_who);
				dstr_printf(&m, "%d.%s", i, p->path_what);
			}
			char *who = sbk_json_string(body, len, a.array);
			char *what = sbk_json_string(body, len, m.array);
			dstr_free(&a);
			dstr_free(&m);
			if (!what) {
				bfree(who);
				break;
			}
			push_comment(p, who, what);
			bfree(who);
			bfree(what);
		}
	}
	int got = p->n_pool;
	pthread_mutex_unlock(&p->lock);
	return got > 0;
}

static void rebuild_url(struct panel *p)
{
	if (p->src == CS_MANUAL) {
		sbk_net_set(&p->net, "", NULL, 3600.0);
		parse_manual(p);
		return;
	}
	if (p->src == CS_JSON) {
		sbk_net_set(&p->net, p->url ? p->url : "", NULL, p->interval);
		return;
	}
	/* YouTube builds its own URL in prepare, but the poll has to be running
	   for prepare to be called at all */
	sbk_net_set(&p->net, "https://www.googleapis.com/youtube/v3/", NULL, p->interval);
}

/* ---- the source -------------------------------------------------------------- */

static void panel_update(void *data, obs_data_t *s)
{
	struct panel *p = data;
	sbk_look_read(&p->look, s);
	sbk_anim_read(&p->anim, s, 4.0f * sbk_u(&p->look));

	const char *from = obs_data_get_string(s, "from");
	enum comment_src was = p->src;
	p->src = astrcmpi(from, "youtube") == 0 ? CS_YOUTUBE : astrcmpi(from, "json") == 0 ? CS_JSON : CS_MANUAL;

#define SET(f, k)                                           \
	do {                                                \
		bfree(p->f);                                \
		p->f = bstrdup(obs_data_get_string(s, k));  \
	} while (0)
	SET(s_title, "title");
	SET(manual, "manual");
	SET(key, "key");
	SET(video, "video");
	SET(url, "url");
	SET(path_list, "path_list");
	SET(path_who, "path_who");
	SET(path_what, "path_what");
#undef SET

	p->show_count = (int)obs_data_get_int(s, "show_count");
	if (p->show_count < 1)
		p->show_count = 1;
	if (p->show_count > MAX_SHOWN)
		p->show_count = MAX_SHOWN;
	p->rotate_s = obs_data_get_double(s, "rotate");
	p->interval = obs_data_get_double(s, "interval");
	p->avatars = obs_data_get_bool(s, "avatars");
	p->newest_first = obs_data_get_bool(s, "newest_first");
	p->surf = sbk_surface_from(obs_data_get_string(s, "variant"));
	p->width = (uint32_t)obs_data_get_int(s, "width");

	if (was != p->src) {
		pthread_mutex_lock(&p->lock);
		bfree(p->chat_id);
		p->chat_id = NULL;
		bfree(p->page_token);
		p->page_token = NULL;
		clear_pool(p);
		pthread_mutex_unlock(&p->lock);
		p->offset = 0;
	}
	rebuild_url(p);
}

static void *panel_create(obs_data_t *s, obs_source_t *source)
{
	struct panel *p = bzalloc(sizeof(*p));
	p->self = source;
	pthread_mutex_init(&p->lock, NULL);
	p->card_fx = sbk_load_effect("effects/card.effect");
	sbk_stage_init(&p->stage);
	sbk_net_init(&p->net, p, panel_prepare, panel_parse);
	panel_update(p, s);
	sbk_anim_play(&p->anim);
	return p;
}

static void panel_destroy(void *data)
{
	struct panel *p = data;
	sbk_net_free(&p->net);
	clear_pool(p);
	pthread_mutex_destroy(&p->lock);
	for (int i = 0; i < MAX_SHOWN; i++) {
		sbk_text_free(&p->who[i]);
		sbk_text_free(&p->what[i]);
	}
	sbk_text_free(&p->title);
	sbk_stage_free(&p->stage);
	sbk_free_effect(&p->card_fx);
	bfree(p->s_title); bfree(p->manual); bfree(p->key); bfree(p->video);
	bfree(p->url); bfree(p->path_list); bfree(p->path_who); bfree(p->path_what);
	bfree(p->chat_id); bfree(p->page_token);
	bfree(p);
}

static void panel_tick(void *data, float seconds)
{
	struct panel *p = data;
	sbk_anim_tick(&p->anim, seconds);
	const struct sbk_look *l = &p->look;
	const float u = sbk_u(l);
	const bool bare = p->surf == SBK_SURF_NONE;

	pthread_mutex_lock(&p->lock);
	const int total = p->n_pool;
	pthread_mutex_unlock(&p->lock);

	/* turn the page, but only when there is more than a page to turn to */
	p->since_turn += seconds;
	if (p->rotate_s > 0.5 && total > p->show_count && p->since_turn >= p->rotate_s) {
		p->since_turn = 0.0f;
		p->offset = (p->offset + p->show_count) % total;
	}
	if (total <= p->show_count)
		p->offset = 0;

	float pad_x = bare ? 0.0f : 7.0f * u, pad_y = bare ? 0.0f : 6.0f * u;
	int inner = (int)((float)p->width - pad_x * 2.0f - (p->avatars ? 9.0f * u : 0.0f));
	if (inner < 60)
		inner = 60;

	struct dstr t = {0};
	dstr_copy(&t, p->s_title ? p->s_title : "");
	sbk_caps(&t);
	sbk_text_set_full(&p->title, t.array ? t.array : "", l->face, "SemiBold", (int)(4.2f * u),
			  sbk_surface_ink(p->surf, l, true), 0, bare);
	dstr_free(&t);

	float y = pad_y;
	if (p->title.text.len)
		y += (float)sbk_text_h(&p->title) + 3.5f * u;

	/* never repeat: with fewer in the pool than slots, show what there is */
	int slots = p->show_count;
	if (total > 0 && total < slots)
		slots = total;

	p->shown = 0;
	for (int i = 0; i < slots; i++) {
		const char *who = "", *what = "";
		char *who_copy = NULL, *what_copy = NULL;
		pthread_mutex_lock(&p->lock);
		if (total > 0) {
			int idx = p->newest_first ? (total - 1 - ((p->offset + i) % total))
						  : ((p->offset + i) % total);
			who_copy = bstrdup(p->pool[idx].who);
			what_copy = bstrdup(p->pool[idx].what);
		}
		pthread_mutex_unlock(&p->lock);
		if (who_copy) { who = who_copy; what = what_copy; }
		else if (i > 0) { break; }
		else if (p->src == CS_MANUAL) { who = ""; what = "Write the questions in the properties, one per line."; }
		else { who = ""; what = "Waiting for comments…"; }

		sbk_text_set_full(&p->who[i], who, l->face, "SemiBold", (int)(4.4f * u), l->accent, 0, bare);
		sbk_text_set_full(&p->what[i], what, l->face, "Regular", (int)(5.2f * u),
				  sbk_surface_ink(p->surf, l, false), inner, bare);

		p->row_y[i] = y;
		float h = (float)sbk_text_h(&p->what[i]) + (*who ? (float)sbk_text_h(&p->who[i]) + 0.5f * u : 0.0f);
		float avatar = p->avatars ? 8.0f * u : 0.0f;
		p->row_h[i] = fmaxf(h, avatar);
		y += p->row_h[i] + 4.5f * u;
		p->shown++;
		bfree(who_copy);
		bfree(what_copy);
	}
	for (int i = p->shown; i < MAX_SHOWN; i++) {
		sbk_text_set(&p->who[i], "", l->face, "SemiBold", (int)(4.4f * u), l->accent);
		sbk_text_set(&p->what[i], "", l->face, "Regular", (int)(5.2f * u), l->ink);
	}

	p->cx = p->width;
	p->cy = (uint32_t)(y - (p->shown ? 4.5f * u : 0.0f) + pad_y + 0.5f);
}

static uint32_t panel_width(void *d) { return ((struct panel *)d)->cx; }
static uint32_t panel_height(void *d) { return ((struct panel *)d)->cy; }

static void panel_render(void *data, gs_effect_t *unused)
{
	UNUSED_PARAMETER(unused);
	struct panel *p = data;
	if (!p->card_fx || !sbk_stage_begin(&p->stage, p->cx, p->cy))
		return;
	const struct sbk_look *l = &p->look;
	const float u = sbk_u(l);
	float pad_x = p->surf == SBK_SURF_NONE ? 0.0f : 7.0f * u;
	float pad_y = p->surf == SBK_SURF_NONE ? 0.0f : 6.0f * u;

	sbk_surface_draw(p->card_fx, p->surf, l, 0, 0, (float)p->cx, (float)p->cy, 4.0f * u);

	if (p->title.text.len)
		sbk_text_draw(&p->title, pad_x, pad_y);

	for (int i = 0; i < p->shown; i++) {
		float x = pad_x;
		if (p->avatars) {
			/* the first letter in a disc — a stand-in for a picture the
			   kit has no business downloading and caching */
			float d = 8.0f * u;
			sbk_fill(p->card_fx, x, p->row_y[i], d, d, d * 0.5f, sbk_alpha(l->accent, 0.22f));
			x += d + 3.0f * u;
		}
		float y = p->row_y[i];
		if (p->who[i].text.len) {
			sbk_text_draw(&p->who[i], x, y);
			y += (float)sbk_text_h(&p->who[i]) + 0.5f * u;
		}
		sbk_text_draw(&p->what[i], x, y);
		if (i + 1 < p->shown)
			sbk_fill(p->card_fx, pad_x, p->row_y[i] + p->row_h[i] + 2.0f * u,
				 (float)p->cx - pad_x * 2.0f, 1.0f, 0.5f, l->glass_line);
	}

	sbk_stage_end(&p->stage);
	sbk_stage_present_anim(&p->stage, sbk_anim_eval(&p->anim));
}

static void panel_show(void *d) { sbk_anim_on_show(&((struct panel *)d)->anim); }
static void panel_enum(void *d, obs_source_enum_proc_t cb, void *param)
{
	struct panel *p = d;
	sbk_text_enum(&p->title, p->self, cb, param);
	for (int i = 0; i < MAX_SHOWN; i++) {
		sbk_text_enum(&p->who[i], p->self, cb, param);
		sbk_text_enum(&p->what[i], p->self, cb, param);
	}
}

static bool on_from_changed(obs_properties_t *props, obs_property_t *prop, obs_data_t *s)
{
	UNUSED_PARAMETER(prop);
	const char *from = obs_data_get_string(s, "from");
	obs_property_set_visible(obs_properties_get(props, "g_manual"), astrcmpi(from, "manual") == 0);
	obs_property_set_visible(obs_properties_get(props, "g_yt"), astrcmpi(from, "youtube") == 0);
	obs_property_set_visible(obs_properties_get(props, "g_json"), astrcmpi(from, "json") == 0);
	return true;
}

static obs_properties_t *panel_properties(void *data)
{
	UNUSED_PARAMETER(data);
	obs_properties_t *p = obs_properties_create();
	obs_property_t *from = obs_properties_add_list(p, "from", "Comments come from", OBS_COMBO_TYPE_LIST,
						       OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(from, "Written here", "manual");
	obs_property_list_add_string(from, "YouTube live chat", "youtube");
	obs_property_list_add_string(from, "Any JSON endpoint", "json");
	obs_property_set_modified_callback(from, on_from_changed);

	obs_properties_t *man = obs_properties_create();
	obs_property_t *ml = obs_properties_add_text(man, "manual", "One per line", OBS_TEXT_MULTILINE);
	obs_property_set_long_description(ml, "\"Name: the question\". A line with no colon is all message and no "
					      "name. For a lesson this is the useful one — write the questions "
					      "beforehand and they appear on cue.");
	obs_properties_add_group(p, "g_manual", "Written here", OBS_GROUP_NORMAL, man);

	obs_properties_t *yt = obs_properties_create();
	obs_property_t *vid = obs_properties_add_text(yt, "video", "Video ID of the live broadcast", OBS_TEXT_DEFAULT);
	obs_property_set_long_description(vid, "The part after ?v= in the watch URL. The kit asks that video which "
					       "live chat it has, then polls the chat.");
	obs_property_t *ytk = obs_properties_add_text(yt, "key", "API key", OBS_TEXT_PASSWORD);
	obs_property_set_long_description(ytk, "The same YouTube Data API v3 key the counter uses. Begin with @ and a "
					       "path to read it from a file instead of the scene collection.");
	obs_properties_add_group(p, "g_yt", "YouTube", OBS_GROUP_NORMAL, yt);

	obs_properties_t *js = obs_properties_create();
	obs_properties_add_text(js, "url", "URL", OBS_TEXT_DEFAULT);
	obs_property_t *pl = obs_properties_add_text(js, "path_list", "Path to the list", OBS_TEXT_DEFAULT);
	obs_property_set_long_description(pl, "Leave empty if the response is the list itself. Otherwise a dot-path, "
					      "such as \"data.comments\".");
	obs_properties_add_text(js, "path_who", "Field with the name", OBS_TEXT_DEFAULT);
	obs_properties_add_text(js, "path_what", "Field with the message", OBS_TEXT_DEFAULT);
	obs_properties_add_group(p, "g_json", "JSON endpoint", OBS_GROUP_NORMAL, js);

	obs_properties_add_text(p, "title", "Heading", OBS_TEXT_DEFAULT);
	obs_properties_add_int_slider(p, "show_count", "How many at once", 1, MAX_SHOWN, 1);
	obs_properties_add_float_slider(p, "rotate", "Turn the page every (seconds)", 0.0, 60.0, 0.5);
	obs_properties_add_bool(p, "newest_first", "Newest first");
	obs_properties_add_bool(p, "avatars", "Show a disc beside each name");
	obs_properties_add_float_slider(p, "interval", "Check every (seconds)", 15.0, 600.0, 5.0);
	sbk_surface_list(p, "variant", "Variant");
	obs_properties_add_int(p, "width", "Width", 240, 1920, 2);
	sbk_look_props(p, false);
	sbk_anim_props(p);
	obs_properties_add_text(p, "hint",
				"Turning the page at zero holds the same comments on screen. YouTube's API tells "
				"the kit how often it wants to be polled and the kit obeys it, which is the "
				"difference between a day's quota lasting a day and lasting an hour.",
				OBS_TEXT_INFO);
	return p;
}

static void panel_defaults(obs_data_t *s)
{
	obs_data_set_default_string(s, "from", "manual");
	obs_data_set_default_string(s, "manual",
				    "Aarti: How do you structure an org for scale?\n"
				    "Dev: Does this work with managed packages?\n"
				    "Priya: Can you show the deploy step again?");
	obs_data_set_default_string(s, "title", "Questions");
	obs_data_set_default_string(s, "path_list", "");
	obs_data_set_default_string(s, "path_who", "author");
	obs_data_set_default_string(s, "path_what", "text");
	obs_data_set_default_int(s, "show_count", 3);
	obs_data_set_default_double(s, "rotate", 12.0);
	obs_data_set_default_bool(s, "newest_first", true);
	obs_data_set_default_bool(s, "avatars", true);
	obs_data_set_default_double(s, "interval", 20.0);
	obs_data_set_default_string(s, "variant", "card");
	obs_data_set_default_int(s, "width", 560);
	sbk_look_defaults(s);
	sbk_anim_defaults(s, "left");
}

struct obs_source_info sbk_comments_info = {
	.id = "sbk_comments",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
	.get_name = panel_name,
	.create = panel_create,
	.destroy = panel_destroy,
	.update = panel_update,
	.get_defaults = panel_defaults,
	.get_properties = panel_properties,
	.get_width = panel_width,
	.get_height = panel_height,
	.video_tick = panel_tick,
	.video_render = panel_render,
	.show = panel_show,
	.enum_active_sources = panel_enum,
	.icon_type = OBS_ICON_TYPE_TEXT,
};
