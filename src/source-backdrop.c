/*  SBK Backdrop — the ground under an announcement scene: a solid, a scrim
    rising from the foot so a camera can sit behind the card, or a vignette.  */

#include "sbk-common.h"
#include "sbk-anim.h"

struct backdrop {
	struct sbk_anim anim;
	gs_effect_t *fx;
	struct vec4 color, color2;
	float mode, reach, pitch, weight, angle, drift, t;
	uint32_t cx, cy;
};

static const char *bd_name(void *u)
{
	UNUSED_PARAMETER(u);
	return obs_module_text("SBK.Backdrop");
}

static void bd_update(void *data, obs_data_t *s)
{
	struct backdrop *b = data;
	sbk_anim_read(&b->anim, s, 0.0f);
	const char *m = obs_data_get_string(s, "mode");
	b->mode = astrcmpi(m, "scrim") == 0        ? 1.0f
		  : astrcmpi(m, "scrim-top") == 0  ? 2.0f
		  : astrcmpi(m, "vignette") == 0   ? 3.0f
		  : astrcmpi(m, "gradient") == 0   ? 4.0f
		  : astrcmpi(m, "grid") == 0       ? 5.0f
		  : astrcmpi(m, "dots") == 0       ? 6.0f
		  : astrcmpi(m, "stripes") == 0    ? 7.0f
		  : astrcmpi(m, "waves") == 0      ? 8.0f
		  : astrcmpi(m, "rings") == 0      ? 9.0f
		  : astrcmpi(m, "hex") == 0        ? 10.0f
		  : astrcmpi(m, "grain") == 0      ? 11.0f
		  : astrcmpi(m, "aurora") == 0     ? 12.0f
		  : astrcmpi(m, "plasma") == 0     ? 13.0f
		  : astrcmpi(m, "stars") == 0      ? 14.0f
		  : astrcmpi(m, "checkers") == 0   ? 15.0f
						   : 0.0f;
	b->color = sbk_vec((uint32_t)obs_data_get_int(s, "color"));
	b->color2 = sbk_vec((uint32_t)obs_data_get_int(s, "color2"));
	b->reach = (float)obs_data_get_double(s, "reach");
	b->pitch = (float)obs_data_get_double(s, "pitch");
	b->weight = (float)obs_data_get_double(s, "weight");
	b->angle = astrcmpi(obs_data_get_string(s, "angle"), "diagonal") == 0 ? 1.0f
		   : astrcmpi(obs_data_get_string(s, "angle"), "horizontal") == 0 ? 2.0f : 0.0f;
	b->drift = (float)obs_data_get_double(s, "drift");
	b->cx = (uint32_t)obs_data_get_int(s, "width");
	b->cy = (uint32_t)obs_data_get_int(s, "height");
}

static void *bd_create(obs_data_t *s, obs_source_t *source)
{
	UNUSED_PARAMETER(source);
	struct backdrop *b = bzalloc(sizeof(*b));
	b->fx = sbk_load_effect("effects/backdrop.effect");
	bd_update(b, s);
	sbk_anim_play(&b->anim);
	return b;
}

static void bd_destroy(void *data)
{
	struct backdrop *b = data;
	sbk_free_effect(&b->fx);
	bfree(b);
}

static void bd_tick(void *data, float seconds)
{
	struct backdrop *b = data;
	b->t += seconds;
	sbk_anim_tick(&b->anim, seconds);
}
static uint32_t bd_width(void *d) { return ((struct backdrop *)d)->cx; }
static uint32_t bd_height(void *d) { return ((struct backdrop *)d)->cy; }

static void bd_render(void *data, gs_effect_t *unused)
{
	UNUSED_PARAMETER(unused);
	struct backdrop *b = data;
	if (!b->fx)
		return;
	struct sbk_anim_out a = sbk_anim_eval(&b->anim);
	struct vec4 col = sbk_alpha(b->color, a.alpha);
	struct vec4 col2 = sbk_alpha(b->color2, a.alpha);
	sbk_set_vec4(b->fx, "color", &col);
	sbk_set_vec4(b->fx, "color2", &col2);
	sbk_set_float(b->fx, "mode", b->mode);
	sbk_set_float(b->fx, "reach", b->reach);
	sbk_set_float(b->fx, "pitch", fmaxf(4.0f, b->pitch));
	sbk_set_float(b->fx, "weight", b->weight);
	sbk_set_float(b->fx, "angle", b->angle);
	sbk_set_float(b->fx, "time", b->t);
	sbk_set_float(b->fx, "drift", b->drift);
	sbk_set_vec2(b->fx, "size", (float)b->cx, (float)b->cy);
	gs_blend_state_push();
	gs_blend_function(GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA);
	while (gs_effect_loop(b->fx, "Draw"))
		gs_draw_sprite(NULL, 0, b->cx, b->cy);
	gs_blend_state_pop();
}

