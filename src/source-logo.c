/*  SBK Logo — the mark, looping.

    A channel bug that is alive. Point it at a PNG or pick one of the kit's
    marks, choose how it should move, and it moves for ever without a video
    file, a GIF, or a browser source burning a Chromium process to spin a
    hundred pixels.

    The loops are the ones that read at bug size and do not pull the eye off
    what you are doing: a breath, a pulse of the glow, a slow spin, a dot going
    round, a ring drawing itself on and off, a gentle bob. Everything is driven
    by the clock, so it starts on load and never waits for a click — a Browser
    Source overlay cannot promise that.

    The same source blown up to 400 px is the card a sting lands on, which is
    why the sting transition offers the same mark list.  */

#include "sbk-common.h"
#include "sbk-glyph.h"
#include "sbk-text.h"
#include "sbk-stage.h"
#include "sbk-anim.h"

#include <graphics/image-file.h>

enum logo_loop {
	LL_NONE = 0,
	LL_BREATHE,
	LL_PULSE,
	LL_SPIN,
	LL_ORBIT,
	LL_DRAW,
	LL_BOB,
};

struct logo {
	obs_source_t *self;
	struct sbk_look look;
	struct sbk_anim anim;
	struct sbk_stage stage;
	gs_effect_t *card_fx, *glyph_fx, *ring_fx;

	char *path;
	gs_image_file_t img;
	bool img_loaded;

	enum sbk_glyph glyph;
	enum logo_loop loop;
	enum sbk_surface surf;
	struct vec4 tint;
	bool use_accent;
	float mark_px, speed, weight;
	char *s_caption;
	struct sbk_text caption;

	float t;
	uint32_t cx, cy;
	float box; /* the square the mark and its ring live in */
};

static const char *logo_label(void *u)
{
	UNUSED_PARAMETER(u);
	return obs_module_text("SBK.Logo");
}

static void unload_image(struct logo *g)
{
	if (!g->img_loaded)
		return;
	obs_enter_graphics();
	gs_image_file_free(&g->img);
	obs_leave_graphics();
	g->img_loaded = false;
}

static void load_image(struct logo *g, const char *path)
{
	unload_image(g);
	if (!path || !*path)
		return;
	gs_image_file_init(&g->img, path);
	obs_enter_graphics();
	gs_image_file_init_texture(&g->img);
	obs_leave_graphics();
	g->img_loaded = g->img.loaded;
	if (!g->img_loaded)
		SBK_LOG(LOG_WARNING, "logo: could not read the image at %s", path);
}

static enum logo_loop loop_from(const char *id)
{
	if (!id || astrcmpi(id, "none") == 0)
		return LL_NONE;
	if (astrcmpi(id, "pulse") == 0)
		return LL_PULSE;
	if (astrcmpi(id, "spin") == 0)
		return LL_SPIN;
	if (astrcmpi(id, "orbit") == 0)
		return LL_ORBIT;
	if (astrcmpi(id, "draw") == 0)
		return LL_DRAW;
	if (astrcmpi(id, "bob") == 0)
		return LL_BOB;
	return LL_BREATHE;
}

static void logo_update(void *data, obs_data_t *st)
{
	struct logo *g = data;
	sbk_look_read(&g->look, st);
	sbk_anim_read(&g->anim, st, 4.0f * sbk_u(&g->look));

	g->glyph = sbk_glyph_from(obs_data_get_string(st, "mark"));
	g->loop = loop_from(obs_data_get_string(st, "loop"));
	g->surf = sbk_surface_from(obs_data_get_string(st, "variant"));
	g->mark_px = (float)obs_data_get_int(st, "mark_size");
	g->speed = (float)obs_data_get_double(st, "speed");
	g->weight = (float)obs_data_get_double(st, "weight");
	g->use_accent = obs_data_get_bool(st, "use_accent");
	g->tint = sbk_vec((uint32_t)obs_data_get_int(st, "colour"));

	bfree(g->s_caption);
	g->s_caption = bstrdup(obs_data_get_string(st, "caption"));

	const char *path = obs_data_get_string(st, "image");
	if (!g->path || strcmp(g->path, path) != 0) {
		bfree(g->path);
		g->path = bstrdup(path);
		load_image(g, path);
	}
}

static void *logo_create(obs_data_t *st, obs_source_t *source)
{
	struct logo *g = bzalloc(sizeof(*g));
	g->self = source;
	g->card_fx = sbk_load_effect("effects/card.effect");
	g->glyph_fx = sbk_load_effect("effects/glyph.effect");
	g->ring_fx = sbk_load_effect("effects/ring.effect");
	sbk_stage_init(&g->stage);
	logo_update(g, st);
	sbk_anim_play(&g->anim);
	return g;
}

static void logo_destroy(void *data)
{
	struct logo *g = data;
	unload_image(g);
	sbk_text_free(&g->caption);
	sbk_stage_free(&g->stage);
	sbk_free_effect(&g->card_fx);
	sbk_free_effect(&g->glyph_fx);
	sbk_free_effect(&g->ring_fx);
	bfree(g->s_caption);
	bfree(g->path);
	bfree(g);
}

