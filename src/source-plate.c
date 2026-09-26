/*  SBK Plate — the shadow, the lift and the glow that belong UNDER something.

    A filter cannot do this. A filter is handed its source's own rectangle and
    can only draw inside it; a drop shadow is by definition outside. So it is a
    source you place behind the camera or the panel it lifts — which is also the
    arrangement that looks right, because the shadow lands under the thing and
    SBK Cam Frame's outline still draws over the top of it.

    Match its shape to the box it sits behind and it does the rest.  */

#include "sbk-common.h"
#include "sbk-anim.h"

/* the same shape list the cam frame uses, so a plate and a frame can be set to
   the same thing without arithmetic */
struct plate_aspect {
	const char *id, *label;
	int w, h;
};

static const struct plate_aspect PLATE_ASPECTS[] = {
	{"custom", "Custom — use the width and height below", 0, 0},
	{"16x9", "16:9 — landscape", 640, 360},
	{"9x16", "9:16 — Shorts, Reels, a phone", 360, 640},
	{"1x1", "1:1 — square", 480, 480},
	{"4x5", "4:5 — portrait feed", 432, 540},
	{"4x3", "4:3 — classic", 560, 420},
	{"21x9", "21:9 — ultrawide", 756, 324},
};
#define N_PLATE_ASPECTS (sizeof(PLATE_ASPECTS) / sizeof(PLATE_ASPECTS[0]))

struct plate {
	obs_source_t *self;
	struct sbk_look look;
	struct sbk_anim anim;
	gs_effect_t *fx;

	const struct plate_aspect *aspect;
	float size_k, radius;
	struct vec4 fill, shadow, glow;
	float shadow_x, shadow_y, shadow_blur, shadow_grow, glow_size;
	bool fill_glass;
	uint32_t cx, cy;
};

static const char *plate_name(void *u)
{
	UNUSED_PARAMETER(u);
	return obs_module_text("SBK.Plate");
}

static void plate_update(void *data, obs_data_t *s)
{
	struct plate *p = data;
	sbk_look_read(&p->look, s);
	sbk_anim_read(&p->anim, s, 0.0f);

	const char *want = obs_data_get_string(s, "aspect");
	p->aspect = &PLATE_ASPECTS[0];
	for (size_t i = 1; i < N_PLATE_ASPECTS; i++)
		if (astrcmpi(PLATE_ASPECTS[i].id, want) == 0)
			p->aspect = &PLATE_ASPECTS[i];
	p->size_k = (float)obs_data_get_double(s, "size");
	p->cx = (uint32_t)obs_data_get_int(s, "width");
	p->cy = (uint32_t)obs_data_get_int(s, "height");
	if (p->aspect->w) {
		p->cx = (uint32_t)((float)p->aspect->w * p->size_k);
		p->cy = (uint32_t)((float)p->aspect->h * p->size_k);
	}

	p->radius = (float)obs_data_get_double(s, "radius");
	p->fill_glass = obs_data_get_bool(s, "fill_glass");
	p->fill = sbk_vec((uint32_t)obs_data_get_int(s, "fill"));
	p->shadow = sbk_vec((uint32_t)obs_data_get_int(s, "shadow"));
	p->shadow_x = (float)obs_data_get_double(s, "shadow_x");
	p->shadow_y = (float)obs_data_get_double(s, "shadow_y");
	p->shadow_blur = (float)obs_data_get_double(s, "shadow_blur");
	p->shadow_grow = (float)obs_data_get_double(s, "shadow_grow");
	p->glow = sbk_vec((uint32_t)obs_data_get_int(s, "glow"));
	p->glow_size = (float)obs_data_get_double(s, "glow_size");
}

static void *plate_create(obs_data_t *s, obs_source_t *source)
{
	struct plate *p = bzalloc(sizeof(*p));
	p->self = source;
	p->fx = sbk_load_effect("effects/plate.effect");
	plate_update(p, s);
	sbk_anim_play(&p->anim);
	return p;
}

static void plate_destroy(void *data)
{
	struct plate *p = data;
	sbk_free_effect(&p->fx);
	bfree(p);
}

static void plate_tick(void *data, float seconds)
{
	sbk_anim_tick(&((struct plate *)data)->anim, seconds);
}

/* The source has to be bigger than the shape, or the shadow is clipped by the
   very rectangle it is supposed to fall outside of. */
static float plate_pad(const struct plate *p)
{
	float k = p->look.scale;
	float reach = fmaxf(p->shadow_blur + p->shadow_grow + fmaxf(fabsf(p->shadow_x), fabsf(p->shadow_y)),
			    p->glow_size);
	return reach * k + 4.0f;
}

static uint32_t plate_width(void *d)
{
	struct plate *p = d;
	return p->cx + (uint32_t)(plate_pad(p) * 2.0f);
}
static uint32_t plate_height(void *d)
{
	struct plate *p = d;
	return p->cy + (uint32_t)(plate_pad(p) * 2.0f);
}

