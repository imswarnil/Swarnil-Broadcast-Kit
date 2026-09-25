/*  SBK Visualizer — bars, mirrored bars, a waveform or a dot matrix, from
    any audio source OBS carries. Listens to the Mic/Aux channel by default and
    paints a demo signal when there is nothing to hear, so it is never dead.  */

#include "sbk-common.h"
#include "sbk-anim.h"
#include "sbk-audio.h"

struct viz {
	obs_source_t *self;
	struct sbk_anim anim;
	gs_effect_t *fx;
	gs_texture_t *tex;
	struct sbk_audio audio;
	float upload[SBK_MAX_BANDS];
	float mode, gap, radius, line_width, rest, cell, inner, fill_alpha;
	bool peaks, mono;
	struct vec4 accent;
	float scale;
	uint32_t cx, cy;
};

static const char *viz_name(void *u)
{
	UNUSED_PARAMETER(u);
	return obs_module_text("SBK.Visualizer");
}

/* matches the switch in viz.effect */
static float mode_from(const char *id)
{
	if (astrcmpi(id, "mirror") == 0) return 1.0f;
	if (astrcmpi(id, "wave") == 0) return 2.0f;
	if (astrcmpi(id, "dots") == 0) return 3.0f;
	if (astrcmpi(id, "ring") == 0) return 4.0f;
	if (astrcmpi(id, "blocks") == 0) return 5.0f;
	if (astrcmpi(id, "line") == 0) return 6.0f;
	return 0.0f;
}

static void viz_update(void *data, obs_data_t *s)
{
	struct viz *v = data;
	sbk_anim_read(&v->anim, s, 0.0f);
	sbk_audio_set_source(&v->audio, obs_data_get_string(s, "source"));
	v->mode = mode_from(obs_data_get_string(s, "style"));
	v->audio.band_count = (int)obs_data_get_int(s, "bars");
	v->cx = (uint32_t)obs_data_get_int(s, "width");
	v->cy = (uint32_t)obs_data_get_int(s, "height");
	v->gap = (float)obs_data_get_double(s, "gap");
	v->radius = (float)obs_data_get_double(s, "radius");
	v->line_width = (float)obs_data_get_double(s, "line_width");
	v->rest = (float)obs_data_get_double(s, "rest");
	v->cell = (float)obs_data_get_double(s, "cell");
	v->inner = (float)obs_data_get_double(s, "inner");
	v->fill_alpha = (float)obs_data_get_double(s, "fill_alpha");
	v->peaks = obs_data_get_bool(s, "peaks");
	v->mono = obs_data_get_bool(s, "mono");
	v->accent = sbk_vec((uint32_t)obs_data_get_int(s, "accent"));
	v->scale = (float)obs_data_get_double(s, "scale");
	v->audio.gain = (float)obs_data_get_double(s, "gain");
	v->audio.floor_db = (float)obs_data_get_double(s, "floor_db");
	v->audio.smoothing = (float)obs_data_get_double(s, "smoothing");
	v->audio.decay = (float)obs_data_get_double(s, "decay");
}

static void *viz_create(obs_data_t *s, obs_source_t *source)
{
	struct viz *v = bzalloc(sizeof(*v));
	v->self = source;
	v->fx = sbk_load_effect("effects/viz.effect");
	sbk_audio_init(&v->audio);
	viz_update(v, s);
	sbk_anim_play(&v->anim);
	return v;
}

static void viz_destroy(void *data)
{
	struct viz *v = data;
	sbk_audio_free(&v->audio);
	obs_enter_graphics();
	if (v->tex)
		gs_texture_destroy(v->tex);
	if (v->fx)
		gs_effect_destroy(v->fx);
	obs_leave_graphics();
	bfree(v);
}