static struct vec4 logo_colour(const struct logo *g)
{
	return g->use_accent ? g->look.accent : g->tint;
}

static void logo_tick(void *data, float seconds)
{
	struct logo *g = data;
	g->t += seconds;
	sbk_anim_tick(&g->anim, seconds);
	const struct sbk_look *l = &g->look;
	const float u = sbk_u(l);
	const bool bare = g->surf == SBK_SURF_NONE;

	sbk_text_set(&g->caption, g->s_caption, l->face, "SemiBold", (int)(4.0f * u),
		     sbk_surface_ink(g->surf, l, true));

	/* the ring loops need room outside the mark, and a breath or a bob needs
	   a little slack so the edge never clips mid-cycle */
	float ring = (g->loop == LL_ORBIT || g->loop == LL_DRAW) ? 1.52f : 1.14f;
	g->box = g->mark_px * ring;

	float pad = bare ? 0.0f : 4.0f * u;
	float cap_h = g->caption.text.len ? (float)sbk_text_h(&g->caption) + 2.0f * u : 0.0f;
	float w = fmaxf(g->box, (float)sbk_text_w(&g->caption));
	g->cx = (uint32_t)(w + pad * 2.0f + 0.5f);
	g->cy = (uint32_t)(g->box + cap_h + pad * 2.0f + 0.5f);
}

static uint32_t logo_width(void *d) { return ((struct logo *)d)->cx; }
static uint32_t logo_height(void *d) { return ((struct logo *)d)->cy; }

static void draw_image(struct logo *g, float x, float y, float d)
{
	if (!g->img_loaded || !g->img.texture)
		return;
	uint32_t iw = gs_texture_get_width(g->img.texture);
	uint32_t ih = gs_texture_get_height(g->img.texture);
	if (!iw || !ih)
		return;
	/* fit inside the square, keeping the shape of the file */
	float k = fminf(d / (float)iw, d / (float)ih);
	float w = (float)iw * k, h = (float)ih * k;

	gs_effect_t *fx = obs_get_base_effect(OBS_EFFECT_DEFAULT);
	gs_eparam_t *p = gs_effect_get_param_by_name(fx, "image");
	gs_effect_set_texture_srgb(p, g->img.texture);
	gs_matrix_push();
	gs_matrix_translate3f(x + (d - w) * 0.5f, y + (d - h) * 0.5f, 0.0f);
	while (gs_effect_loop(fx, "Draw"))
		gs_draw_sprite(g->img.texture, 0, (uint32_t)w, (uint32_t)h);
	gs_matrix_pop();
}

static void logo_render(void *data, gs_effect_t *unused)
{
	UNUSED_PARAMETER(unused);
	struct logo *g = data;
	if (!g->card_fx || !g->glyph_fx || !sbk_stage_begin(&g->stage, g->cx, g->cy))
		return;
	const struct sbk_look *l = &g->look;
	const float u = sbk_u(l);
	const bool bare = g->surf == SBK_SURF_NONE;
	const float pad = bare ? 0.0f : 4.0f * u;
	const struct vec4 col = logo_colour(g);

	sbk_surface_draw(g->card_fx, g->surf, l, 0, 0, (float)g->cx, (float)g->cy, 4.0f * u);

	const float cycle = fmaxf(0.2f, g->speed);
	const float phase = fmodf(g->t, cycle) / cycle;         /* 0..1, once per cycle */
	const float wave = 0.5f - 0.5f * cosf(phase * 6.2831853f); /* 0..1..0, smooth */

	float bx = ((float)g->cx - g->box) * 0.5f;
	float by = pad;
	float cxp = bx + g->box * 0.5f, cyp = by + g->box * 0.5f;

	/* the ring loops sit outside the mark and do not move it */
	if (g->loop == LL_ORBIT) {
		float d = g->box * 0.94f;
		sbk_ring(g->ring_fx, cxp - d * 0.5f, cyp - d * 0.5f, d, fmaxf(1.5f, 0.6f * u), 1.0f,
			 sbk_alpha(col, 0.16f), sbk_alpha(col, 0.16f), true);
		float a = phase * 6.2831853f - 1.5707963f;
		float r = d * 0.5f, dot = fmaxf(3.0f, 1.6f * u);
		sbk_dot(g->card_fx, cxp + cosf(a) * r - dot * 0.5f, cyp + sinf(a) * r - dot * 0.5f, dot, col,
			dot * 2.0f, 0.55f);
	} else if (g->loop == LL_DRAW) {
		float d = g->box * 0.94f;
		/* draws on for the first half of the cycle and off for the second */
		float k = phase < 0.5f ? sbk_ease_out(phase * 2.0f) : 1.0f - sbk_ease_out((phase - 0.5f) * 2.0f);
		sbk_ring(g->ring_fx, cxp - d * 0.5f, cyp - d * 0.5f, d, fmaxf(1.5f, 0.7f * u), k,
			 sbk_alpha(col, 0.12f), col, true);
	}

	float scale = 1.0f, bob = 0.0f, spin = 0.0f;
	struct vec4 mark_col = col;
	switch (g->loop) {
	case LL_BREATHE:
		scale = 0.94f + 0.06f * wave;
		break;
	case LL_PULSE:
		mark_col = sbk_alpha(col, 0.62f + 0.38f * wave);
		break;
	case LL_SPIN:
		spin = phase * 6.2831853f;
		break;
	case LL_BOB:
		bob = (wave - 0.5f) * g->mark_px * 0.10f;
		break;
	default:
		break;
	}

	float md = g->mark_px;
	gs_matrix_push();
	gs_matrix_translate3f(cxp, cyp + bob, 0.0f);
	if (spin != 0.0f)
		gs_matrix_rotaa4f(0.0f, 0.0f, 1.0f, spin);
	if (scale != 1.0f)
		gs_matrix_scale3f(scale, scale, 1.0f);
	if (g->img_loaded)
		draw_image(g, -md * 0.5f, -md * 0.5f, md);
	else
		sbk_glyph_draw(g->glyph_fx, g->glyph, -md * 0.5f, -md * 0.5f, md, mark_col,
			       fmaxf(1.5f, g->weight));
	gs_matrix_pop();

	if (g->caption.text.len)
		sbk_text_draw(&g->caption, ((float)g->cx - (float)sbk_text_w(&g->caption)) * 0.5f,
			      by + g->box + 2.0f * u);

	sbk_stage_end(&g->stage);
	sbk_stage_present_anim(&g->stage, sbk_anim_eval(&g->anim));
}

