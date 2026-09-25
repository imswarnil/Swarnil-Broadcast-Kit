/*  SBK Frame — a rounded outline to sit over the camera, with a chip on its
    edge. Size it to the camera; the line keeps its weight round the corners
    because it is one signed-distance shape, not four rectangles.  */

#include "sbk-common.h"
#include "sbk-text.h"
#include "sbk-stage.h"
#include "sbk-anim.h"

struct frame {
	obs_source_t *self;
	struct sbk_look look;
	struct sbk_anim anim;
	struct sbk_stage stage;
	gs_effect_t *card_fx;
	struct sbk_text chip;
	gs_effect_t *frame_fx;
	const struct aspect_def *aspect;
	float size_k;
	char *s_label, *line_kind, *chip_at, *style_id;
	bool chip_accent;
	float radius, weight, bracket, glow;
	uint32_t cx, cy;
};

/*  The shapes a camera actually gets cut to. Picking one sets the box and the
    size slider scales it, so a 9:16 frame for a Short is one menu away rather
    than two numbers to work out. Custom leaves the width and height in charge. */
struct aspect_def {
	const char *id, *label;
	int w, h;
};

static const struct aspect_def ASPECTS[] = {
	{"custom", "Custom — use the width and height below", 0, 0},
	{"16x9", "16:9 — landscape", 640, 360},
	{"9x16", "9:16 — Shorts, Reels, a phone", 360, 640},
	{"1x1", "1:1 — square, for a post", 480, 480},
	{"4x5", "4:5 — portrait feed", 432, 540},
	{"4x3", "4:3 — classic", 560, 420},
	{"21x9", "21:9 — ultrawide", 756, 324},
};
#define N_ASPECTS (sizeof(ASPECTS) / sizeof(ASPECTS[0]))

static const char *frame_name(void *u)
{
	UNUSED_PARAMETER(u);
	return obs_module_text("SBK.Frame");
}

static void frame_update(void *data, obs_data_t *s)
{
	struct frame *f = data;
	sbk_look_read(&f->look, s);
	sbk_anim_read(&f->anim, s, 0.0f);
	bfree(f->s_label);
	bfree(f->line_kind);
	bfree(f->chip_at);
	f->s_label = bstrdup(obs_data_get_string(s, "label"));
	f->line_kind = bstrdup(obs_data_get_string(s, "line"));
	f->chip_at = bstrdup(obs_data_get_string(s, "chip_at"));
	bfree(f->style_id);
	f->style_id = bstrdup(obs_data_get_string(s, "style"));
	f->chip_accent = obs_data_get_bool(s, "chip_accent");
	f->cx = (uint32_t)obs_data_get_int(s, "width");
	f->cy = (uint32_t)obs_data_get_int(s, "height");
	const char *want = obs_data_get_string(s, "aspect");
	f->aspect = &ASPECTS[0];
	for (size_t i = 1; i < N_ASPECTS; i++)
		if (astrcmpi(ASPECTS[i].id, want) == 0)
			f->aspect = &ASPECTS[i];
	f->size_k = (float)obs_data_get_double(s, "size");
	if (f->aspect->w) {
		f->cx = (uint32_t)((float)f->aspect->w * f->size_k);
		f->cy = (uint32_t)((float)f->aspect->h * f->size_k);
	}
	f->radius = (float)obs_data_get_double(s, "radius");
	f->weight = (float)obs_data_get_double(s, "weight");
	f->bracket = (float)obs_data_get_double(s, "bracket");
	f->glow = (float)obs_data_get_double(s, "glow");
}

/* matches the switch in frame.effect */
static float style_kind(const char *id)
{
	if (!id) return 1.0f;
	if (astrcmpi(id, "none") == 0) return 0.0f;
	if (astrcmpi(id, "inset") == 0) return 2.0f;
	if (astrcmpi(id, "corner") == 0) return 3.0f;
	if (astrcmpi(id, "corner-out") == 0) return 4.0f;
	if (astrcmpi(id, "edge") == 0) return 5.0f;
	if (astrcmpi(id, "glow") == 0) return 6.0f;
	if (astrcmpi(id, "double") == 0) return 7.0f;
	return 1.0f;
}

