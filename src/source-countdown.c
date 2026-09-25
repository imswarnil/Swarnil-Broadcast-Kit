/*  SBK Countdown — to a duration ("15 minutes") or a time of day ("21:30").
    Shows a word when it reaches zero. Restarts when shown, and on a hotkey.  */

#include <time.h>

#include "sbk-common.h"
#include "sbk-text.h"
#include "sbk-stage.h"
#include "sbk-anim.h"

struct countdown {
	obs_source_t *self;
	struct sbk_look look;
	struct sbk_anim anim;
	struct sbk_stage stage;
	gs_effect_t *card_fx;
	struct sbk_text figures;

	bool to_time, restart_on_show, draw_card, done_fired;
	int minutes, seconds_v;
	char *at, *done_word;
	uint64_t target_ns;
	obs_hotkey_id restart_id;
	uint32_t cx, cy;
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
	int64_t secs = c->to_time ? secs_until(c->at) : (int64_t)c->minutes * 60 + c->seconds_v;
	if (secs < 0)
		secs = 0;
	c->target_ns = os_gettime_ns() + (uint64_t)secs * 1000000000ULL;
	c->done_fired = false;
}

static void on_restart(void *data, obs_hotkey_id id, obs_hotkey_t *hk, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hk);
	if (pressed)
		restart(data);
}

static void cd_update(void *data, obs_data_t *s)
{
	struct countdown *c = data;
	sbk_look_read(&c->look, s);
	sbk_anim_read(&c->anim, s, 4.0f * sbk_u(&c->look));
	c->to_time = astrcmpi(obs_data_get_string(s, "mode"), "time") == 0;
	c->minutes = (int)obs_data_get_int(s, "minutes");
	c->seconds_v = (int)obs_data_get_int(s, "seconds");
	bfree(c->at);
	bfree(c->done_word);
	c->at = bstrdup(obs_data_get_string(s, "at"));
	c->done_word = bstrdup(obs_data_get_string(s, "done"));
	c->restart_on_show = obs_data_get_bool(s, "restart_on_show");
	c->draw_card = obs_data_get_bool(s, "draw_card");
	restart(c);
}

static void *cd_create(obs_data_t *s, obs_source_t *source)
{
	struct countdown *c = bzalloc(sizeof(*c));
	c->self = source;
	c->card_fx = sbk_load_effect("effects/card.effect");
	sbk_stage_init(&c->stage);
	c->restart_id = obs_hotkey_register_source(source, "SBK.Countdown.Restart", "Broadcast Kit: restart the countdown",
						   on_restart, c);
	cd_update(c, s);
	sbk_anim_play(&c->anim);
	return c;
}

static void cd_destroy(void *data)
{
	struct countdown *c = data;
	if (c->restart_id != OBS_INVALID_HOTKEY_ID)
		obs_hotkey_unregister(c->restart_id);
	sbk_text_free(&c->figures);
	sbk_stage_free(&c->stage);
	sbk_free_effect(&c->card_fx);
	bfree(c->at);
	bfree(c->done_word);
	bfree(c);
}

static void cd_tick(void *data, float seconds)
{
	struct countdown *c = data;
	sbk_anim_tick(&c->anim, seconds);
	const struct sbk_look *l = &c->look;
	const float u = sbk_u(l);

	uint64_t now = os_gettime_ns();
	int64_t left = now >= c->target_ns ? 0 : (int64_t)((c->target_ns - now + 999999999ULL) / 1000000000ULL);
	char buf[64];
	if (left <= 0) {
		snprintf(buf, sizeof(buf), "%s", c->done_word && *c->done_word ? c->done_word : "Now");
		if (!c->done_fired) {
			c->done_fired = true;
			SBK_LOG(LOG_INFO, "countdown '%s' reached zero", obs_source_get_name(c->self));
		}
	} else if (left >= 3600) {
		snprintf(buf, sizeof(buf), "%lld:%02lld:%02lld", (long long)(left / 3600), (long long)((left % 3600) / 60),
			 (long long)(left % 60));
	} else {
		snprintf(buf, sizeof(buf), "%02lld:%02lld", (long long)(left / 60), (long long)(left % 60));
	}
	sbk_text_set(&c->figures, buf, l->mono, "Bold", (int)(24.0f * u), l->ink);

	float pad_x = c->draw_card ? 8.0f * u : 0.0f, pad_y = c->draw_card ? 4.0f * u : 0.0f;
	c->cx = (uint32_t)(pad_x * 2.0f + (float)sbk_text_w(&c->figures) + 0.5f);
	c->cy = (uint32_t)(pad_y * 2.0f + (float)sbk_text_h(&c->figures) + 0.5f);
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
	struct vec4 none = {{{0.0f, 0.0f, 0.0f, 0.0f}}};
	float pad_x = c->draw_card ? 8.0f * u : 0.0f, pad_y = c->draw_card ? 4.0f * u : 0.0f;
	if (c->draw_card)
		sbk_card(c->card_fx, 0, 0, (float)c->cx, (float)c->cy, 5.0f * u, l->glass, l->glass_line, 1.0f, none, 0.0f);
	sbk_text_draw(&c->figures, pad_x, pad_y);
	sbk_stage_end(&c->stage);
	struct sbk_anim_out a = sbk_anim_eval(&c->anim);
	sbk_stage_present(&c->stage, a.alpha, a.dx, a.dy);
}

static void cd_show(void *d)
{
	struct countdown *c = d;
	sbk_anim_on_show(&c->anim);
	if (c->restart_on_show && !c->to_time)
		restart(c);
}
static void cd_enum(void *d, obs_source_enum_proc_t cb, void *p)
{
	struct countdown *c = d;
	sbk_text_enum(&c->figures, c->self, cb, p);
}

static obs_properties_t *cd_properties(void *data)
{
	UNUSED_PARAMETER(data);
	obs_properties_t *p = obs_properties_create();
	obs_property_t *m = obs_properties_add_list(p, "mode", "Count down", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(m, "For a duration", "duration");
	obs_property_list_add_string(m, "To a time of day", "time");
	obs_properties_add_int(p, "minutes", "Minutes", 0, 600, 1);
	obs_properties_add_int(p, "seconds", "Seconds", 0, 59, 1);
	obs_properties_add_text(p, "at", "Time of day (HH:MM, 24-hour)", OBS_TEXT_DEFAULT);
	obs_properties_add_text(p, "done", "Word at zero", OBS_TEXT_DEFAULT);
	obs_properties_add_bool(p, "restart_on_show", "Restart each time the source is shown (duration only)");
	obs_properties_add_bool(p, "draw_card", "Draw a card behind it");
	sbk_look_props(p, true);
	sbk_anim_props(p);
	obs_properties_add_text(p, "hint", "Bind \"Broadcast Kit: restart the countdown\" under Settings → Hotkeys.", OBS_TEXT_INFO);
	return p;
}

static void cd_defaults(obs_data_t *s)
{
	obs_data_set_default_string(s, "mode", "duration");
	obs_data_set_default_int(s, "minutes", 15);
	obs_data_set_default_int(s, "seconds", 0);
	obs_data_set_default_string(s, "at", "21:30");
	obs_data_set_default_string(s, "done", "Now");
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
