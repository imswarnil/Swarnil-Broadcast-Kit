/*  SBK Colour — a grade for any source.

    A webcam under a desk lamp is the single most fixable thing on most streams,
    and fixing it means exposure, white balance and a little contrast, in that
    order. The presets are the corrections people actually need, not looks:
    "warm room" and "cold room" undo a cast rather than adding one.  */

#include "sbk-common.h"

struct colour {
	obs_source_t *self;
	gs_effect_t *fx;
	float exposure, contrast, saturation, vibrance, temperature, tint, fade;
	struct vec4 lift, gamma, gain;
	uint32_t cx, cy;
};

static const char *colour_name(void *u)
{
	UNUSED_PARAMETER(u);
	return obs_module_text("SBK.Colour");
}

/* the three trims are stored as 0..2 sliders around a neutral 1, because a
   colour picker for lift/gamma/gain is a worse control than three numbers */
static void colour_update(void *data, obs_data_t *s)
{
	struct colour *c = data;
	c->exposure = (float)obs_data_get_double(s, "exposure");
	c->contrast = (float)obs_data_get_double(s, "contrast");
	c->saturation = (float)obs_data_get_double(s, "saturation");
	c->vibrance = (float)obs_data_get_double(s, "vibrance");
	c->temperature = (float)obs_data_get_double(s, "temperature");
	c->tint = (float)obs_data_get_double(s, "tint");
	c->fade = (float)obs_data_get_double(s, "fade");
	vec4_set(&c->lift, (float)obs_data_get_double(s, "lift_r"), (float)obs_data_get_double(s, "lift_g"),
		 (float)obs_data_get_double(s, "lift_b"), 0.0f);
	vec4_set(&c->gamma, (float)obs_data_get_double(s, "gamma_r"), (float)obs_data_get_double(s, "gamma_g"),
		 (float)obs_data_get_double(s, "gamma_b"), 1.0f);
	vec4_set(&c->gain, (float)obs_data_get_double(s, "gain_r"), (float)obs_data_get_double(s, "gain_g"),
		 (float)obs_data_get_double(s, "gain_b"), 1.0f);
}

static void *colour_create(obs_data_t *s, obs_source_t *source)
{
	struct colour *c = bzalloc(sizeof(*c));
	c->self = source;
	c->fx = sbk_load_effect("effects/colour.effect");
	colour_update(c, s);
	return c;
}

static void colour_destroy(void *data)
{
	struct colour *c = data;
	sbk_free_effect(&c->fx);
	bfree(c);
}

static void colour_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);
	struct colour *c = data;
	obs_source_t *target = obs_filter_get_target(c->self);
	if (!c->fx || !target) {
		obs_source_skip_video_filter(c->self);
		return;
	}
	c->cx = obs_source_get_base_width(target);
	c->cy = obs_source_get_base_height(target);
	if (!c->cx || !c->cy) {
		obs_source_skip_video_filter(c->self);
		return;
	}
	if (!obs_source_process_filter_begin(c->self, GS_RGBA, OBS_ALLOW_DIRECT_RENDERING))
		return;

	sbk_set_float(c->fx, "exposure", c->exposure);
	sbk_set_float(c->fx, "contrast", c->contrast);
	sbk_set_float(c->fx, "saturation", c->saturation);
	sbk_set_float(c->fx, "vibrance", c->vibrance);
	sbk_set_float(c->fx, "temperature", c->temperature);
	sbk_set_float(c->fx, "tint", c->tint);
	sbk_set_float(c->fx, "fade", c->fade);
	gs_eparam_t *p;
	if ((p = gs_effect_get_param_by_name(c->fx, "lift")))
		gs_effect_set_vec3(p, (struct vec3 *)&c->lift);
	if ((p = gs_effect_get_param_by_name(c->fx, "gamma")))
		gs_effect_set_vec3(p, (struct vec3 *)&c->gamma);
	if ((p = gs_effect_get_param_by_name(c->fx, "gain")))
		gs_effect_set_vec3(p, (struct vec3 *)&c->gain);

	obs_source_process_filter_end(c->self, c->fx, c->cx, c->cy);
}

struct cpreset {
	const char *id, *label;
	float exposure, contrast, saturation, vibrance, temperature, tint, fade;
};

