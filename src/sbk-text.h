#pragma once

#include <ctype.h>
#include "sbk-common.h"

/*  A line of type, as a child text_ft2 source.

    The type is OBS's own FreeType text — real, hinted, using whatever font is
    installed — rather than something drawn by hand. Each line is a private
    source that re-rasterises only when its text or look actually changes, so
    a component may call sbk_text_set every frame without cost.  */

struct sbk_text {
	obs_source_t *src;
	struct dstr text;
	char face[96], style[32];
	int size;
	uint32_t color;
	int width; /* custom_width for wrapping, 0 = none */
	bool shadow;
};

/* Upper-case in place, ASCII only — an eyebrow, a label, a tag. */
static inline void sbk_caps(struct dstr *s)
{
	for (size_t i = 0; i < s->len; i++)
		s->array[i] = (char)toupper((unsigned char)s->array[i]);
}

/* Tracking a caps label. FreeType text has no letter-spacing, and the only way
   to fake it — thin spaces between the letters — needs U+2009, which Geist
   does not carry (it rendered as boxes). So a tracked label is a plain caps
   label; the call stays so the intent is on record if a font with the glyph
   is ever the default. */
static inline void sbk_track(struct dstr *s)
{
	UNUSED_PARAMETER(s);
}

/* shadow = a drop shadow behind the glyphs, for type that sits straight on the
   video with no card under it. Off everywhere else: on glass it only muddies. */
static inline void sbk_text_set_full(struct sbk_text *t, const char *text, const char *face,
				       const char *style, int size, struct vec4 color, int wrap_width,
				       bool shadow)
{
	uint32_t c = sbk_int(color);
	if (!text)
		text = "";
	if (t->src && dstr_cmp(&t->text, text) == 0 && strcmp(t->face, face) == 0 &&
	    strcmp(t->style, style) == 0 && t->size == size && t->color == c && t->width == wrap_width &&
	    t->shadow == shadow)
		return;

	dstr_copy(&t->text, text);
	snprintf(t->face, sizeof(t->face), "%s", face);
	snprintf(t->style, sizeof(t->style), "%s", style);
	t->size = size;
	t->color = c;
	t->width = wrap_width;
	t->shadow = shadow;

	obs_data_t *s = obs_data_create();
	obs_data_t *f = obs_data_create();
	obs_data_set_string(f, "face", face);
	obs_data_set_int(f, "size", size);
	obs_data_set_int(f, "flags", 0);
	obs_data_set_string(f, "style", style);
	obs_data_set_obj(s, "font", f);
	obs_data_release(f);
	obs_data_set_string(s, "text", text);
	obs_data_set_int(s, "color1", c);
	obs_data_set_int(s, "color2", c);
	obs_data_set_bool(s, "outline", false);
	obs_data_set_bool(s, "drop_shadow", shadow);
	obs_data_set_bool(s, "antialiasing", true);
	if (wrap_width > 0) {
		obs_data_set_bool(s, "word_wrap", true);
		obs_data_set_int(s, "custom_width", wrap_width);
	} else {
		obs_data_set_bool(s, "word_wrap", false);
		obs_data_set_int(s, "custom_width", 0);
	}

	if (!t->src)
		t->src = obs_source_create_private("text_ft2_source", NULL, s);
	else
		obs_source_update(t->src, s);
	obs_data_release(s);
}

static inline void sbk_text_set_ex(struct sbk_text *t, const char *text, const char *face, const char *style,
				     int size, struct vec4 color, int wrap_width)
{
	sbk_text_set_full(t, text, face, style, size, color, wrap_width, false);
}

static inline void sbk_text_set(struct sbk_text *t, const char *text, const char *face, const char *style,
				  int size, struct vec4 color)
{
	sbk_text_set_full(t, text, face, style, size, color, 0, false);
}

static inline uint32_t sbk_text_w(const struct sbk_text *t)
{
	return t->src ? obs_source_get_width(t->src) : 0;
}
static inline uint32_t sbk_text_h(const struct sbk_text *t)
{
	return t->src ? obs_source_get_height(t->src) : 0;
}

static inline void sbk_text_draw(struct sbk_text *t, float x, float y)
{
	if (!t->src || !t->text.len)
		return;
	gs_matrix_push();
	gs_matrix_translate3f(floorf(x), floorf(y), 0.0f);
	obs_source_video_render(t->src);
	gs_matrix_pop();
}

static inline void sbk_text_free(struct sbk_text *t)
{
	if (t->src) {
		obs_source_release(t->src);
		t->src = NULL;
	}
	dstr_free(&t->text);
}

/* for enum_active_sources, so OBS knows the children belong to the parent */
static inline void sbk_text_enum(struct sbk_text *t, obs_source_t *parent, obs_source_enum_proc_t cb,
				   void *param)
{
	if (t->src)
		cb(parent, t->src, param);
}
