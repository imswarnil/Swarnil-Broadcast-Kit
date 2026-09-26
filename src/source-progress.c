/*  SBK Progress — a goal: subscribers, a fundraiser, chapter 3 of 8.

    The number is the point, so it is set in the mono face and the bar eases
    toward its new value rather than jumping — a counter that snaps reads as a
    glitch, and a stream has time for half a second of movement. Two hotkeys
    nudge the value without opening the dialog, which is what you actually need
    while live.  */

#include "sbk-common.h"
#include "sbk-text.h"
#include "sbk-stage.h"
#include "sbk-anim.h"

enum bar_style { BAR_SOLID = 0, BAR_SEGMENTS, BAR_LINE };

struct progress {
	obs_source_t *self;
	struct sbk_look look;
	struct sbk_anim anim;
	struct sbk_stage stage;
	gs_effect_t *card_fx;
	struct sbk_text label, figures;

	char *s_label;
	double value, target;
	enum bar_style style;
	enum sbk_surface surf;
	bool show_figures, as_percent;
	int segments;
	double step;
	float shown; /* what the bar is actually drawing, easing toward value */
	uint32_t width, cx, cy;
	obs_hotkey_id up_id, down_id;
	float label_y, bar_y, bar_h;
};

static const char *prog_name(void *u)
{
	UNUSED_PARAMETER(u);
	return obs_module_text("SBK.Progress");
}

static void nudge(struct progress *g, double by)
{
	g->value += by;
	if (g->value < 0.0)
		g->value = 0.0;
	obs_data_t *s = obs_source_get_settings(g->self);
	obs_data_set_double(s, "value", g->value);
	obs_data_release(s);
}

static void on_up(void *data, obs_hotkey_id id, obs_hotkey_t *hk, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hk);
	struct progress *g = data;
	if (pressed)
		nudge(g, g->step);
}
static void on_down(void *data, obs_hotkey_id id, obs_hotkey_t *hk, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hk);
	struct progress *g = data;
	if (pressed)
		nudge(g, -g->step);
}

static void prog_update(void *data, obs_data_t *s)
{
	struct progress *g = data;
	sbk_look_read(&g->look, s);
	sbk_anim_read(&g->anim, s, 4.0f * sbk_u(&g->look));
	bfree(g->s_label);
	g->s_label = bstrdup(obs_data_get_string(s, "label"));
	g->value = obs_data_get_double(s, "value");
	g->target = obs_data_get_double(s, "target");
	if (g->target <= 0.0)
		g->target = 1.0;
	const char *st = obs_data_get_string(s, "style");
	g->style = astrcmpi(st, "segments") == 0 ? BAR_SEGMENTS : astrcmpi(st, "line") == 0 ? BAR_LINE : BAR_SOLID;
	g->surf = sbk_surface_from(obs_data_get_string(s, "variant"));
	g->show_figures = obs_data_get_bool(s, "show_figures");
	g->as_percent = obs_data_get_bool(s, "as_percent");
	g->segments = (int)obs_data_get_int(s, "segments");
	g->step = obs_data_get_double(s, "step");
	g->width = (uint32_t)obs_data_get_int(s, "width");
}

static void *prog_create(obs_data_t *s, obs_source_t *source)
{
	struct progress *g = bzalloc(sizeof(*g));
	g->self = source;
	g->card_fx = sbk_load_effect("effects/card.effect");
	sbk_stage_init(&g->stage);
	g->up_id = obs_hotkey_register_source(source, "SBK.Progress.Up", "Broadcast Kit: nudge the goal up", on_up, g);
	g->down_id = obs_hotkey_register_source(source, "SBK.Progress.Down", "Broadcast Kit: nudge the goal down", on_down, g);
	prog_update(g, s);
	g->shown = (float)(g->value / g->target);
	sbk_anim_play(&g->anim);
	return g;
}

static void prog_destroy(void *data)
{
	struct progress *g = data;
	if (g->up_id != OBS_INVALID_HOTKEY_ID)
		obs_hotkey_unregister(g->up_id);
	if (g->down_id != OBS_INVALID_HOTKEY_ID)
		obs_hotkey_unregister(g->down_id);
	sbk_text_free(&g->label);
	sbk_text_free(&g->figures);
	sbk_stage_free(&g->stage);
	sbk_free_effect(&g->card_fx);
	bfree(g->s_label);
	bfree(g);
}