static const struct cpreset CPRESETS[] = {
	{"neutral", "Neutral — undo everything", 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
	{"lift", "Lift a dark webcam", 0.55f, 0.06f, 0.05f, 0.12f, 0.04f, 0.0f, 0.0f},
	{"warm-room", "Warm room — take the orange out", 0.15f, 0.04f, 0.0f, 0.06f, -0.22f, 0.02f, 0.0f},
	{"cold-room", "Cold room — take the blue out", 0.15f, 0.04f, 0.0f, 0.06f, 0.24f, -0.02f, 0.0f},
	{"filmic", "Filmic — soft blacks, low saturation", 0.05f, 0.12f, -0.12f, 0.10f, 0.05f, 0.0f, 0.14f},
	{"punchy", "Punchy — for a screen share", 0.0f, 0.20f, 0.14f, 0.10f, 0.0f, 0.0f, 0.0f},
	{"mono", "Monochrome", 0.05f, 0.10f, -1.0f, 0.0f, 0.0f, 0.0f, 0.04f},
};
#define N_CPRESETS (sizeof(CPRESETS) / sizeof(CPRESETS[0]))

static bool on_cpreset(obs_properties_t *props, obs_property_t *p, obs_data_t *s)
{
	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(p);
	const char *want = obs_data_get_string(s, "preset");
	if (!want || astrcmpi(want, "custom") == 0)
		return false;
	for (size_t i = 0; i < N_CPRESETS; i++) {
		if (astrcmpi(CPRESETS[i].id, want) != 0)
			continue;
		const struct cpreset *v = &CPRESETS[i];
		obs_data_set_double(s, "exposure", v->exposure);
		obs_data_set_double(s, "contrast", v->contrast);
		obs_data_set_double(s, "saturation", v->saturation);
		obs_data_set_double(s, "vibrance", v->vibrance);
		obs_data_set_double(s, "temperature", v->temperature);
		obs_data_set_double(s, "tint", v->tint);
		obs_data_set_double(s, "fade", v->fade);
		obs_data_set_string(s, "preset", "custom");
		return true;
	}
	return false;
}

static obs_properties_t *colour_properties(void *data)
{
	UNUSED_PARAMETER(data);
	obs_properties_t *p = obs_properties_create();
	obs_property_t *pr = obs_properties_add_list(p, "preset", "Start from", OBS_COMBO_TYPE_LIST,
						     OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(pr, "Custom", "custom");
	for (size_t i = 0; i < N_CPRESETS; i++)
		obs_property_list_add_string(pr, CPRESETS[i].label, CPRESETS[i].id);
	obs_property_set_modified_callback(pr, on_cpreset);

	obs_properties_add_float_slider(p, "exposure", "Exposure (stops)", -3.0, 3.0, 0.01);
	obs_properties_add_float_slider(p, "contrast", "Contrast", -1.0, 1.0, 0.01);
	obs_properties_add_float_slider(p, "temperature", "Temperature", -1.0, 1.0, 0.01);
	obs_properties_add_float_slider(p, "tint", "Tint", -1.0, 1.0, 0.01);
	obs_properties_add_float_slider(p, "saturation", "Saturation", -1.0, 1.0, 0.01);
	obs_property_t *vb = obs_properties_add_float_slider(p, "vibrance", "Vibrance", -1.0, 1.0, 0.01);
	obs_property_set_long_description(vb, "Saturation that spares what is already vivid — it lifts a dull face "
					      "without turning a red shirt into a warning sign.");
	obs_properties_add_float_slider(p, "fade", "Faded blacks", 0.0, 0.5, 0.01);

	obs_properties_t *t = obs_properties_create();
	const char *chan[3] = {"r", "g", "b"};
	const char *label[3] = {"Red", "Green", "Blue"};
	for (int i = 0; i < 3; i++) {
		struct dstr k = {0}, l = {0};
		dstr_printf(&k, "lift_%s", chan[i]);
		dstr_printf(&l, "Lift %s", label[i]);
		obs_properties_add_float_slider(t, k.array, l.array, -0.3, 0.3, 0.005);
		dstr_printf(&k, "gamma_%s", chan[i]);
		dstr_printf(&l, "Gamma %s", label[i]);
		obs_properties_add_float_slider(t, k.array, l.array, 0.3, 3.0, 0.01);
		dstr_printf(&k, "gain_%s", chan[i]);
		dstr_printf(&l, "Gain %s", label[i]);
		obs_properties_add_float_slider(t, k.array, l.array, 0.0, 3.0, 0.01);
		dstr_free(&k);
		dstr_free(&l);
	}
	obs_properties_add_group(p, "trim", "Lift, gamma and gain", OBS_GROUP_NORMAL, t);

	obs_properties_add_text(p, "hint",
				"Fix the exposure and the white balance before reaching for anything else — a dark, "
				"orange webcam is the most common and most fixable problem on a stream.",
				OBS_TEXT_INFO);
	return p;
}

static void colour_defaults(obs_data_t *s)
{
	obs_data_set_default_string(s, "preset", "custom");
	obs_data_set_default_double(s, "exposure", 0.0);
	obs_data_set_default_double(s, "contrast", 0.0);
	obs_data_set_default_double(s, "saturation", 0.0);
	obs_data_set_default_double(s, "vibrance", 0.0);
	obs_data_set_default_double(s, "temperature", 0.0);
	obs_data_set_default_double(s, "tint", 0.0);
	obs_data_set_default_double(s, "fade", 0.0);
	const char *chan[3] = {"r", "g", "b"};
	for (int i = 0; i < 3; i++) {
		struct dstr k = {0};
		dstr_printf(&k, "lift_%s", chan[i]);
		obs_data_set_default_double(s, k.array, 0.0);
		dstr_printf(&k, "gamma_%s", chan[i]);
		obs_data_set_default_double(s, k.array, 1.0);
		dstr_printf(&k, "gain_%s", chan[i]);
		obs_data_set_default_double(s, k.array, 1.0);
		dstr_free(&k);
	}
}

struct obs_source_info sbk_colour_info = {
	.id = "sbk_colour",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_VIDEO,
	.get_name = colour_name,
	.create = colour_create,
	.destroy = colour_destroy,
	.update = colour_update,
	.get_defaults = colour_defaults,
	.get_properties = colour_properties,
	.video_render = colour_render,
};
