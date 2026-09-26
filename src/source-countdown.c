/*  SBK Timer — the clock that counts, in four modes and four styles.

    Counting down to a duration is the common one, but a stream also wants to
    count down to a time of day ("we start at 21:30" survives you being late),
    count up from zero for a segment, and show how long the broadcast has
    actually been running. All four are the same digits with a different source
    of truth, so they are one source rather than four.

    The id is still sbk_countdown so scenes built before it grew up keep working.  */

#include <time.h>

#include "sbk-common.h"
#include "sbk-text.h"
#include "sbk-stage.h"
#include "sbk-anim.h"
#include "sbk-state.h"

enum tmode { T_DURATION = 0, T_TIME_OF_DAY, T_UPTIME, T_STOPWATCH };
enum tstyle { TS_DIGITS = 0, TS_RING, TS_RING_ONLY, TS_BAR };

struct countdown {
	obs_source_t *self;
	struct sbk_look look;
	struct sbk_anim anim;
	struct sbk_stage stage;
	gs_effect_t *card_fx, *ring_fx;
	struct sbk_text figures, label;

	enum tmode mode;
	enum tstyle style;
	bool restart_on_show, draw_card, running, done_fired, show_hours;
	int minutes, seconds_v;
	char *at, *done_word, *s_label;
	uint64_t target_ns;   /* counting down: when it reaches zero */
	uint64_t started_ns;  /* counting up: when it began */
	double span_s;        /* the full length, for the ring */
	obs_hotkey_id restart_id, pause_id;
	uint32_t cx, cy;
	float ring_d, bar_h;
};

static const char *cd_name(void *u)
{
	UNUSED_PARAMETER(u);
	return obs_module_text("SBK.Countdown");
}

/* seconds until the next HH:MM on the wall clock */
static int64_t secs_until(const char *hhmm)
{
	int hh = 0, mm = 0;
	if (!hhmm || sscanf(hhmm, "%d:%d", &hh, &mm) < 2)
		return 0;
	time_t now = time(NULL);
	struct tm t;
	localtime_r(&now, &t);
	t.tm_hour = hh;
	t.tm_min = mm;
	t.tm_sec = 0;
	time_t target = mktime(&t);
	if (target <= now)
		target += 24 * 3600;
	return (int64_t)(target - now);
}

static void restart(struct countdown *c)
{
	uint64_t now = os_gettime_ns();
	c->started_ns = now;
	c->done_fired = false;
	c->running = true;
	if (c->mode == T_TIME_OF_DAY) {
		int64_t s = secs_until(c->at);
		c->span_s = (double)s;
		c->target_ns = now + (uint64_t)(s < 0 ? 0 : s) * 1000000000ULL;
	} else if (c->mode == T_DURATION) {
		int64_t s = (int64_t)c->minutes * 60 + c->seconds_v;
		if (s < 0)
			s = 0;
		c->span_s = (double)s;
		c->target_ns = now + (uint64_t)s * 1000000000ULL;
	} else {
		c->span_s = (double)((int64_t)c->minutes * 60 + c->seconds_v);
		c->target_ns = 0;
	}
}

static void on_restart(void *data, obs_hotkey_id id, obs_hotkey_t *hk, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hk);
	if (pressed)
		restart(data);
}
static void on_pause(void *data, obs_hotkey_id id, obs_hotkey_t *hk, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hk);
	struct countdown *c = data;
	if (!pressed)
		return;
	/* pausing freezes by moving the target with the clock, so resuming does
	   not have to reconstruct where it was */
	c->running = !c->running;
}

static void cd_update(void *data, obs_data_t *s)
{
	struct countdown *c = data;
	sbk_look_read(&c->look, s);
	sbk_anim_read(&c->anim, s, 4.0f * sbk_u(&c->look));

	const char *m = obs_data_get_string(s, "mode");
	c->mode = astrcmpi(m, "time") == 0      ? T_TIME_OF_DAY
		  : astrcmpi(m, "uptime") == 0  ? T_UPTIME
		  : astrcmpi(m, "up") == 0      ? T_STOPWATCH
						: T_DURATION;
	const char *st = obs_data_get_string(s, "style");
	c->style = astrcmpi(st, "ring") == 0        ? TS_RING
		   : astrcmpi(st, "ring-only") == 0 ? TS_RING_ONLY
		   : astrcmpi(st, "bar") == 0       ? TS_BAR
						    : TS_DIGITS;

	c->minutes = (int)obs_data_get_int(s, "minutes");
	c->seconds_v = (int)obs_data_get_int(s, "seconds");
	bfree(c->at);
	bfree(c->done_word);
	bfree(c->s_label);
	c->at = bstrdup(obs_data_get_string(s, "at"));
	c->done_word = bstrdup(obs_data_get_string(s, "done"));
	c->s_label = bstrdup(obs_data_get_string(s, "label"));
	c->restart_on_show = obs_data_get_bool(s, "restart_on_show");
	c->draw_card = obs_data_get_bool(s, "draw_card");
	c->show_hours = obs_data_get_bool(s, "show_hours");
	c->ring_d = (float)obs_data_get_int(s, "ring_size");
	restart(c);
}