/* 12400 → "12.4k", so a goal fits the bar at any size */
static void human(char *buf, size_t n, double v)
{
	double a = fabs(v);
	if (a >= 1000000.0)
		snprintf(buf, n, "%.1fM", v / 1000000.0);
	else if (a >= 1000.0)
		snprintf(buf, n, "%.1fk", v / 1000.0);
	else
		snprintf(buf, n, "%.0f", v);
}

static void prog_tick(void *data, float seconds)
{
	struct progress *g = data;
	sbk_anim_tick(&g->anim, seconds);
	const struct sbk_look *l = &g->look;
	const float u = sbk_u(l);
	const bool bare = g->surf == SBK_SURF_NONE;

	float want = (float)sbk_clampf((float)(g->value / g->target), 0.0f, 1.0f);
	/* ease toward the target: 6 per second is quick enough to feel live and
	   slow enough to read as movement */
	g->shown += (want - g->shown) * sbk_clampf(seconds * 6.0f, 0.0f, 1.0f);

	char figs[64] = {0};
	if (g->show_figures) {
		if (g->as_percent) {
			snprintf(figs, sizeof(figs), "%.0f%%", g->shown * 100.0f);
		} else {
			char a[32], b[32];
			human(a, sizeof(a), g->value);
			human(b, sizeof(b), g->target);
			snprintf(figs, sizeof(figs), "%s / %s", a, b);
		}
	}

	struct dstr lab = {0};
	dstr_copy(&lab, g->s_label ? g->s_label : "");
	sbk_caps(&lab);
	sbk_text_set_full(&g->label, lab.array ? lab.array : "", l->face, "SemiBold", (int)(4.5f * u),
			    sbk_surface_ink(g->surf, l, true), 0, bare);
	dstr_free(&lab);
	sbk_text_set_full(&g->figures, figs, l->mono, "Bold", (int)(5.5f * u), sbk_surface_ink(g->surf, l, false), 0,
			    bare);

	float pad_y = bare ? 0.0f : 5.0f * u;
	float head = fmaxf((float)sbk_text_h(&g->label), (float)sbk_text_h(&g->figures));
	g->bar_h = g->style == BAR_LINE ? 1.5f * u : 3.0f * u;
	g->label_y = pad_y;
	g->bar_y = pad_y + (head > 0.0f ? head + 3.0f * u : 0.0f);
	g->cx = g->width;
	g->cy = (uint32_t)(g->bar_y + g->bar_h + pad_y + 0.5f);
}

static uint32_t prog_width(void *d) { return ((struct progress *)d)->cx; }
static uint32_t prog_height(void *d) { return ((struct progress *)d)->cy; }

static void prog_render(void *data, gs_effect_t *unused)
{
	UNUSED_PARAMETER(unused);
	struct progress *g = data;
	if (!g->card_fx || !sbk_stage_begin(&g->stage, g->cx, g->cy))
		return;
	const struct sbk_look *l = &g->look;
	const float u = sbk_u(l);
	const bool bare = g->surf == SBK_SURF_NONE;
	float pad_x = bare ? 0.0f : 6.0f * u;

	sbk_surface_draw(g->card_fx, g->surf, l, 0, 0, (float)g->cx, (float)g->cy, 3.0f * u);

	sbk_text_draw(&g->label, pad_x, g->label_y);
	if (g->figures.text.len)
		sbk_text_draw(&g->figures, (float)g->cx - pad_x - (float)sbk_text_w(&g->figures), g->label_y);

	float track_w = (float)g->cx - pad_x * 2.0f;
	float r = g->bar_h * 0.5f;
	struct vec4 track = g->surf == SBK_SURF_ACCENT ? sbk_alpha(l->on_accent, 0.25f) : l->wash;

	if (g->style == BAR_SEGMENTS) {
		int n = g->segments < 2 ? 2 : g->segments;
		float gap = 1.5f * u;
		float seg = (track_w - gap * (float)(n - 1)) / (float)n;
		float lit = g->shown * (float)n;
		for (int i = 0; i < n; i++) {
			float x = pad_x + (float)i * (seg + gap);
			float fillness = sbk_clampf(lit - (float)i, 0.0f, 1.0f);
			sbk_fill(g->card_fx, x, g->bar_y, seg, g->bar_h, r, track);
			if (fillness > 0.01f)
				sbk_fill(g->card_fx, x, g->bar_y, seg * fillness, g->bar_h, r, l->accent);
		}
	} else {
		sbk_fill(g->card_fx, pad_x, g->bar_y, track_w, g->bar_h, r, track);
		float w = track_w * g->shown;
		if (w > 0.5f) {
			struct vec4 none = SBK_NONE;
			sbk_card_ex(g->card_fx, pad_x, g->bar_y, w, g->bar_h, r, l->accent,
				      sbk_alpha(l->accent, 0.65f), 3.0f, none, 0.0f,
				      sbk_alpha(l->accent, 0.35f), 2.0f * u);
		}
	}

	sbk_stage_end(&g->stage);
	sbk_stage_present_anim(&g->stage, sbk_anim_eval(&g->anim));
}

