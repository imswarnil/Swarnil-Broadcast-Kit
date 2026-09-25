/*  SBK Scanlines — a CRT treatment for any source.

    Scanlines, an aperture mask, the colour fringing a badly converged tube had,
    barrel curvature, a vignette, grain and mains flicker. Every part dials to
    zero on its own, because the whole difference between a tasteful hint of a
    monitor and something unwatchable is the amounts. The defaults are fine
    scanlines and nothing else.

    Three presets cover what people actually reach for, because nobody wants to
    find "retro handheld" by moving eight sliders.  */

#include "sbk-common.h"

struct scan_f {
	obs_source_t *self;
	gs_effect_t *fx;
	float t;
	float line_height, line_depth, line_roll;
	float mask, mask_width, fringe, curve, vignette, grain, flicker, bright;
	uint32_t cx, cy;
};

static const char *scan_name(void *u)
{
	UNUSED_PARAMETER(u);
	return obs_module_text("SBK.Scanlines");
}

static void scan_update(void *data, obs_data_t *s)
{
	struct scan_f *f = data;
	f->line_height = (float)obs_data_get_double(s, "line_height");
	f->line_depth = (float)obs_data_get_double(s, "line_depth");
	f->line_roll = (float)obs_data_get_double(s, "line_roll");
	f->mask = (float)obs_data_get_double(s, "mask");
	f->mask_width = (float)obs_data_get_double(s, "mask_width");
	f->fringe = (float)obs_data_get_double(s, "fringe");
	f->curve = (float)obs_data_get_double(s, "curve");
	f->vignette = (float)obs_data_get_double(s, "vignette");
	f->grain = (float)obs_data_get_double(s, "grain");
	f->flicker = (float)obs_data_get_double(s, "flicker");
	f->bright = (float)obs_data_get_double(s, "bright");
}

static void *scan_create(obs_data_t *s, obs_source_t *source)
{
	struct scan_f *f = bzalloc(sizeof(*f));
	f->self = source;
	f->fx = sbk_load_effect("effects/scanlines.effect");
	scan_update(f, s);
	return f;
}

static void scan_destroy(void *data)
{
	struct scan_f *f = data;
	sbk_free_effect(&f->fx);
	bfree(f);
}

static void scan_tick(void *data, float seconds)
{
	struct scan_f *f = data;
	f->t += seconds;
	if (f->t > 10000.0f) /* keep the trig arguments small enough to stay precise */
		f->t -= 10000.0f;
}

static void scan_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);
	struct scan_f *f = data;
	obs_source_t *target = obs_filter_get_target(f->self);
	if (!f->fx || !target) {
		obs_source_skip_video_filter(f->self);
		return;
	}
	f->cx = obs_source_get_base_width(target);
	f->cy = obs_source_get_base_height(target);
	if (!f->cx || !f->cy) {
		obs_source_skip_video_filter(f->self);
		return;
	}
	if (!obs_source_process_filter_begin(f->self, GS_RGBA, OBS_ALLOW_DIRECT_RENDERING))
		return;

	sbk_set_vec2(f->fx, "size", (float)f->cx, (float)f->cy);
	sbk_set_float(f->fx, "time", f->t);
	sbk_set_float(f->fx, "line_height", f->line_height);
	sbk_set_float(f->fx, "line_depth", f->line_depth);
	sbk_set_float(f->fx, "line_roll", f->line_roll);
	sbk_set_float(f->fx, "mask", f->mask);
	sbk_set_float(f->fx, "mask_width", f->mask_width);
	sbk_set_float(f->fx, "fringe", f->fringe);
	sbk_set_float(f->fx, "curve", f->curve);
	sbk_set_float(f->fx, "vignette", f->vignette);
	sbk_set_float(f->fx, "grain", f->grain);
	sbk_set_float(f->fx, "flicker", f->flicker);
	sbk_set_float(f->fx, "bright", f->bright);

	obs_source_process_filter_end(f->self, f->fx, f->cx, f->cy);
}

/* ---- presets --------------------------------------------------------------- */

struct preset {
	const char *id, *label;
	float line_height, line_depth, line_roll, mask, mask_width, fringe, curve, vignette, grain, flicker, bright;
};

static const struct preset PRESETS[] = {
	{"fine", "Fine — a hint of a monitor", 2.0f, 0.18f, 0.0f, 0.0f, 3.0f, 0.0f, 0.0f, 0.10f, 0.0f, 0.0f, 0.06f},
	{"crt", "CRT — a broadcast tube", 3.0f, 0.34f, 0.0f, 0.22f, 3.0f, 1.2f, 0.06f, 0.30f, 0.04f, 0.15f, 0.22f},
	{"vhs", "VHS — a worn tape", 2.0f, 0.22f, 26.0f, 0.10f, 4.0f, 3.5f, 0.03f, 0.34f, 0.16f, 0.35f, 0.16f},
	{"arcade", "Arcade — a coarse grille", 5.0f, 0.42f, 0.0f, 0.45f, 6.0f, 0.6f, 0.10f, 0.26f, 0.02f, 0.05f, 0.30f},
};
#define N_PRESETS (sizeof(PRESETS) / sizeof(PRESETS[0]))

