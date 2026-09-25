#pragma once

#include <obs-module.h>
#include <graphics/graphics.h>
#include <graphics/vec2.h>
#include <graphics/vec4.h>
#include <util/dstr.h>
#include <util/platform.h>
#include <math.h>
#include <string.h>

/*  Swarnil Broadcast Kit — the shared foundation every source is built on.

    One palette, because an overlay always sits ON VIDEO: white ink in three
    strengths over a dark glass, and one accent, the tally light. Every length
    is a multiple of the unit, and the unit is 4px × the source's scale, so
    turning one slider grows a component uniformly — type, padding, radius.

    Colours are 0xAABBGGRR, the order OBS's colour picker hands back and the
    order text_ft2 wants for color1/color2.  */

#define SBK_LOG(level, fmt, ...) blog(level, "[sbk] " fmt, ##__VA_ARGS__)

/* ---- tokens -------------------------------------------------------------- */
#define SBK_ACCENT 0xFF3F27F5u      /* #f5273f — a tally lamp is red */
#define SBK_REC 0xFF335CFFu         /* #ff5c33 */
#define SBK_OK 0xFF6ACF3Fu          /* #3fcf6a */
#define SBK_OFF 0xFFA3A3A3u         /* the neutral ramp at 68% */
#define SBK_INK 0xFFFFFFFFu
#define SBK_INK_DIM 0xC7FFFFFFu     /* 78% */
#define SBK_INK_FAINT 0x8CFFFFFFu   /* 55% */
#define SBK_GLASS 0xB80C0C0Cu       /* rgb(12 12 12 / 72%) */
#define SBK_GLASS_SOFT 0x800C0C0Cu  /* 50% */
#define SBK_GLASS_LINE 0x24FFFFFFu  /* 14% white hairline */
#define SBK_WASH 0x1AFFFFFFu        /* 10% white — a chip's fill */
#define SBK_SOLID 0xFF1B1B1Bu       /* the neutral ramp at 12% */
#define SBK_SOLID_SOFT 0xFF2B2B2Bu
#define SBK_LIGHT 0xD6FFFFFFu       /* a pale desk: white at 84% */
#define SBK_LIGHT_SOFT 0x9EFFFFFFu
#define SBK_LIGHT_LINE 0x1A000000u
#define SBK_LIGHT_INK 0xFF1B1B1Bu
#define SBK_LIGHT_INK_DIM 0xBD000000u
#define SBK_LIGHT_INK_FAINT 0x80000000u
#define SBK_LIGHT_WASH 0x14000000u

#define SBK_FONT "Geist"
#define SBK_FONT_MONO "Geist Mono"

#define SBK_ENTER_SECS 0.6f
#define SBK_PULSE_SECS 1.6f

/* ---- colour helpers ------------------------------------------------------ */

static inline struct vec4 sbk_vec(uint32_t c)
{
	struct vec4 v;
	vec4_set(&v, (float)(c & 0xFF) / 255.0f, (float)((c >> 8) & 0xFF) / 255.0f,
		 (float)((c >> 16) & 0xFF) / 255.0f, (float)((c >> 24) & 0xFF) / 255.0f);
	return v;
}

static inline uint32_t sbk_int(struct vec4 c)
{
	return ((uint32_t)(c.w * 255.0f + 0.5f) << 24) | ((uint32_t)(c.z * 255.0f + 0.5f) << 16) |
	       ((uint32_t)(c.y * 255.0f + 0.5f) << 8) | (uint32_t)(c.x * 255.0f + 0.5f);
}

/* the same colour at a different opacity (multiplied, so a 78% ink stays 78% of a) */
static inline struct vec4 sbk_alpha(struct vec4 c, float a)
{
	c.w *= a;
	return c;
}

static inline float sbk_clampf(float v, float lo, float hi)
{
	return v < lo ? lo : v > hi ? hi : v;
}

static inline float sbk_ease_out(float t)
{
	float u = 1.0f - sbk_clampf(t, 0.0f, 1.0f);
	return 1.0f - u * u * u;
}

/* ---- effects ------------------------------------------------------------- */