static void *cd_create(obs_data_t *s, obs_source_t *source)
{
	struct countdown *c = bzalloc(sizeof(*c));
	c->self = source;
	c->card_fx = sbk_load_effect("effects/card.effect");
	c->ring_fx = sbk_load_effect("effects/ring.effect");
	sbk_stage_init(&c->stage);
	c->restart_id = obs_hotkey_register_source(source, "SBK.Countdown.Restart",
						   "Broadcast Kit: restart the timer", on_restart, c);
	c->pause_id = obs_hotkey_register_source(source, "SBK.Countdown.Pause",
						 "Broadcast Kit: pause or resume the timer", on_pause, c);
	cd_update(c, s);
	sbk_anim_play(&c->anim);
	return c;
}

static void cd_destroy(void *data)
{
	struct countdown *c = data;
	if (c->restart_id != OBS_INVALID_HOTKEY_ID)
		obs_hotkey_unregister(c->restart_id);
	if (c->pause_id != OBS_INVALID_HOTKEY_ID)
		obs_hotkey_unregister(c->pause_id);
	sbk_text_free(&c->figures);
	sbk_text_free(&c->label);
	sbk_stage_free(&c->stage);
	sbk_free_effect(&c->card_fx);
	sbk_free_effect(&c->ring_fx);
	bfree(c->at);
	bfree(c->done_word);
	bfree(c->s_label);
	bfree(c);
}

/* seconds on the clock, and how far through the span we are */
static void read_clock(struct countdown *c, double *secs, float *progress, bool *finished)
{
	uint64_t now = os_gettime_ns();
	*finished = false;
	if (c->mode == T_UPTIME) {
		double up = sbk_state_uptime(false);
		if (up <= 0.0)
			up = sbk_state_uptime(true);
		*secs = up;
		*progress = c->span_s > 0.0 ? (float)sbk_clampf((float)(up / c->span_s), 0.0f, 1.0f) : 0.0f;
		return;
	}
	if (c->mode == T_STOPWATCH) {
		double up = (double)(now - c->started_ns) / 1e9;
		*secs = up;
		*progress = c->span_s > 0.0 ? (float)sbk_clampf((float)(up / c->span_s), 0.0f, 1.0f) : 0.0f;
		return;
	}
	double left = now >= c->target_ns ? 0.0 : (double)(c->target_ns - now) / 1e9;
	*secs = left;
	*finished = left <= 0.0;
	/* the ring empties as the time runs out, which is the only way round that
	   reads as "time left" rather than "time spent" */
	*progress = c->span_s > 0.0 ? (float)sbk_clampf((float)(left / c->span_s), 0.0f, 1.0f) : 0.0f;
}

