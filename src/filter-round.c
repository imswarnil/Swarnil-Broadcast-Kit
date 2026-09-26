/*  SBK Round — a filter that rounds the corners of the source it sits on, and
    can draw a border just inside the edge.

    Apply it to your camera. This is the thing an overlay frame cannot do: a
    frame drawn on top is a rectangle with a hole in it, so the camera's square
    corners are still there underneath and the "rounded" webcam only looks
    rounded against a background that happens to match. A filter cuts the
    picture itself.  */

#include "sbk-common.h"

struct round_f {
	obs_source_t *self;
	gs_effect_t *fx;
	float radius, border, softness, inset;
	bool percent;
	struct vec4 border_color, fill;
	uint32_t cx, cy;
};

static const char *round_name(void *u)
{
	UNUSED_PARAMETER(u);
	return obs_module_text("SBK.Round");
}

static void round_update(void *data, obs_data_t *s)
{
	struct round_f *f = data;
	f->percent = obs_data_get_bool(s, "percent");
	f->radius = (float)obs_data_get_double(s, "radius");
	f->border = (float)obs_data_get_double(s, "border");
	f->softness = (float)obs_data_get_double(s, "softness");
	f->inset = (float)obs_data_get_double(s, "inset");
	f->border_color = sbk_vec((uint32_t)obs_data_get_int(s, "border_color"));
	f->fill = sbk_vec((uint32_t)obs_data_get_int(s, "fill"));
}

static void *round_create(obs_data_t *s, obs_source_t *source)
{
	struct round_f *f = bzalloc(sizeof(*f));
	f->self = source;
	f->fx = sbk_load_effect("effects/round.effect");
	round_update(f, s);
	return f;
}

static void round_destroy(void *data)
{
	struct round_f *f = data;
	sbk_free_effect(&f->fx);
	bfree(f);
}

static void round_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);
	struct round_f *f = data;
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

	/* As a percentage the same setting suits a 320px webcam box and a 1080p
	   share, which is the difference between a filter you set once and one you
	   re-tune for every source. Of the shorter side, so a wide source does not
	   get a radius taller than it is. */
	float shorter = (float)(f->cx < f->cy ? f->cx : f->cy);
	float radius = f->percent ? shorter * 0.5f * (f->radius / 100.0f) : f->radius;

	sbk_set_vec2(f->fx, "size", (float)f->cx, (float)f->cy);
	sbk_set_float(f->fx, "radius", radius);
	sbk_set_float(f->fx, "border", f->border);
	sbk_set_float(f->fx, "softness", f->softness);
	sbk_set_float(f->fx, "inset", f->inset);
	sbk_set_vec4(f->fx, "border_color", &f->border_color);
	sbk_set_vec4(f->fx, "fill", &f->fill);

	obs_source_process_filter_end(f->self, f->fx, f->cx, f->cy);
}

static obs_properties_t *round_properties(void *data)
{
	UNUSED_PARAMETER(data);
	obs_properties_t *p = obs_properties_create();
	obs_property_t *pc = obs_properties_add_bool(p, "percent", "Radius as a percentage of the shorter side");
	obs_property_set_long_description(pc,
		"On, 100 makes a circle out of a square source and a stadium out of a wide one — and the "
		"same setting suits a small webcam box and a full-frame share. Off, the radius is in pixels.");
	obs_properties_add_float_slider(p, "radius", "Radius", 0.0, 100.0, 0.5);
	obs_properties_add_float_slider(p, "border", "Border thickness (px)", 0.0, 40.0, 0.5);
	obs_properties_add_color_alpha(p, "border_color", "Border colour");
	obs_properties_add_float_slider(p, "inset", "Pull the shape in (px)", 0.0, 60.0, 1.0);
	obs_properties_add_float_slider(p, "softness", "Edge softness (px)", 0.5, 8.0, 0.25);
	obs_properties_add_color_alpha(p, "fill", "Behind the cut corners");
	obs_properties_add_text(p, "hint",
				"Add this to your camera under Filters, not to a scene. Put SBK Cam Frame over the "
				"top if you also want a chip or corner brackets.",
				OBS_TEXT_INFO);
	return p;
}

static void round_defaults(obs_data_t *s)
{
	obs_data_set_default_bool(s, "percent", true);
	obs_data_set_default_double(s, "radius", 14.0);
	obs_data_set_default_double(s, "border", 0.0);
	obs_data_set_default_int(s, "border_color", (long long)SBK_ACCENT);
	obs_data_set_default_double(s, "inset", 0.0);
	obs_data_set_default_double(s, "softness", 1.0);
	obs_data_set_default_int(s, "fill", 0x00000000);
}

struct obs_source_info sbk_round_info = {
	.id = "sbk_round",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_VIDEO,
	.get_name = round_name,
	.create = round_create,
	.destroy = round_destroy,
	.update = round_update,
	.get_defaults = round_defaults,
	.get_properties = round_properties,
	.video_render = round_render,
};