static inline gs_effect_t *sbk_load_effect(const char *file)
{
	char *path = obs_module_file(file);
	if (!path) {
		SBK_LOG(LOG_ERROR, "%s not found in the plugin bundle", file);
		return NULL;
	}
	obs_enter_graphics();
	char *errors = NULL;
	gs_effect_t *fx = gs_effect_create_from_file(path, &errors);
	obs_leave_graphics();
	if (!fx)
		SBK_LOG(LOG_ERROR, "%s failed to compile: %s", file, errors ? errors : "(no message)");
	bfree(errors);
	bfree(path);
	return fx;
}

static inline void sbk_free_effect(gs_effect_t **fx)
{
	if (!*fx)
		return;
	obs_enter_graphics();
	gs_effect_destroy(*fx);
	obs_leave_graphics();
	*fx = NULL;
}

/* set a param only if the shader declares it, so one effect can serve many callers */
static inline void sbk_set_vec4(gs_effect_t *fx, const char *name, const struct vec4 *v)
{
	gs_eparam_t *p = gs_effect_get_param_by_name(fx, name);
	if (p)
		gs_effect_set_vec4(p, v);
}
static inline void sbk_set_vec2(gs_effect_t *fx, const char *name, float x, float y)
{
	struct vec2 v;
	vec2_set(&v, x, y);
	gs_eparam_t *p = gs_effect_get_param_by_name(fx, name);
	if (p)
		gs_effect_set_vec2(p, &v);
}
static inline void sbk_set_float(gs_effect_t *fx, const char *name, float f)
{
	gs_eparam_t *p = gs_effect_get_param_by_name(fx, name);
	if (p)
		gs_effect_set_float(p, f);
}

/* ---- the card: one rounded box with a fill, a hairline and a glow ---------- */

/* Draws card.effect at (x, y) sized w × h. The quad is padded by the glow so
   the falloff has room; the shader accounts for the padding. line_w = 0 draws
   no hairline; glow_size = 0 draws no glow; grad 0 is a flat fill, 1 vertical,
   2 diagonal, 3 horizontal. Blending is whatever the caller set. */
static inline void sbk_card_ex(gs_effect_t *fx, float x, float y, float w, float h, float radius,
				 struct vec4 fill, struct vec4 fill2, float grad, struct vec4 line,
				 float line_w, struct vec4 glow, float glow_size)
{
	if (!fx || w < 1.0f || h < 1.0f)
		return;
	float pad = glow_size > 0.5f ? glow_size * 2.0f : 0.0f;
	sbk_set_vec2(fx, "size", w, h);
	sbk_set_float(fx, "pad", pad);
	sbk_set_float(fx, "radius", radius);
	sbk_set_vec4(fx, "fill", &fill);
	sbk_set_vec4(fx, "fill2", &fill2);
	sbk_set_float(fx, "grad", grad);
	sbk_set_vec4(fx, "line", &line);
	sbk_set_float(fx, "line_width", line_w);
	sbk_set_vec4(fx, "glow", &glow);
	sbk_set_float(fx, "glow_size", glow_size);
	gs_matrix_push();
	gs_matrix_translate3f(x - pad, y - pad, 0.0f);
	while (gs_effect_loop(fx, "Draw"))
		gs_draw_sprite(NULL, 0, (uint32_t)(w + 2.0f * pad + 0.5f), (uint32_t)(h + 2.0f * pad + 0.5f));
	gs_matrix_pop();
}

#define SBK_NONE ((struct vec4){{{0.0f, 0.0f, 0.0f, 0.0f}}})

static inline void sbk_card(gs_effect_t *fx, float x, float y, float w, float h, float radius,
			      struct vec4 fill, struct vec4 line, float line_w, struct vec4 glow,
			      float glow_size)
{
	sbk_card_ex(fx, x, y, w, h, radius, fill, fill, 0.0f, line, line_w, glow, glow_size);
}

/* a plain filled box, the call most components actually want */
static inline void sbk_fill(gs_effect_t *fx, float x, float y, float w, float h, float radius,
			      struct vec4 color)
{
	struct vec4 none = SBK_NONE;
	sbk_card_ex(fx, x, y, w, h, radius, color, color, 0.0f, none, 0.0f, none, 0.0f);
}

/* the recording-light dot: a filled circle, glowing when lit */
static inline void sbk_dot(gs_effect_t *fx, float x, float y, float d, struct vec4 color, float glow_size,
			     float glow_alpha)
{
	struct vec4 none = SBK_NONE;
	struct vec4 glow = sbk_alpha(color, glow_alpha);
	sbk_card_ex(fx, x, y, d, d, d * 0.5f, color, color, 0.0f, none, 0.0f, glow, glow_size);
}