static void viz_tick(void *data, float seconds)
{
	struct viz *v = data;
	sbk_anim_tick(&v->anim, seconds);
	sbk_audio_tick(&v->audio, seconds);
	int n = v->audio.band_count;
	if (v->mode == 2.0f) {
		sbk_audio_waveform(&v->audio, v->upload, n, v->audio.gain);
		return;
	}
	for (int b = 0; b < n; b++)
		v->upload[b] = v->peaks ? fmaxf(v->audio.bands[b], v->audio.peaks[b]) : v->audio.bands[b];
}

static uint32_t viz_width(void *d) { return ((struct viz *)d)->cx; }
static uint32_t viz_height(void *d) { return ((struct viz *)d)->cy; }

static void viz_render(void *data, gs_effect_t *unused)
{
	UNUSED_PARAMETER(unused);
	struct viz *v = data;
	if (!v->fx)
		return;
	int n = v->audio.band_count;
	if (!v->tex || (int)gs_texture_get_width(v->tex) != n) {
		if (v->tex)
			gs_texture_destroy(v->tex);
		v->tex = gs_texture_create((uint32_t)n, 1, GS_R32F, 1, NULL, GS_DYNAMIC);
	}
	if (!v->tex)
		return;
	gs_texture_set_image(v->tex, (const uint8_t *)v->upload, (uint32_t)n * sizeof(float), false);

	struct sbk_anim_out a = sbk_anim_eval(&v->anim);
	struct vec4 hi = v->mono ? sbk_vec(SBK_INK) : v->accent;
	struct vec4 lo = sbk_alpha(hi, 0.55f);
	hi = sbk_alpha(hi, a.alpha);
	lo = sbk_alpha(lo, a.alpha);

	gs_eparam_t *p = gs_effect_get_param_by_name(v->fx, "bands");
	if (p)
		gs_effect_set_texture(p, v->tex);
	sbk_set_vec4(v->fx, "color_lo", &lo);
	sbk_set_vec4(v->fx, "color_hi", &hi);
	sbk_set_vec2(v->fx, "size", (float)v->cx, (float)v->cy);
	sbk_set_float(v->fx, "band_count", (float)n);
	sbk_set_float(v->fx, "mode", v->mode);
	sbk_set_float(v->fx, "gap", v->gap);
	sbk_set_float(v->fx, "radius", v->radius * v->scale);
	sbk_set_float(v->fx, "line_width", v->line_width * v->scale);
	sbk_set_float(v->fx, "floor_level", v->rest);
	sbk_set_float(v->fx, "cell", fmaxf(4.0f, v->cell * v->scale));
	sbk_set_float(v->fx, "inner", v->inner);
	sbk_set_float(v->fx, "fill_alpha", v->fill_alpha);

	gs_blend_state_push();
	gs_blend_function(GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA);
	while (gs_effect_loop(v->fx, "Draw"))
		gs_draw_sprite(NULL, 0, v->cx, v->cy);
	gs_blend_state_pop();
}

static void viz_show(void *d) { sbk_anim_on_show(&((struct viz *)d)->anim); }