static void prog_show(void *d) { sbk_anim_on_show(&((struct progress *)d)->anim); }
static void prog_enum(void *d, obs_source_enum_proc_t cb, void *p)
{
	struct progress *g = d;
	sbk_text_enum(&g->label, g->self, cb, p);
	sbk_text_enum(&g->figures, g->self, cb, p);
}

static obs_properties_t *prog_properties(void *data)
{
	UNUSED_PARAMETER(data);
	obs_properties_t *p = obs_properties_create();
	obs_properties_add_text(p, "label", "Label", OBS_TEXT_DEFAULT);
	obs_properties_add_float(p, "value", "Value", 0.0, 1000000000.0, 1.0);
	obs_properties_add_float(p, "target", "Target", 1.0, 1000000000.0, 1.0);
	obs_properties_add_float(p, "step", "Hotkey step", 1.0, 100000.0, 1.0);
	obs_properties_add_bool(p, "show_figures", "Show the numbers");
	obs_properties_add_bool(p, "as_percent", "As a percentage instead of value / target");
	obs_property_t *st = obs_properties_add_list(p, "style", "Bar", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(st, "Solid", "solid");
	obs_property_list_add_string(st, "Segments", "segments");
	obs_property_list_add_string(st, "Thin line", "line");
	obs_properties_add_int_slider(p, "segments", "How many segments", 2, 40, 1);
	sbk_surface_list(p, "variant", "Variant");
	obs_properties_add_int(p, "width", "Width", 160, 3840, 2);
	sbk_look_props(p, true);
	sbk_anim_props(p);
	obs_properties_add_text(p, "hint", "Bind \"Broadcast Kit: nudge the goal up / down\" under Settings → Hotkeys to move it while live.",
				OBS_TEXT_INFO);
	return p;
}

static void prog_defaults(obs_data_t *s)
{
	obs_data_set_default_string(s, "label", "Subscriber goal");
	obs_data_set_default_double(s, "value", 640.0);
	obs_data_set_default_double(s, "target", 1000.0);
	obs_data_set_default_double(s, "step", 1.0);
	obs_data_set_default_bool(s, "show_figures", true);
	obs_data_set_default_bool(s, "as_percent", false);
	obs_data_set_default_string(s, "style", "solid");
	obs_data_set_default_int(s, "segments", 10);
	obs_data_set_default_string(s, "variant", "card");
	obs_data_set_default_int(s, "width", 520);
	sbk_look_defaults(s);
	sbk_anim_defaults(s, "fade");
}

struct obs_source_info sbk_progress_info = {
	.id = "sbk_progress",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
	.get_name = prog_name,
	.create = prog_create,
	.destroy = prog_destroy,
	.update = prog_update,
	.get_defaults = prog_defaults,
	.get_properties = prog_properties,
	.get_width = prog_width,
	.get_height = prog_height,
	.video_tick = prog_tick,
	.video_render = prog_render,
	.show = prog_show,
	.enum_active_sources = prog_enum,
	.icon_type = OBS_ICON_TYPE_TEXT,
};