static void plate_render(void *data, gs_effect_t *unused)
{
	UNUSED_PARAMETER(unused);
	struct plate *p = data;
	if (!p->fx || !p->cx || !p->cy)
		return;
	const struct sbk_look *l = &p->look;
	const float k = l->scale;
	const float pad = plate_pad(p);
	struct sbk_anim_out a = sbk_anim_eval(&p->anim);

	struct vec4 fill = p->fill_glass ? l->glass : p->fill;
	fill = sbk_alpha(fill, a.alpha);

	sbk_set_vec2(p->fx, "size", (float)p->cx, (float)p->cy);
	sbk_set_float(p->fx, "pad", pad);
	sbk_set_float(p->fx, "radius", p->radius * k);
	sbk_set_vec4(p->fx, "fill", &fill);
	struct vec4 shadow = sbk_alpha(p->shadow, a.alpha);
	struct vec4 glow = sbk_alpha(p->glow, a.alpha);
	sbk_set_vec4(p->fx, "shadow", &shadow);
	sbk_set_float(p->fx, "shadow_x", p->shadow_x * k);
	sbk_set_float(p->fx, "shadow_y", p->shadow_y * k);
	sbk_set_float(p->fx, "shadow_blur", fmaxf(1.0f, p->shadow_blur * k));
	sbk_set_float(p->fx, "shadow_grow", p->shadow_grow * k);
	sbk_set_vec4(p->fx, "glow", &glow);
	sbk_set_float(p->fx, "glow_size", p->glow_size * k);

	gs_blend_state_push();
	gs_blend_function(GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA);
	while (gs_effect_loop(p->fx, "Draw"))
		gs_draw_sprite(NULL, 0, plate_width(p), plate_height(p));
	gs_blend_state_pop();
}

static void plate_show(void *d) { sbk_anim_on_show(&((struct plate *)d)->anim); }

static obs_properties_t *plate_properties(void *data)
{
	UNUSED_PARAMETER(data);
	obs_properties_t *p = obs_properties_create();
	obs_property_t *asp = obs_properties_add_list(p, "aspect", "Shape", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	for (size_t i = 0; i < N_PLATE_ASPECTS; i++)
		obs_property_list_add_string(asp, PLATE_ASPECTS[i].label, PLATE_ASPECTS[i].id);
	obs_properties_add_float_slider(p, "size", "Size (with a shape chosen)", 0.25, 4.0, 0.05);
	obs_properties_add_int(p, "width", "Width (Custom)", 40, 7680, 2);
	obs_properties_add_int(p, "height", "Height (Custom)", 40, 4320, 2);
	obs_properties_add_float_slider(p, "radius", "Corner radius", 0.0, 200.0, 1.0);

	obs_properties_t *f = obs_properties_create();
	obs_properties_add_bool(f, "fill_glass", "Use the Look's glass colour");
	obs_properties_add_color_alpha(f, "fill", "Fill");
	obs_properties_add_group(p, "plate", "The plate itself", OBS_GROUP_NORMAL, f);

	obs_properties_t *sh = obs_properties_create();
	obs_properties_add_color_alpha(sh, "shadow", "Shadow colour");
	obs_properties_add_float_slider(sh, "shadow_x", "Across", -120.0, 120.0, 1.0);
	obs_properties_add_float_slider(sh, "shadow_y", "Down", -120.0, 120.0, 1.0);
	obs_properties_add_float_slider(sh, "shadow_blur", "Softness", 1.0, 160.0, 1.0);
	obs_properties_add_float_slider(sh, "shadow_grow", "Spread", 0.0, 80.0, 1.0);
	obs_properties_add_group(p, "shad", "Shadow", OBS_GROUP_NORMAL, sh);

	obs_properties_t *g = obs_properties_create();
	obs_properties_add_color_alpha(g, "glow", "Glow colour");
	obs_properties_add_float_slider(g, "glow_size", "Reach", 0.0, 200.0, 1.0);
	obs_properties_add_group(p, "gl", "Glow", OBS_GROUP_NORMAL, g);

	sbk_look_props(p, false);
	sbk_anim_props(p);
	obs_properties_add_text(p, "hint",
				"Put it BELOW the camera in the Sources list and give it the same shape and size. "
				"The shadow falls outside the picture, which is why this is a source and not a "
				"filter — a filter can only draw inside its own rectangle.",
				OBS_TEXT_INFO);
	return p;
}

static void plate_defaults(obs_data_t *s)
{
	obs_data_set_default_string(s, "aspect", "16x9");
	obs_data_set_default_double(s, "size", 1.0);
	obs_data_set_default_int(s, "width", 640);
	obs_data_set_default_int(s, "height", 360);
	obs_data_set_default_double(s, "radius", 20.0);
	obs_data_set_default_bool(s, "fill_glass", false);
	obs_data_set_default_int(s, "fill", 0x00000000);
	obs_data_set_default_int(s, "shadow", 0x8C000000);
	obs_data_set_default_double(s, "shadow_x", 0.0);
	obs_data_set_default_double(s, "shadow_y", 16.0);
	obs_data_set_default_double(s, "shadow_blur", 44.0);
	obs_data_set_default_double(s, "shadow_grow", 2.0);
	obs_data_set_default_int(s, "glow", 0x00000000);
	obs_data_set_default_double(s, "glow_size", 0.0);
	sbk_look_defaults(s);
	sbk_anim_defaults(s, "fade");
}

struct obs_source_info sbk_plate_info = {
	.id = "sbk_plate",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
	.get_name = plate_name,
	.create = plate_create,
	.destroy = plate_destroy,
	.update = plate_update,
	.get_defaults = plate_defaults,
	.get_properties = plate_properties,
	.get_width = plate_width,
	.get_height = plate_height,
	.video_tick = plate_tick,
	.video_render = plate_render,
	.show = plate_show,
	.icon_type = OBS_ICON_TYPE_COLOR,
};