/* ---- the look every component shares -------------------------------------- */

enum sbk_tone { SBK_TONE_GLASS = 0, SBK_TONE_SOLID, SBK_TONE_LIGHT };

struct sbk_look {
	enum sbk_tone tone;
	float scale;          /* 1 = sized for 1080p */
	struct vec4 accent, on_accent;
	struct vec4 ink, ink_dim, ink_faint;
	struct vec4 glass, glass_soft, glass_line, wash;
	float blur_hint;      /* unused for now: OBS has no backdrop blur; kept for parity */
	char face[96], mono[96];
};

/* the unit: 4px × scale — every length in a component is u × something */
static inline float sbk_u(const struct sbk_look *l)
{
	return 4.0f * l->scale;
}

static inline void sbk_look_defaults(obs_data_t *s)
{
	obs_data_set_default_int(s, "accent", (long long)SBK_ACCENT);
	obs_data_set_default_double(s, "scale", 1.0);
	obs_data_set_default_string(s, "tone", "glass");

	obs_data_t *f = obs_data_create();
	obs_data_set_string(f, "face", SBK_FONT);
	obs_data_set_string(f, "style", "Regular");
	obs_data_set_int(f, "size", 32);
	obs_data_set_int(f, "flags", 0);
	obs_data_set_default_obj(s, "font", f);
	obs_data_release(f);

	obs_data_t *m = obs_data_create();
	obs_data_set_string(m, "face", SBK_FONT_MONO);
	obs_data_set_string(m, "style", "Regular");
	obs_data_set_int(m, "size", 32);
	obs_data_set_int(m, "flags", 0);
	obs_data_set_default_obj(s, "mono", m);
	obs_data_release(m);
}

static inline void sbk_look_read(struct sbk_look *l, obs_data_t *s)
{
	const char *tone = obs_data_get_string(s, "tone");
	l->tone = astrcmpi(tone, "solid") == 0 ? SBK_TONE_SOLID
		  : astrcmpi(tone, "light") == 0 ? SBK_TONE_LIGHT
						  : SBK_TONE_GLASS;
	l->scale = sbk_clampf((float)obs_data_get_double(s, "scale"), 0.25f, 4.0f);
	if (l->scale <= 0.0f)
		l->scale = 1.0f;
	l->accent = sbk_vec((uint32_t)obs_data_get_int(s, "accent"));
	l->on_accent = sbk_vec(SBK_INK);

	switch (l->tone) {
	case SBK_TONE_SOLID:
		l->glass = sbk_vec(SBK_SOLID);
		l->glass_soft = sbk_vec(SBK_SOLID_SOFT);
		l->glass_line = sbk_vec(SBK_GLASS_LINE);
		l->wash = sbk_vec(SBK_WASH);
		l->ink = sbk_vec(SBK_INK);
		l->ink_dim = sbk_vec(SBK_INK_DIM);
		l->ink_faint = sbk_vec(SBK_INK_FAINT);
		break;
	case SBK_TONE_LIGHT:
		l->glass = sbk_vec(SBK_LIGHT);
		l->glass_soft = sbk_vec(SBK_LIGHT_SOFT);
		l->glass_line = sbk_vec(SBK_LIGHT_LINE);
		l->wash = sbk_vec(SBK_LIGHT_WASH);
		l->ink = sbk_vec(SBK_LIGHT_INK);
		l->ink_dim = sbk_vec(SBK_LIGHT_INK_DIM);
		l->ink_faint = sbk_vec(SBK_LIGHT_INK_FAINT);
		break;
	default:
		l->glass = sbk_vec(SBK_GLASS);
		l->glass_soft = sbk_vec(SBK_GLASS_SOFT);
		l->glass_line = sbk_vec(SBK_GLASS_LINE);
		l->wash = sbk_vec(SBK_WASH);
		l->ink = sbk_vec(SBK_INK);
		l->ink_dim = sbk_vec(SBK_INK_DIM);
		l->ink_faint = sbk_vec(SBK_INK_FAINT);
		break;
	}

	obs_data_t *f = obs_data_get_obj(s, "font");
	const char *face = f ? obs_data_get_string(f, "face") : NULL;
	snprintf(l->face, sizeof(l->face), "%s", face && *face ? face : SBK_FONT);
	obs_data_release(f);

	obs_data_t *m = obs_data_get_obj(s, "mono");
	const char *mface = m ? obs_data_get_string(m, "face") : NULL;
	snprintf(l->mono, sizeof(l->mono), "%s", mface && *mface ? mface : SBK_FONT_MONO);
	obs_data_release(m);
}

