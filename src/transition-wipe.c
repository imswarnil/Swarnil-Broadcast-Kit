/*  SBK Wipe — a real transition type, in OBS's own Scene Transitions panel
    beside Fade and Cut. This is the piece a browser source can never be: a
    transition is composited by OBS between two scene textures, so nothing
    running inside a page could ever see both.

    Five styles, all from one shader. The bar and the iris carry the accent on
    their leading edge, which is what ties a cut to the rest of the overlay
    set — the same red that is on the tally light sweeps the frame.  */

#include "sbk-common.h"

struct wipe {
	obs_source_t *self;
	gs_effect_t *fx;
	float style, bar_width, softness, bars, dir;
	struct vec4 accent;
};

static const char *wipe_name(void *u)
{
	UNUSED_PARAMETER(u);
	return obs_module_text("SBK.Wipe");
}

static void wipe_update(void *data, obs_data_t *s)
{
	struct wipe *w = data;
	const char *st = obs_data_get_string(s, "style");
	w->style = astrcmpi(st, "dip") == 0      ? 1.0f
		   : astrcmpi(st, "slide") == 0  ? 2.0f
		   : astrcmpi(st, "iris") == 0   ? 3.0f
		   : astrcmpi(st, "blinds") == 0 ? 4.0f
		   : astrcmpi(st, "push") == 0   ? 5.0f
		   : astrcmpi(st, "bars") == 0   ? 6.0f
						 : 0.0f;
	const char *d = obs_data_get_string(s, "direction");
	w->dir = astrcmpi(d, "right-left") == 0 ? 1.0f : astrcmpi(d, "top-bottom") == 0 ? 2.0f
		 : astrcmpi(d, "bottom-top") == 0 ? 3.0f : 0.0f;
	w->accent = sbk_vec((uint32_t)obs_data_get_int(s, "accent"));
	w->bar_width = (float)obs_data_get_double(s, "bar_width");
	w->softness = (float)obs_data_get_double(s, "softness");
	w->bars = (float)obs_data_get_int(s, "bars");
}

static void *wipe_create(obs_data_t *s, obs_source_t *source)
{
	struct wipe *w = bzalloc(sizeof(*w));
	w->self = source;
	w->fx = sbk_load_effect("effects/wipe.effect");
	wipe_update(w, s);
	return w;
}

static void wipe_destroy(void *data)
{
	struct wipe *w = data;
	sbk_free_effect(&w->fx);
	bfree(w);
}

static void wipe_callback(void *data, gs_texture_t *a, gs_texture_t *b, float t, uint32_t cx, uint32_t cy)
{
	struct wipe *w = data;
	if (!w->fx)
		return;
	gs_eparam_t *pa = gs_effect_get_param_by_name(w->fx, "tex_a");
	gs_eparam_t *pb = gs_effect_get_param_by_name(w->fx, "tex_b");
	if (pa)
		gs_effect_set_texture(pa, a);
	if (pb)
		gs_effect_set_texture(pb, b);
	sbk_set_float(w->fx, "t", t);
	sbk_set_float(w->fx, "style", w->style);
	sbk_set_float(w->fx, "dir", w->dir);
	sbk_set_vec4(w->fx, "accent", &w->accent);
	sbk_set_float(w->fx, "bar_width", w->bar_width);
	sbk_set_float(w->fx, "softness", w->softness);
	sbk_set_float(w->fx, "bars", w->bars);
	while (gs_effect_loop(w->fx, "Draw"))
		gs_draw_sprite(NULL, 0, cx, cy);
}

static void wipe_video_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);
	struct wipe *w = data;
	obs_transition_video_render(w->self, wipe_callback);
}

/* a straight crossfade on the audio: the picture is where the character goes,
   and an audio wipe would only ever sound like a mistake */
static float mix_a(void *data, float t)
{
	UNUSED_PARAMETER(data);
	return 1.0f - t;
}
static float mix_b(void *data, float t)
{
	UNUSED_PARAMETER(data);
	return t;
}

static bool wipe_audio_render(void *data, uint64_t *ts_out, struct obs_source_audio_mix *audio, uint32_t mixers,
			      size_t channels, size_t sample_rate)
{
	struct wipe *w = data;
	return obs_transition_audio_render(w->self, ts_out, audio, mixers, channels, sample_rate, mix_a, mix_b);
}

static obs_properties_t *wipe_properties(void *data)
{
	UNUSED_PARAMETER(data);
	obs_properties_t *p = obs_properties_create();
	obs_property_t *st = obs_properties_add_list(p, "style", "Style", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(st, "Bar — an accent edge sweeps across", "bar");
	obs_property_list_add_string(st, "Dip — through the accent and out", "dip");
	obs_property_list_add_string(st, "Slide — the new scene pushes the old off", "slide");
	obs_property_list_add_string(st, "Iris — a circle opens from the middle", "iris");
	obs_property_list_add_string(st, "Blinds", "blinds");
	obs_property_list_add_string(st, "Push — both scenes move together", "push");
	obs_property_list_add_string(st, "Bars — a band of accent takes the cut with it", "bars");
	obs_property_t *d = obs_properties_add_list(p, "direction", "Direction", OBS_COMBO_TYPE_LIST,
						    OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(d, "Left to right", "left-right");
	obs_property_list_add_string(d, "Right to left", "right-left");
	obs_property_list_add_string(d, "Top to bottom", "top-bottom");
	obs_property_list_add_string(d, "Bottom to top", "bottom-top");
	obs_properties_add_color(p, "accent", "Accent");
	obs_properties_add_float_slider(p, "bar_width", "Bar width", 0.0, 0.5, 0.01);
	obs_properties_add_float_slider(p, "softness", "Edge softness", 0.002, 0.2, 0.002);
	obs_properties_add_int_slider(p, "bars", "Blinds", 2, 24, 1);
	obs_properties_add_text(p, "hint",
				"Set the duration beside the transition in the Scene Transitions panel. "
				"300–500 ms suits the bar; the dip wants a little longer.",
				OBS_TEXT_INFO);
	return p;
}

static void wipe_defaults(obs_data_t *s)
{
	obs_data_set_default_string(s, "style", "bar");
	obs_data_set_default_string(s, "direction", "left-right");
	obs_data_set_default_int(s, "accent", (long long)SBK_ACCENT);
	obs_data_set_default_double(s, "bar_width", 0.12);
	obs_data_set_default_double(s, "softness", 0.02);
	obs_data_set_default_int(s, "bars", 6);
}

struct obs_source_info sbk_wipe_info = {
	.id = "sbk_wipe",
	.type = OBS_SOURCE_TYPE_TRANSITION,
	.get_name = wipe_name,
	.create = wipe_create,
	.destroy = wipe_destroy,
	.update = wipe_update,
	.get_defaults = wipe_defaults,
	.get_properties = wipe_properties,
	.video_render = wipe_video_render,
	.audio_render = wipe_audio_render,
};