static void *frame_create(obs_data_t *s, obs_source_t *source)
{
	struct frame *f = bzalloc(sizeof(*f));
	f->self = source;
	f->card_fx = sbk_load_effect("effects/card.effect");
	f->frame_fx = sbk_load_effect("effects/frame.effect");
	sbk_stage_init(&f->stage);
	frame_update(f, s);
	sbk_anim_play(&f->anim);
	return f;
}

static void frame_destroy(void *data)
{
	struct frame *f = data;
	sbk_text_free(&f->chip);
	sbk_stage_free(&f->stage);
	sbk_free_effect(&f->card_fx);
	sbk_free_effect(&f->frame_fx);
	bfree(f->s_label);
	bfree(f->style_id);
	bfree(f->line_kind);
	bfree(f->chip_at);
	bfree(f);
}

static void frame_tick(void *data, float seconds)
{
	struct frame *f = data;
	sbk_anim_tick(&f->anim, seconds);
	const struct sbk_look *l = &f->look;
	sbk_text_set(&f->chip, f->s_label, l->face, "SemiBold", (int)(4.5f * sbk_u(l)),
		       f->chip_accent ? l->on_accent : l->ink);
}

static uint32_t frame_width(void *d) { return ((struct frame *)d)->cx; }
static uint32_t frame_height(void *d) { return ((struct frame *)d)->cy; }

static void frame_render(void *data, gs_effect_t *unused)
{
	UNUSED_PARAMETER(unused);
	struct frame *f = data;
	if (!f->card_fx || !sbk_stage_begin(&f->stage, f->cx, f->cy))
		return;
	const struct sbk_look *l = &f->look;
	const float u = sbk_u(l);
	struct vec4 none = {{{0.0f, 0.0f, 0.0f, 0.0f}}};

	struct vec4 line = astrcmpi(f->line_kind, "accent") == 0 ? l->accent
			   : astrcmpi(f->line_kind, "ink") == 0 ? l->ink
								: sbk_alpha(l->ink, 0.4f);
	if (f->frame_fx) {
		float glow = f->glow * l->scale;
		float pad = glow > 0.5f ? glow * 2.0f : 0.0f;
		sbk_set_vec4(f->frame_fx, "color", &line);
		sbk_set_vec2(f->frame_fx, "size", (float)f->cx, (float)f->cy);
		sbk_set_float(f->frame_fx, "pad", pad);
		sbk_set_float(f->frame_fx, "radius", f->radius * l->scale);
		sbk_set_float(f->frame_fx, "weight", f->weight * l->scale);
		sbk_set_float(f->frame_fx, "bracket", f->bracket * l->scale);
		sbk_set_float(f->frame_fx, "style", style_kind(f->style_id));
		sbk_set_float(f->frame_fx, "glow_size", glow);
		gs_matrix_push();
		gs_matrix_translate3f(-pad, -pad, 0.0f);
		while (gs_effect_loop(f->frame_fx, "Draw"))
			gs_draw_sprite(NULL, 0, (uint32_t)((float)f->cx + 2.0f * pad), (uint32_t)((float)f->cy + 2.0f * pad));
		gs_matrix_pop();
	}

	if (f->chip.text.len) {
		float tw = (float)sbk_text_w(&f->chip), th = (float)sbk_text_h(&f->chip);
		float pw = tw + 7.0f * u, ph = th + 3.0f * u, inset = 3.0f * u;
		bool right = strstr(f->chip_at, "right") != NULL, top = strstr(f->chip_at, "top") != NULL;
		float x = right ? (float)f->cx - inset - pw : inset;
		float y = top ? inset : (float)f->cy - inset - ph;
		struct vec4 fill = f->chip_accent ? l->accent : l->glass;
		sbk_card(f->card_fx, x, y, pw, ph, ph * 0.5f, fill, f->chip_accent ? none : l->glass_line, f->chip_accent ? 0.0f : 1.0f, none, 0.0f);
		sbk_text_draw(&f->chip, x + 3.5f * u, y + 1.5f * u);
	}

	sbk_stage_end(&f->stage);
	struct sbk_anim_out a = sbk_anim_eval(&f->anim);
	sbk_stage_present(&f->stage, a.alpha, a.dx, a.dy);
}

