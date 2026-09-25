/*  SBK Clock — the time, set in the mono face, in a pill.  */

#include <time.h>

#include "sbk-common.h"
#include "sbk-text.h"
#include "sbk-stage.h"
#include "sbk-anim.h"

struct clock_src {
	obs_source_t *self;
	struct sbk_look look;
	struct sbk_anim anim;
	struct sbk_stage stage;
	gs_effect_t *card_fx;
	struct sbk_text figures, meridiem;
	bool h24, seconds, draw_card;
	uint32_t cx, cy;
};

static const char *clock_name(void *u)
{
	UNUSED_PARAMETER(u);
	return obs_module_text("SBK.Clock");
}

static void clock_update(void *data, obs_data_t *s)
{
	struct clock_src *c = data;
	sbk_look_read(&c->look, s);
	sbk_anim_read(&c->anim, s, 4.0f * sbk_u(&c->look));
	c->h24 = obs_data_get_bool(s, "h24");
	c->seconds = obs_data_get_bool(s, "seconds");
	c->draw_card = obs_data_get_bool(s, "draw_card");
}

static void *clock_create(obs_data_t *s, obs_source_t *source)
{
	struct clock_src *c = bzalloc(sizeof(*c));
	c->self = source;
	c->card_fx = sbk_load_effect("effects/card.effect");
	sbk_stage_init(&c->stage);
	clock_update(c, s);
	sbk_anim_play(&c->anim);
	return c;
}

static void clock_destroy(void *data)
{
	struct clock_src *c = data;
	sbk_text_free(&c->figures);
	sbk_text_free(&c->meridiem);
	sbk_stage_free(&c->stage);
	sbk_free_effect(&c->card_fx);
	bfree(c);
}

static void clock_tick(void *data, float seconds)
{
	struct clock_src *c = data;
	sbk_anim_tick(&c->anim, seconds);
	const struct sbk_look *l = &c->look;
	const float u = sbk_u(l);

	time_t now = time(NULL);
	struct tm tmv;
	localtime_r(&now, &tmv);
	int h = tmv.tm_hour;
	const char *mer = "";
	if (!c->h24) {
		mer = h >= 12 ? "PM" : "AM";
		h = h % 12;
		if (h == 0)
			h = 12;
	}
	char buf[32];
	if (c->seconds)
		snprintf(buf, sizeof(buf), "%02d:%02d:%02d", h, tmv.tm_min, tmv.tm_sec);
	else
		snprintf(buf, sizeof(buf), "%02d:%02d", h, tmv.tm_min);

	sbk_text_set(&c->figures, buf, l->mono, "Medium", (int)(7.0f * u), l->ink);
	sbk_text_set(&c->meridiem, mer, l->face, "Medium", (int)(4.0f * u), l->ink_faint);

	float pad_x = 4.0f * u, pad_y = 2.5f * u;
	float fw = (float)sbk_text_w(&c->figures), fh = (float)sbk_text_h(&c->figures);
	float mw = *mer ? (float)sbk_text_w(&c->meridiem) + 1.5f * u : 0.0f;
	c->cx = (uint32_t)(pad_x * 2.0f + fw + mw + 0.5f);
	c->cy = (uint32_t)(pad_y * 2.0f + fh + 0.5f);
}

static uint32_t clock_width(void *d) { return ((struct clock_src *)d)->cx; }
static uint32_t clock_height(void *d) { return ((struct clock_src *)d)->cy; }

static void clock_render(void *data, gs_effect_t *unused)
{
	UNUSED_PARAMETER(unused);
	struct clock_src *c = data;
	if (!c->card_fx || !sbk_stage_begin(&c->stage, c->cx, c->cy))
		return;
	const struct sbk_look *l = &c->look;
	const float u = sbk_u(l);
	struct vec4 none = {{{0.0f, 0.0f, 0.0f, 0.0f}}};
	if (c->draw_card)
		sbk_card(c->card_fx, 0, 0, (float)c->cx, (float)c->cy, (float)c->cy * 0.5f, l->glass, l->glass_line, 1.0f, none, 0.0f);
	float pad_x = 4.0f * u, pad_y = 2.5f * u;
	sbk_text_draw(&c->figures, pad_x, pad_y);
	if (c->meridiem.text.len) {
		float fh = (float)sbk_text_h(&c->figures), mh = (float)sbk_text_h(&c->meridiem);
		sbk_text_draw(&c->meridiem, pad_x + (float)sbk_text_w(&c->figures) + 1.5f * u, pad_y + fh - mh - 0.5f * u);
	}
	sbk_stage_end(&c->stage);
	struct sbk_anim_out a = sbk_anim_eval(&c->anim);
	sbk_stage_present(&c->stage, a.alpha, a.dx, a.dy);
}

static void clock_show(void *d) { sbk_anim_on_show(&((struct clock_src *)d)->anim); }
static void clock_enum(void *d, obs_source_enum_proc_t cb, void *p)
{
	struct clock_src *c = d;
	sbk_text_enum(&c->figures, c->self, cb, p);
	sbk_text_enum(&c->meridiem, c->self, cb, p);
}

static obs_properties_t *clock_properties(void *data)
{
	UNUSED_PARAMETER(data);
	obs_properties_t *p = obs_properties_create();
	obs_properties_add_bool(p, "h24", "24-hour");
	obs_properties_add_bool(p, "seconds", "Show seconds");
	obs_properties_add_bool(p, "draw_card", "Draw the pill behind it");
	sbk_look_props(p, true);
	sbk_anim_props(p);
	return p;
}

static void clock_defaults(obs_data_t *s)
{
	obs_data_set_default_bool(s, "h24", false);
	obs_data_set_default_bool(s, "seconds", false);
	obs_data_set_default_bool(s, "draw_card", true);
	sbk_look_defaults(s);
	sbk_anim_defaults(s, "fade");
}

struct obs_source_info sbk_clock_info = {
	.id = "sbk_clock",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
	.get_name = clock_name,
	.create = clock_create,
	.destroy = clock_destroy,
	.update = clock_update,
	.get_defaults = clock_defaults,
	.get_properties = clock_properties,
	.get_width = clock_width,
	.get_height = clock_height,
	.video_tick = clock_tick,
	.video_render = clock_render,
	.show = clock_show,
	.enum_active_sources = clock_enum,
	.icon_type = OBS_ICON_TYPE_TEXT,
};