static void cd_tick(void *data, float seconds)
{
	struct countdown *c = data;
	sbk_anim_tick(&c->anim, seconds);
	const struct sbk_look *l = &c->look;
	const float u = sbk_u(l);

	if (!c->running && (c->mode == T_DURATION || c->mode == T_TIME_OF_DAY)) {
		/* frozen: hold the target the same distance away */
		c->target_ns += (uint64_t)(seconds * 1e9);
	}
	if (!c->running && c->mode == T_STOPWATCH)
		c->started_ns += (uint64_t)(seconds * 1e9);

	double secs;
	float progress;
	bool finished;
	read_clock(c, &secs, &progress, &finished);

	char buf[64];
	if (finished && c->done_word && *c->done_word) {
		snprintf(buf, sizeof(buf), "%s", c->done_word);
		if (!c->done_fired) {
			c->done_fired = true;
			SBK_LOG(LOG_INFO, "timer '%s' reached zero", obs_source_get_name(c->self));
		}
	} else {
		int64_t t = (int64_t)(secs + 0.5);
		if (t >= 3600 || c->show_hours)
			snprintf(buf, sizeof(buf), "%lld:%02lld:%02lld", (long long)(t / 3600),
				 (long long)((t % 3600) / 60), (long long)(t % 60));
		else
			snprintf(buf, sizeof(buf), "%02lld:%02lld", (long long)(t / 60), (long long)(t % 60));
	}

	bool ring = c->style == TS_RING || c->style == TS_RING_ONLY;
	int size = ring ? (int)(c->ring_d * 0.26f) : (int)(24.0f * u);
	sbk_text_set(&c->figures, c->style == TS_RING_ONLY ? "" : buf, l->mono, "Bold", size, l->ink);

	struct dstr lab = {0};
	dstr_copy(&lab, c->s_label ? c->s_label : "");
	sbk_caps(&lab);
	sbk_text_set(&c->label, lab.array ? lab.array : "", l->face, "SemiBold", (int)(4.0f * u), l->ink_faint);
	dstr_free(&lab);

	float pad_x = c->draw_card ? 8.0f * u : 0.0f, pad_y = c->draw_card ? 4.0f * u : 0.0f;
	float fw = (float)sbk_text_w(&c->figures), fh = (float)sbk_text_h(&c->figures);
	float lw = c->label.text.len ? (float)sbk_text_w(&c->label) : 0.0f;
	float lh = c->label.text.len ? (float)sbk_text_h(&c->label) + 1.5f * u : 0.0f;

	if (ring) {
		c->cx = (uint32_t)(c->ring_d + pad_x * 2.0f + 0.5f);
		c->cy = (uint32_t)(c->ring_d + pad_y * 2.0f + lh + 0.5f);
	} else if (c->style == TS_BAR) {
		c->bar_h = 2.0f * u;
		float w = fmaxf(fw, fmaxf(lw, 40.0f * u));
		c->cx = (uint32_t)(w + pad_x * 2.0f + 0.5f);
		c->cy = (uint32_t)(pad_y * 2.0f + lh + fh + 2.5f * u + c->bar_h + 0.5f);
	} else {
		c->cx = (uint32_t)(fmaxf(fw, lw) + pad_x * 2.0f + 0.5f);
		c->cy = (uint32_t)(pad_y * 2.0f + lh + fh + 0.5f);
	}
}

static uint32_t cd_width(void *d) { return ((struct countdown *)d)->cx; }
static uint32_t cd_height(void *d) { return ((struct countdown *)d)->cy; }

static void cd_render(void *data, gs_effect_t *unused)
{
	UNUSED_PARAMETER(unused);
	struct countdown *c = data;
	if (!c->card_fx || !sbk_stage_begin(&c->stage, c->cx, c->cy))
		return;
	const struct sbk_look *l = &c->look;
	const float u = sbk_u(l);
	struct vec4 none = SBK_NONE;
	float pad_x = c->draw_card ? 8.0f * u : 0.0f, pad_y = c->draw_card ? 4.0f * u : 0.0f;

	if (c->draw_card)
		sbk_card(c->card_fx, 0, 0, (float)c->cx, (float)c->cy, 5.0f * u, l->glass, l->glass_line, 1.0f, none,
			 0.0f);

	double secs;
	float progress;
	bool finished;
	read_clock(c, &secs, &progress, &finished);

	float y = pad_y;
	if (c->label.text.len) {
		sbk_text_draw(&c->label, ((float)c->cx - (float)sbk_text_w(&c->label)) * 0.5f, y);
		y += (float)sbk_text_h(&c->label) + 1.5f * u;
	}

	bool ring = c->style == TS_RING || c->style == TS_RING_ONLY;
	if (ring && c->ring_fx) {
		struct vec4 track = l->wash;
		/* the last ten seconds go red, which is the only warning a countdown
		   on a waiting screen ever gets to give */
		struct vec4 fill = (!finished && secs <= 10.0 && (c->mode == T_DURATION || c->mode == T_TIME_OF_DAY))
					   ? sbk_vec(SBK_REC)
					   : l->accent;
		sbk_ring(c->ring_fx, pad_x, y, c->ring_d, fmaxf(3.0f, c->ring_d * 0.055f), progress, track, fill, true);
		if (c->figures.text.len)
			sbk_text_draw(&c->figures, pad_x + (c->ring_d - (float)sbk_text_w(&c->figures)) * 0.5f,
				      y + (c->ring_d - (float)sbk_text_h(&c->figures)) * 0.5f);
	} else {
		sbk_text_draw(&c->figures, ((float)c->cx - (float)sbk_text_w(&c->figures)) * 0.5f, y);
		if (c->style == TS_BAR) {
			float by = y + (float)sbk_text_h(&c->figures) + 2.5f * u;
			float w = (float)c->cx - pad_x * 2.0f;
			sbk_fill(c->card_fx, pad_x, by, w, c->bar_h, c->bar_h * 0.5f, l->wash);
			if (progress > 0.001f)
				sbk_fill(c->card_fx, pad_x, by, w * progress, c->bar_h, c->bar_h * 0.5f, l->accent);
		}
	}

	sbk_stage_end(&c->stage);
	sbk_stage_present_anim(&c->stage, sbk_anim_eval(&c->anim));
}