static void frame_show(void *d) { sbk_anim_on_show(&((struct frame *)d)->anim); }
static void frame_enum(void *d, obs_source_enum_proc_t cb, void *p)
{
	struct frame *f = d;
	sbk_text_enum(&f->chip, f->self, cb, p);
}

static obs_properties_t *frame_properties(void *data)
{
	UNUSED_PARAMETER(data);
	obs_properties_t *p = obs_properties_create();
	obs_property_t *asp = obs_properties_add_list(p, "aspect", "Shape", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	for (size_t i = 0; i < N_ASPECTS; i++)
		obs_property_list_add_string(asp, ASPECTS[i].label, ASPECTS[i].id);
	obs_properties_add_float_slider(p, "size", "Size (with a shape chosen)", 0.25, 4.0, 0.05);
	obs_properties_add_int(p, "width", "Width (Custom)", 120, 7680, 2);
	obs_properties_add_int(p, "height", "Height (Custom)", 120, 4320, 2);
	obs_properties_add_float_slider(p, "radius", "Corner radius", 0.0, 120.0, 1.0);
	obs_properties_add_float_slider(p, "weight", "Line weight", 1.0, 24.0, 0.5);
	obs_property_t *st = obs_properties_add_list(p, "style", "Treatment", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(st, "Ring — all the way round", "ring");
	obs_property_list_add_string(st, "Inset — a gap inside the edge", "inset");
	obs_property_list_add_string(st, "Corner brackets", "corner");
	obs_property_list_add_string(st, "Corner brackets, stood off", "corner-out");
	obs_property_list_add_string(st, "Head and foot rules", "edge");
	obs_property_list_add_string(st, "Glow only", "glow");
	obs_property_list_add_string(st, "Double line", "double");
	obs_property_list_add_string(st, "None — just the chip", "none");
	obs_properties_add_float_slider(p, "bracket", "Bracket length", 8.0, 400.0, 2.0);
	obs_properties_add_float_slider(p, "glow", "Glow", 0.0, 80.0, 1.0);
	obs_property_t *ln = obs_properties_add_list(p, "line", "Line colour", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(ln, "Hairline — white at 40%", "hairline");
	obs_property_list_add_string(ln, "Accent", "accent");
	obs_property_list_add_string(ln, "Ink", "ink");
	obs_properties_add_text(p, "label", "Chip (empty for none)", OBS_TEXT_DEFAULT);
	obs_property_t *at = obs_properties_add_list(p, "chip_at", "Chip corner", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(at, "Bottom left", "bottom-left");
	obs_property_list_add_string(at, "Bottom right", "bottom-right");
	obs_property_list_add_string(at, "Top left", "top-left");
	obs_property_list_add_string(at, "Top right", "top-right");
	obs_properties_add_bool(p, "chip_accent", "Chip in the accent");
	sbk_look_props(p, false);
	sbk_anim_props(p);
	return p;
}

static void frame_defaults(obs_data_t *s)
{
	obs_data_set_default_string(s, "aspect", "16x9");
	obs_data_set_default_double(s, "size", 1.0);
	obs_data_set_default_int(s, "width", 640);
	obs_data_set_default_int(s, "height", 360);
	obs_data_set_default_double(s, "radius", 20.0);
	obs_data_set_default_double(s, "weight", 3.0);
	obs_data_set_default_string(s, "style", "ring");
	obs_data_set_default_double(s, "bracket", 56.0);
	obs_data_set_default_double(s, "glow", 0.0);
	obs_data_set_default_string(s, "line", "hairline");
	obs_data_set_default_string(s, "label", "@imswarnil");
	obs_data_set_default_string(s, "chip_at", "bottom-left");
	obs_data_set_default_bool(s, "chip_accent", false);
	sbk_look_defaults(s);
	sbk_anim_defaults(s, "fade");
}

struct obs_source_info sbk_frame_info = {
	.id = "sbk_frame",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
	.get_name = frame_name,
	.create = frame_create,
	.destroy = frame_destroy,
	.update = frame_update,
	.get_defaults = frame_defaults,
	.get_properties = frame_properties,
	.get_width = frame_width,
	.get_height = frame_height,
	.video_tick = frame_tick,
	.video_render = frame_render,
	.show = frame_show,
	.enum_active_sources = frame_enum,
	.icon_type = OBS_ICON_TYPE_CAMERA,
};
