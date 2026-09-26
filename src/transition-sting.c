/*  SBK Sting — a logo sting, without a video file.

    A stinger transition normally means rendering a video with an alpha channel
    in something else, exporting it, and pointing OBS at the file. This does the
    same job from the shape the kit already has: a colour field crosses the
    frame, the mark lands in the middle, the field leaves, and the cut happens
    underneath while the frame is covered — which is the entire trick.

    If you have a logo, point it at the file and it is laid over the drawn mark.  */

#include <graphics/image-file.h>

#include "sbk-common.h"

struct sting {
	obs_source_t *self;
	gs_effect_t *fx;

	float style, dir, softness, mark_size, hold;
	struct vec4 field, mark;

	char *logo_path;
	gs_image_file_t logo;
	bool logo_loaded;
};

static const char *sting_name(void *u)
{
	UNUSED_PARAMETER(u);
	return obs_module_text("SBK.Sting");
}

static void unload_logo(struct sting *s)
{
	if (!s->logo_loaded)
		return;
	obs_enter_graphics();
	gs_image_file_free(&s->logo);
	obs_leave_graphics();
	s->logo_loaded = false;
}

static void load_logo(struct sting *s, const char *path)
{
	unload_logo(s);
	if (!path || !*path)
		return;
	gs_image_file_init(&s->logo, path);
	obs_enter_graphics();
	gs_image_file_init_texture(&s->logo);
	obs_leave_graphics();
	s->logo_loaded = s->logo.loaded;
	if (!s->logo_loaded)
		SBK_LOG(LOG_WARNING, "sting: could not read the logo at %s", path);
}

static void sting_update(void *data, obs_data_t *settings)
{
	struct sting *s = data;
	const char *st = obs_data_get_string(settings, "style");
	s->style = astrcmpi(st, "iris") == 0 ? 1.0f : astrcmpi(st, "curtain") == 0 ? 2.0f : 0.0f;
	const char *d = obs_data_get_string(settings, "direction");
	s->dir = astrcmpi(d, "right-left") == 0 ? 1.0f : astrcmpi(d, "top-bottom") == 0 ? 2.0f
		 : astrcmpi(d, "bottom-top") == 0 ? 3.0f : 0.0f;
	s->field = sbk_vec((uint32_t)obs_data_get_int(settings, "field"));
	s->mark = sbk_vec((uint32_t)obs_data_get_int(settings, "mark"));
	s->mark_size = (float)obs_data_get_double(settings, "mark_size");
	s->softness = (float)obs_data_get_double(settings, "softness");
	s->hold = (float)obs_data_get_double(settings, "hold");

	const char *path = obs_data_get_string(settings, "logo");
	if (!s->logo_path || strcmp(s->logo_path, path) != 0) {
		bfree(s->logo_path);
		s->logo_path = bstrdup(path);
		load_logo(s, path);
	}
}

static void *sting_create(obs_data_t *settings, obs_source_t *source)
{
	struct sting *s = bzalloc(sizeof(*s));
	s->self = source;
	s->fx = sbk_load_effect("effects/sting.effect");
	sting_update(s, settings);
	return s;
}

static void sting_destroy(void *data)
{
	struct sting *s = data;
	unload_logo(s);
	sbk_free_effect(&s->fx);
	bfree(s->logo_path);
	bfree(s);
}

/* ease in and out, so the field does not arrive or leave at a constant rate */
static float ease(float t)
{
	t = sbk_clampf(t, 0.0f, 1.0f);
	return t < 0.5f ? 4.0f * t * t * t : 1.0f - powf(-2.0f * t + 2.0f, 3.0f) / 2.0f;
}

static void sting_callback(void *data, gs_texture_t *a, gs_texture_t *b, float t, uint32_t cx, uint32_t cy)
{
	struct sting *s = data;
	if (!s->fx)
		return;

	/* three phases: the field arrives, it holds with the mark on it, it
	   leaves. The hold is what stops a sting feeling like a wipe. */
	const float hold = sbk_clampf(s->hold, 0.0f, 0.6f);
	const float half = (1.0f - hold) * 0.5f;
	float cover, mark_in;
	if (t < half) {
		cover = ease(t / half);
		mark_in = sbk_clampf((t / half - 0.45f) / 0.55f, 0.0f, 1.0f);
	} else if (t < half + hold) {
		cover = 1.0f;
		mark_in = 1.0f;
	} else {
		float u = (t - half - hold) / fmaxf(half, 0.0001f);
		cover = 1.0f - ease(u);
		mark_in = 1.0f - sbk_clampf(u / 0.5f, 0.0f, 1.0f);
	}

	gs_eparam_t *pa = gs_effect_get_param_by_name(s->fx, "tex_a");
	gs_eparam_t *pb = gs_effect_get_param_by_name(s->fx, "tex_b");
	if (pa)
		gs_effect_set_texture(pa, a);
	if (pb)
		gs_effect_set_texture(pb, b);
	sbk_set_vec2(s->fx, "size", (float)cx, (float)cy);
	sbk_set_float(s->fx, "t", t);
	sbk_set_float(s->fx, "cover", cover);
	sbk_set_float(s->fx, "style", s->style);
	sbk_set_float(s->fx, "dir", s->dir);
	sbk_set_vec4(s->fx, "field", &s->field);
	/* a logo image replaces the drawn mark rather than sitting on top of it */
	struct vec4 none = SBK_NONE;
	const struct vec4 *mark = s->logo_loaded ? &none : &s->mark;
	sbk_set_vec4(s->fx, "mark", mark);
	sbk_set_float(s->fx, "mark_in", mark_in);
	sbk_set_float(s->fx, "mark_size", s->mark_size);
	sbk_set_float(s->fx, "softness", s->softness);

	while (gs_effect_loop(s->fx, "Draw"))
		gs_draw_sprite(NULL, 0, cx, cy);

	if (s->logo_loaded && mark_in > 0.001f && s->logo.texture) {
		/* drawn after the field so it lands on top of it, scaled to the
		   mark size and fading in with the same curve */
		float w = (float)s->logo.cx, h = (float)s->logo.cy;
		float k = (s->mark_size * 2.0f) / fmaxf(w, 1.0f);
		float dw = w * k, dh = h * k;
		gs_effect_t *base = obs_get_base_effect(OBS_EFFECT_DEFAULT);
		gs_eparam_t *img = gs_effect_get_param_by_name(base, "image");
		gs_effect_set_texture(img, s->logo.texture);
		gs_blend_state_push();
		gs_blend_function(GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA);
		gs_matrix_push();
		gs_matrix_translate3f(((float)cx - dw) * 0.5f, ((float)cy - dh) * 0.5f, 0.0f);
		gs_matrix_scale3f(k, k, 1.0f);
		while (gs_effect_loop(base, "Draw"))
			gs_draw_sprite(s->logo.texture, 0, s->logo.cx, s->logo.cy);
		gs_matrix_pop();
		gs_blend_state_pop();
	}
}