static obs_properties_t *viz_properties(void *data)
{
	UNUSED_PARAMETER(data);
	obs_properties_t *p = obs_properties_create();
	obs_property_t *src = obs_properties_add_list(p, "source", "Listen to", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	sbk_audio_fill_source_list(src);
	obs_property_t *st = obs_properties_add_list(p, "style", "Style", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(st, "Bars", "bars");
	obs_property_list_add_string(st, "Bars, mirrored", "mirror");
	obs_property_list_add_string(st, "Waveform", "wave");
	obs_property_list_add_string(st, "Dot matrix", "dots");
	obs_property_list_add_string(st, "Ring", "ring");
	obs_property_list_add_string(st, "Blocks — a segment ladder", "blocks");
	obs_property_list_add_string(st, "Line — filled under the curve", "line");
	obs_properties_add_int(p, "width", "Width", 32, 7680, 2);
	obs_properties_add_int(p, "height", "Height", 16, 4320, 2);
	obs_properties_add_int_slider(p, "bars", "Bars", 8, SBK_MAX_BANDS, 1);
	obs_properties_add_float_slider(p, "gap", "Gap between bars", 0.0, 0.9, 0.01);
	obs_properties_add_float_slider(p, "radius", "Bar cap radius", 0.0, 40.0, 1.0);
	obs_properties_add_float_slider(p, "line_width", "Waveform thickness", 1.0, 24.0, 0.5);
	obs_properties_add_float_slider(p, "cell", "Dot and block pitch", 6.0, 60.0, 1.0);
	obs_properties_add_float_slider(p, "inner", "Ring hole", 0.05, 0.9, 0.01);
	obs_properties_add_float_slider(p, "fill_alpha", "Line fill", 0.0, 1.0, 0.02);
	obs_properties_add_bool(p, "peaks", "Hold peaks");
	obs_properties_add_float_slider(p, "rest", "Resting level", 0.0, 0.2, 0.005);

	obs_properties_t *a = obs_properties_create();
	obs_properties_add_float_slider(a, "gain", "Gain", 0.1, 20.0, 0.1);
	obs_properties_add_float_slider(a, "floor_db", "Floor (dB)", -90.0, -20.0, 1.0);
	obs_properties_add_float_slider(a, "smoothing", "Fall smoothing", 0.0, 0.98, 0.01);
	obs_properties_add_float_slider(a, "decay", "Peak fall", 0.001, 0.1, 0.001);
	obs_properties_add_group(p, "audio", "Audio", OBS_GROUP_NORMAL, a);

	obs_properties_t *g = obs_properties_create();
	obs_properties_add_color(g, "accent", "Accent");
	obs_properties_add_bool(g, "mono", "White instead of the accent");
	obs_properties_add_float_slider(g, "scale", "Scale", 0.5, 3.0, 0.05);
	obs_properties_add_group(p, "look", "Look", OBS_GROUP_NORMAL, g);
	sbk_anim_props(p);
	return p;
}

static void viz_defaults(obs_data_t *s)
{
	obs_data_set_default_string(s, "source", "@program");
	obs_data_set_default_string(s, "style", "bars");
	obs_data_set_default_int(s, "width", 1920);
	obs_data_set_default_int(s, "height", 240);
	obs_data_set_default_int(s, "bars", 48);
	obs_data_set_default_double(s, "gap", 0.35);
	obs_data_set_default_double(s, "radius", 4.0);
	obs_data_set_default_double(s, "line_width", 3.0);
	obs_data_set_default_double(s, "cell", 16.0);
	obs_data_set_default_double(s, "inner", 0.45);
	obs_data_set_default_double(s, "fill_alpha", 0.28);
	obs_data_set_default_bool(s, "peaks", false);
	obs_data_set_default_double(s, "rest", 0.045);
	obs_data_set_default_double(s, "gain", 1.0);
	obs_data_set_default_double(s, "floor_db", -60.0);
	obs_data_set_default_double(s, "smoothing", 0.8);
	obs_data_set_default_double(s, "decay", 0.012);
	obs_data_set_default_int(s, "accent", (long long)SBK_ACCENT);
	obs_data_set_default_bool(s, "mono", false);
	obs_data_set_default_double(s, "scale", 1.0);
	sbk_anim_defaults(s, "fade");
}

struct obs_source_info sbk_visualizer_info = {
	.id = "sbk_visualizer",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
	.get_name = viz_name,
	.create = viz_create,
	.destroy = viz_destroy,
	.update = viz_update,
	.get_defaults = viz_defaults,
	.get_properties = viz_properties,
	.get_width = viz_width,
	.get_height = viz_height,
	.video_tick = viz_tick,
	.video_render = viz_render,
	.show = viz_show,
	.icon_type = OBS_ICON_TYPE_AUDIO_OUTPUT,
};