static void logo_show(void *d) { sbk_anim_on_show(&((struct logo *)d)->anim); }
static void logo_enum(void *d, obs_source_enum_proc_t cb, void *p)
{
	struct logo *g = d;
	sbk_text_enum(&g->caption, g->self, cb, p);
}

static obs_properties_t *logo_properties(void *data)
{
	UNUSED_PARAMETER(data);
	obs_properties_t *p = obs_properties_create();

	obs_properties_add_path(p, "image", "Image (a PNG with transparency; empty for a drawn mark)",
				OBS_PATH_FILE, "Images (*.png *.jpg *.jpeg *.webp);;All files (*.*)", NULL);
	obs_property_t *m = obs_properties_add_list(p, "mark", "Drawn mark", OBS_COMBO_TYPE_LIST,
						    OBS_COMBO_FORMAT_STRING);
	sbk_glyph_list(m);

	obs_property_t *lp = obs_properties_add_list(p, "loop", "Loop", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(lp, "Still", "none");
	obs_property_list_add_string(lp, "Breathe — a slow change of size", "breathe");
	obs_property_list_add_string(lp, "Pulse — a slow change of strength", "pulse");
	obs_property_list_add_string(lp, "Spin", "spin");
	obs_property_list_add_string(lp, "Orbit — a dot going round it", "orbit");
	obs_property_list_add_string(lp, "Draw — a ring drawing itself on and off", "draw");
	obs_property_list_add_string(lp, "Bob — up and down", "bob");

	obs_properties_add_float_slider(p, "speed", "One cycle takes (seconds)", 0.4, 12.0, 0.1);
	obs_properties_add_int(p, "mark_size", "Mark size (px)", 24, 720, 4);
	obs_properties_add_float_slider(p, "weight", "Stroke width (px)", 1.0, 16.0, 0.5);
	obs_properties_add_bool(p, "use_accent", "Use the Look's accent");
	obs_properties_add_color_alpha(p, "colour", "Colour");
	obs_properties_add_text(p, "caption", "Caption (empty for none)", OBS_TEXT_DEFAULT);
	sbk_surface_list(p, "variant", "Variant");
	obs_properties_add_text(p, "hint",
				"It starts on load, not on a click, so it is already moving the first time "
				"the scene goes out.",
				OBS_TEXT_INFO);
	sbk_look_props(p, true);
	sbk_anim_props(p);
	return p;
}

static void logo_defaults(obs_data_t *st)
{
	obs_data_set_default_string(st, "image", "");
	obs_data_set_default_string(st, "mark", "ring");
	obs_data_set_default_string(st, "loop", "orbit");
	obs_data_set_default_double(st, "speed", 3.2);
	obs_data_set_default_int(st, "mark_size", 96);
	obs_data_set_default_double(st, "weight", 5.0);
	obs_data_set_default_bool(st, "use_accent", true);
	obs_data_set_default_int(st, "colour", (int)SBK_ACCENT);
	obs_data_set_default_string(st, "caption", "");
	obs_data_set_default_string(st, "variant", "none");
	sbk_look_defaults(st);
	sbk_anim_defaults(st, "pop");
}

struct obs_source_info sbk_logo_info = {
	.id = "sbk_logo",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
	.get_name = logo_label,
	.create = logo_create,
	.destroy = logo_destroy,
	.update = logo_update,
	.get_defaults = logo_defaults,
	.get_properties = logo_properties,
	.get_width = logo_width,
	.get_height = logo_height,
	.video_tick = logo_tick,
	.video_render = logo_render,
	.show = logo_show,
	.enum_active_sources = logo_enum,
	.icon_type = OBS_ICON_TYPE_IMAGE,
};