static void sting_video_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);
	struct sting *s = data;
	obs_transition_video_render(s->self, sting_callback);
}

static float mix_a(void *data, float t)
{
	UNUSED_PARAMETER(data);
	/* the audio ducks toward the middle with the picture rather than
	   crossfading straight through it */
	return 1.0f - sbk_clampf(t * 2.0f, 0.0f, 1.0f);
}
static float mix_b(void *data, float t)
{
	UNUSED_PARAMETER(data);
	return sbk_clampf((t - 0.5f) * 2.0f, 0.0f, 1.0f);
}

static bool sting_audio_render(void *data, uint64_t *ts_out, struct obs_source_audio_mix *audio, uint32_t mixers,
			       size_t channels, size_t sample_rate)
{
	struct sting *s = data;
	return obs_transition_audio_render(s->self, ts_out, audio, mixers, channels, sample_rate, mix_a, mix_b);
}

static obs_properties_t *sting_properties(void *data)
{
	UNUSED_PARAMETER(data);
	obs_properties_t *p = obs_properties_create();
	obs_property_t *st = obs_properties_add_list(p, "style", "The field arrives as", OBS_COMBO_TYPE_LIST,
						     OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(st, "A band across the frame", "band");
	obs_property_list_add_string(st, "An iris from the middle", "iris");
	obs_property_list_add_string(st, "Curtains from both edges", "curtain");
	obs_property_t *d = obs_properties_add_list(p, "direction", "Direction", OBS_COMBO_TYPE_LIST,
						    OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(d, "Left to right", "left-right");
	obs_property_list_add_string(d, "Right to left", "right-left");
	obs_property_list_add_string(d, "Top to bottom", "top-bottom");
	obs_property_list_add_string(d, "Bottom to top", "bottom-top");
	obs_properties_add_color_alpha(p, "field", "Field colour");
	obs_properties_add_color_alpha(p, "mark", "Mark colour");
	obs_properties_add_float_slider(p, "mark_size", "Mark size (px)", 20.0, 400.0, 2.0);
	obs_property_t *logo = obs_properties_add_path(p, "logo", "Logo image (optional)", OBS_PATH_FILE,
						       "Images (*.png *.jpg *.jpeg *.gif *.webp *.svg);;All (*.*)", NULL);
	obs_property_set_long_description(logo, "A PNG with transparency looks best. It replaces the drawn mark and "
						"is scaled to the mark size.");
	obs_properties_add_float_slider(p, "hold", "Hold the field (fraction)", 0.0, 0.6, 0.02);
	obs_properties_add_float_slider(p, "softness", "Edge softness", 0.002, 0.25, 0.002);
	obs_properties_add_text(p, "hint",
				"Set the duration beside the transition in the Scene Transitions panel — a sting "
				"wants longer than a wipe, around 800–1200 ms, or the mark is gone before anyone "
				"has seen it. The cut happens while the frame is covered.",
				OBS_TEXT_INFO);
	return p;
}

static void sting_defaults(obs_data_t *s)
{
	obs_data_set_default_string(s, "style", "band");
	obs_data_set_default_string(s, "direction", "left-right");
	obs_data_set_default_int(s, "field", (long long)SBK_ACCENT);
	obs_data_set_default_int(s, "mark", 0xFFFFFFFF);
	obs_data_set_default_double(s, "mark_size", 110.0);
	obs_data_set_default_string(s, "logo", "");
	obs_data_set_default_double(s, "hold", 0.22);
	obs_data_set_default_double(s, "softness", 0.02);
}

struct obs_source_info sbk_sting_info = {
	.id = "sbk_sting",
	.type = OBS_SOURCE_TYPE_TRANSITION,
	.get_name = sting_name,
	.create = sting_create,
	.destroy = sting_destroy,
	.update = sting_update,
	.get_defaults = sting_defaults,
	.get_properties = sting_properties,
	.video_render = sting_video_render,
	.audio_render = sting_audio_render,
};