/* ---- surfaces ------------------------------------------------------------- */

/* The box a component sits in. Every source with a "shape" or "variant" list
   resolves it through this, so a pill is the same pill everywhere and adding a
   surface once adds it to all of them. */
enum sbk_surface { SBK_SURF_CARD = 0, SBK_SURF_PILL, SBK_SURF_OUTLINE, SBK_SURF_ACCENT, SBK_SURF_NONE };

static inline enum sbk_surface sbk_surface_from(const char *id)
{
	if (!id)
		return SBK_SURF_CARD;
	if (astrcmpi(id, "pill") == 0)
		return SBK_SURF_PILL;
	if (astrcmpi(id, "outline") == 0)
		return SBK_SURF_OUTLINE;
	if (astrcmpi(id, "accent") == 0)
		return SBK_SURF_ACCENT;
	if (astrcmpi(id, "none") == 0 || astrcmpi(id, "plain") == 0 || astrcmpi(id, "minimal") == 0)
		return SBK_SURF_NONE;
	return SBK_SURF_CARD;
}

/* Draw it. `radius` is what a card uses; a pill ignores it and rounds fully. */
static inline void sbk_surface_draw(gs_effect_t *fx, enum sbk_surface surf, const struct sbk_look *l,
				      float x, float y, float w, float h, float radius)
{
	struct vec4 none = SBK_NONE;
	switch (surf) {
	case SBK_SURF_NONE:
		break;
	case SBK_SURF_PILL:
		sbk_card_ex(fx, x, y, w, h, h * 0.5f, l->glass, l->glass, 0.0f, l->glass_line, 1.0f, none, 0.0f);
		break;
	case SBK_SURF_OUTLINE:
		sbk_card_ex(fx, x, y, w, h, radius, none, none, 0.0f, sbk_alpha(l->ink, 0.45f), 1.5f, none, 0.0f);
		break;
	case SBK_SURF_ACCENT:
		sbk_card_ex(fx, x, y, w, h, radius, l->accent, l->accent, 0.0f, none, 0.0f, none, 0.0f);
		break;
	default:
		sbk_card_ex(fx, x, y, w, h, radius, l->glass, l->glass, 0.0f, l->glass_line, 1.0f, none, 0.0f);
		break;
	}
}

/* the ink that reads on that surface */
static inline struct vec4 sbk_surface_ink(enum sbk_surface surf, const struct sbk_look *l, bool dim)
{
	if (surf == SBK_SURF_ACCENT)
		return dim ? sbk_alpha(l->on_accent, 0.8f) : l->on_accent;
	return dim ? l->ink_dim : l->ink;
}

static inline void sbk_surface_list(obs_properties_t *g, const char *key, const char *label)
{
	obs_property_t *v = obs_properties_add_list(g, key, label, OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(v, "Card", "card");
	obs_property_list_add_string(v, "Pill", "pill");
	obs_property_list_add_string(v, "Outline", "outline");
	obs_property_list_add_string(v, "Accent", "accent");
	obs_property_list_add_string(v, "None — straight on the video", "none");
}

/* The "Look" group at the foot of every properties dialog. with_mono adds the
   second font picker for components that set figures in the mono face. */
static inline void sbk_look_props(obs_properties_t *props, bool with_mono)
{
	obs_properties_t *g = obs_properties_create();
	obs_properties_add_color(g, "accent", "Accent");
	obs_properties_add_float_slider(g, "scale", "Scale", 0.5, 3.0, 0.05);
	obs_property_t *tone = obs_properties_add_list(g, "tone", "Tone", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(tone, "Glass — dark, translucent", "glass");
	obs_property_list_add_string(tone, "Solid — broadcast black", "solid");
	obs_property_list_add_string(tone, "Light — a pale desk", "light");
	obs_property_t *font = obs_properties_add_font(g, "font", "Font");
	obs_property_set_long_description(font, "Only the family is used; the kit sets the weights. Geist is installed by build.command.");
	if (with_mono)
		obs_properties_add_font(g, "mono", "Figures font");
	obs_properties_add_group(props, "look", "Look", OBS_GROUP_NORMAL, g);
}