static void cd_show(void *d)
{
	struct countdown *c = d;
	sbk_anim_on_show(&c->anim);
	if (c->restart_on_show && c->mode != T_UPTIME)
		restart(c);
}
static void cd_enum(void *d, obs_source_enum_proc_t cb, void *p)
{
	struct countdown *c = d;
	sbk_text_enum(&c->figures, c->self, cb, p);
	sbk_text_enum(&c->label, c->self, cb, p);
}

static obs_properties_t *cd_properties(void *data)
{
	UNUSED_PARAMETER(data);
	obs_properties_t *p = obs_properties_create();
	obs_property_t *m = obs_properties_add_list(p, "mode", "Counts", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(m, "Down, for a duration", "duration");
	obs_property_list_add_string(m, "Down, to a time of day", "time");
	obs_property_list_add_string(m, "Up, from zero", "up");
	obs_property_list_add_string(m, "Up, since the stream started", "uptime");
	obs_property_t *st = obs_properties_add_list(p, "style", "Style", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(st, "Digits", "digits");
	obs_property_list_add_string(st, "Digits in a ring", "ring");
	obs_property_list_add_string(st, "Ring only", "ring-only");
	obs_property_list_add_string(st, "Digits over a bar", "bar");

	obs_properties_add_text(p, "label", "Label above it", OBS_TEXT_DEFAULT);
	obs_properties_add_int(p, "minutes", "Minutes", 0, 600, 1);
	obs_properties_add_int(p, "seconds", "Seconds", 0, 59, 1);
	obs_property_t *at = obs_properties_add_text(p, "at", "Time of day (HH:MM, 24-hour)", OBS_TEXT_DEFAULT);
	obs_property_set_long_description(at, "If that time has already passed today it counts to tomorrow, so "
					      "\"21:30\" keeps working after 21:30.");
	obs_properties_add_text(p, "done", "Word at zero", OBS_TEXT_DEFAULT);
	obs_properties_add_bool(p, "show_hours", "Always show hours");
	obs_properties_add_int(p, "ring_size", "Ring size (px)", 80, 900, 4);
	obs_properties_add_bool(p, "restart_on_show", "Restart each time the source is shown");
	obs_properties_add_bool(p, "draw_card", "Draw a card behind it");
	sbk_look_props(p, true);
	sbk_anim_props(p);
	obs_properties_add_text(p, "hint",
				"Counting up from zero needs a length too — it is what the ring and the bar fill "
				"against. Two hotkeys under Settings → Hotkeys: restart, and pause or resume.",
				OBS_TEXT_INFO);
	return p;
}

static void cd_defaults(obs_data_t *s)
{
	obs_data_set_default_string(s, "mode", "duration");
	obs_data_set_default_string(s, "style", "digits");
	obs_data_set_default_string(s, "label", "");
	obs_data_set_default_int(s, "minutes", 15);
	obs_data_set_default_int(s, "seconds", 0);
	obs_data_set_default_string(s, "at", "21:30");
	obs_data_set_default_string(s, "done", "Now");
	obs_data_set_default_bool(s, "show_hours", false);
	obs_data_set_default_int(s, "ring_size", 260);
	obs_data_set_default_bool(s, "restart_on_show", true);
	obs_data_set_default_bool(s, "draw_card", false);
	sbk_look_defaults(s);
	sbk_anim_defaults(s, "fade");
}

struct obs_source_info sbk_countdown_info = {
	.id = "sbk_countdown",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
	.get_name = cd_name,
	.create = cd_create,
	.destroy = cd_destroy,
	.update = cd_update,
	.get_defaults = cd_defaults,
	.get_properties = cd_properties,
	.get_width = cd_width,
	.get_height = cd_height,
	.video_tick = cd_tick,
	.video_render = cd_render,
	.show = cd_show,
	.enum_active_sources = cd_enum,
	.icon_type = OBS_ICON_TYPE_TEXT,
};