static bool on_preset(obs_properties_t *props, obs_property_t *p, obs_data_t *s)
{
	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(p);
	const char *want = obs_data_get_string(s, "preset");
	if (!want || !*want || astrcmpi(want, "custom") == 0)
		return false;
	for (size_t i = 0; i < N_PRESETS; i++) {
		if (astrcmpi(PRESETS[i].id, want) != 0)
			continue;
		const struct preset *v = &PRESETS[i];
		obs_data_set_double(s, "line_height", v->line_height);
		obs_data_set_double(s, "line_depth", v->line_depth);
		obs_data_set_double(s, "line_roll", v->line_roll);
		obs_data_set_double(s, "mask", v->mask);
		obs_data_set_double(s, "mask_width", v->mask_width);
		obs_data_set_double(s, "fringe", v->fringe);
		obs_data_set_double(s, "curve", v->curve);
		obs_data_set_double(s, "vignette", v->vignette);
		obs_data_set_double(s, "grain", v->grain);
		obs_data_set_double(s, "flicker", v->flicker);
		obs_data_set_double(s, "bright", v->bright);
		/* back to Custom, so the next nudge of a slider is not undone the next
		   time this callback runs */
		obs_data_set_string(s, "preset", "custom");
		return true;
	}
	return false;
}

static obs_properties_t *scan_properties(void *data)
{
	UNUSED_PARAMETER(data);
	obs_properties_t *p = obs_properties_create();
	obs_property_t *pr = obs_properties_add_list(p, "preset", "Start from", OBS_COMBO_TYPE_LIST,
						     OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(pr, "Custom", "custom");
	for (size_t i = 0; i < N_PRESETS; i++)
		obs_property_list_add_string(pr, PRESETS[i].label, PRESETS[i].id);
	obs_property_set_modified_callback(pr, on_preset);

	obs_properties_t *l = obs_properties_create();
	obs_properties_add_float_slider(l, "line_height", "Line spacing (px)", 1.0, 16.0, 0.5);
	obs_properties_add_float_slider(l, "line_depth", "Line depth", 0.0, 0.9, 0.01);
	obs_properties_add_float_slider(l, "line_roll", "Roll (px/s)", -120.0, 120.0, 1.0);
	obs_properties_add_float_slider(l, "bright", "Brightness back", 0.0, 1.0, 0.01);
	obs_properties_add_group(p, "lines", "Scanlines", OBS_GROUP_NORMAL, l);

	obs_properties_t *t = obs_properties_create();
	obs_properties_add_float_slider(t, "mask", "Aperture mask", 0.0, 1.0, 0.01);
	obs_properties_add_float_slider(t, "mask_width", "Triad width (px)", 2.0, 12.0, 1.0);
	obs_properties_add_float_slider(t, "fringe", "Colour fringing", 0.0, 12.0, 0.1);
	obs_properties_add_float_slider(t, "curve", "Tube curvature", 0.0, 0.35, 0.005);
	obs_properties_add_group(p, "tube", "Tube", OBS_GROUP_NORMAL, t);

	obs_properties_t *g = obs_properties_create();
	obs_properties_add_float_slider(g, "vignette", "Vignette", 0.0, 1.0, 0.01);
	obs_properties_add_float_slider(g, "grain", "Grain", 0.0, 1.0, 0.01);
	obs_properties_add_float_slider(g, "flicker", "Flicker", 0.0, 1.0, 0.01);
	obs_properties_add_group(p, "wear", "Wear", OBS_GROUP_NORMAL, g);

	obs_properties_add_text(p, "hint",
				"Add it under Filters on a source, or on a whole scene to treat everything at once. "
				"Curvature crops to the tube, so leave a little room around what matters.",
				OBS_TEXT_INFO);
	return p;
}

static void scan_defaults(obs_data_t *s)
{
	const struct preset *v = &PRESETS[0];
	obs_data_set_default_string(s, "preset", "custom");
	obs_data_set_default_double(s, "line_height", v->line_height);
	obs_data_set_default_double(s, "line_depth", v->line_depth);
	obs_data_set_default_double(s, "line_roll", v->line_roll);
	obs_data_set_default_double(s, "mask", v->mask);
	obs_data_set_default_double(s, "mask_width", v->mask_width);
	obs_data_set_default_double(s, "fringe", v->fringe);
	obs_data_set_default_double(s, "curve", v->curve);
	obs_data_set_default_double(s, "vignette", v->vignette);
	obs_data_set_default_double(s, "grain", v->grain);
	obs_data_set_default_double(s, "flicker", v->flicker);
	obs_data_set_default_double(s, "bright", v->bright);
}

struct obs_source_info sbk_scanlines_info = {
	.id = "sbk_scanlines",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_VIDEO,
	.get_name = scan_name,
	.create = scan_create,
	.destroy = scan_destroy,
	.update = scan_update,
	.get_defaults = scan_defaults,
	.get_properties = scan_properties,
	.video_tick = scan_tick,
	.video_render = scan_render,
};