static void bd_show(void *d) { sbk_anim_on_show(&((struct backdrop *)d)->anim); }

static obs_properties_t *bd_properties(void *data)
{
	UNUSED_PARAMETER(data);
	obs_properties_t *p = obs_properties_create();
	obs_property_t *m = obs_properties_add_list(p, "mode", "Kind", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(m, "Solid", "solid");
	obs_property_list_add_string(m, "Scrim, rising from the foot", "scrim");
	obs_property_list_add_string(m, "Scrim, falling from the head", "scrim-top");
	obs_property_list_add_string(m, "Vignette", "vignette");
	obs_property_list_add_string(m, "Gradient", "gradient");
	obs_property_list_add_string(m, "Grid", "grid");
	obs_property_list_add_string(m, "Dot grid", "dots");
	obs_property_list_add_string(m, "Diagonal stripes", "stripes");
	obs_property_list_add_string(m, "Waves", "waves");
	obs_property_list_add_string(m, "Concentric rings", "rings");
	obs_property_list_add_string(m, "Hex grid", "hex");
	obs_property_list_add_string(m, "Grain", "grain");
	obs_property_list_add_string(m, "Aurora — drifting light", "aurora");
	obs_property_list_add_string(m, "Plasma", "plasma");
	obs_property_list_add_string(m, "Starfield", "stars");
	obs_property_list_add_string(m, "Checkers", "checkers");
	obs_properties_add_color_alpha(p, "color", "Colour");
	obs_properties_add_color_alpha(p, "color2", "Second colour (gradient and patterns)");
	obs_properties_add_float_slider(p, "reach", "Reach (scrim and vignette)", 0.1, 1.0, 0.01);

	obs_properties_t *pat = obs_properties_create();
	obs_properties_add_float_slider(pat, "pitch", "Spacing", 8.0, 240.0, 2.0);
	obs_properties_add_float_slider(pat, "weight", "Line or dot size", 0.5, 24.0, 0.5);
	obs_properties_add_float_slider(pat, "drift", "Drift (px/s)", -40.0, 40.0, 1.0);
	obs_property_t *ang = obs_properties_add_list(pat, "angle", "Gradient direction", OBS_COMBO_TYPE_LIST,
						      OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(ang, "Vertical", "vertical");
	obs_property_list_add_string(ang, "Diagonal", "diagonal");
	obs_property_list_add_string(ang, "Horizontal", "horizontal");
	obs_properties_add_group(p, "pattern", "Gradient and patterns", OBS_GROUP_NORMAL, pat);
	obs_properties_add_int(p, "width", "Width", 16, 7680, 2);
	obs_properties_add_int(p, "height", "Height", 16, 4320, 2);
	sbk_anim_props(p);
	return p;
}

static void bd_defaults(obs_data_t *s)
{
	struct obs_video_info ovi;
	bool have = obs_get_video_info(&ovi);
	obs_data_set_default_string(s, "mode", "solid");
	obs_data_set_default_int(s, "color", (long long)SBK_SOLID);
	obs_data_set_default_int(s, "color2", 0x14FFFFFF);
	obs_data_set_default_double(s, "reach", 0.6);
	obs_data_set_default_double(s, "pitch", 64.0);
	obs_data_set_default_double(s, "weight", 1.5);
	obs_data_set_default_double(s, "drift", 6.0);
	obs_data_set_default_string(s, "angle", "diagonal");
	obs_data_set_default_int(s, "width", have ? ovi.base_width : 1920);
	obs_data_set_default_int(s, "height", have ? ovi.base_height : 1080);
	sbk_anim_defaults(s, "fade");
}

struct obs_source_info sbk_backdrop_info = {
	.id = "sbk_backdrop",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
	.get_name = bd_name,
	.create = bd_create,
	.destroy = bd_destroy,
	.update = bd_update,
	.get_defaults = bd_defaults,
	.get_properties = bd_properties,
	.get_width = bd_width,
	.get_height = bd_height,
	.video_tick = bd_tick,
	.video_render = bd_render,
	.show = bd_show,
	.icon_type = OBS_ICON_TYPE_COLOR,
};
