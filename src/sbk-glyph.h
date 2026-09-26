/*  The mark set.

    Fifteen small marks, shared by the social bar, the prompt and the logo bug.
    They are generic on purpose: a play triangle, a camera, an at-sign, a chat
    bubble. None of them is anybody's logo. A platform is identified by its
    colour and its name in type, which is honest, redistributable under MIT, and
    does not go stale the week a company redraws its mark.

    Everything is drawn by glyph.effect from a distance field, so one mark is
    crisp at 24 px in a corner and at 400 px on a title card.  */

#pragma once

#include "sbk-common.h"

enum sbk_glyph {
	SBK_GLYPH_NONE = 0,
	SBK_GLYPH_PLAY,
	SBK_GLYPH_CAMERA,
	SBK_GLYPH_AT,
	SBK_GLYPH_CHAT,
	SBK_GLYPH_HEART,
	SBK_GLYPH_BELL,
	SBK_GLYPH_STAR,
	SBK_GLYPH_SHARE,
	SBK_GLYPH_GLOBE,
	SBK_GLYPH_CODE,
	SBK_GLYPH_PERSON,
	SBK_GLYPH_BOOKMARK,
	SBK_GLYPH_PLUS,
	SBK_GLYPH_RING,
	SBK_GLYPH_ARROW,
};

struct sbk_glyph_def {
	const char *id;
	const char *label;
	enum sbk_glyph g;
};

static const struct sbk_glyph_def SBK_GLYPHS[] = {
	{"none", "None", SBK_GLYPH_NONE},
	{"play", "Play", SBK_GLYPH_PLAY},
	{"camera", "Camera", SBK_GLYPH_CAMERA},
	{"at", "At sign", SBK_GLYPH_AT},
	{"chat", "Chat bubble", SBK_GLYPH_CHAT},
	{"heart", "Heart", SBK_GLYPH_HEART},
	{"bell", "Bell", SBK_GLYPH_BELL},
	{"star", "Star", SBK_GLYPH_STAR},
	{"share", "Share", SBK_GLYPH_SHARE},
	{"globe", "Globe", SBK_GLYPH_GLOBE},
	{"code", "Code", SBK_GLYPH_CODE},
	{"person", "Person", SBK_GLYPH_PERSON},
	{"bookmark", "Bookmark", SBK_GLYPH_BOOKMARK},
	{"plus", "Plus", SBK_GLYPH_PLUS},
	{"ring", "Recording ring", SBK_GLYPH_RING},
	{"arrow", "Arrow", SBK_GLYPH_ARROW},
};
#define SBK_N_GLYPHS (sizeof(SBK_GLYPHS) / sizeof(SBK_GLYPHS[0]))

static inline enum sbk_glyph sbk_glyph_from(const char *id)
{
	if (!id || !*id)
		return SBK_GLYPH_NONE;
	for (size_t i = 0; i < SBK_N_GLYPHS; i++)
		if (astrcmpi(id, SBK_GLYPHS[i].id) == 0)
			return SBK_GLYPHS[i].g;
	return SBK_GLYPH_NONE;
}

static inline void sbk_glyph_list(obs_property_t *p)
{
	for (size_t i = 0; i < SBK_N_GLYPHS; i++)
		obs_property_list_add_string(p, SBK_GLYPHS[i].label, SBK_GLYPHS[i].id);
}

/* (x, y) is the top-left of a square d wide. weight is the stroke width for the
   outline marks; the solid ones ignore it. */
static inline void sbk_glyph_draw(gs_effect_t *fx, enum sbk_glyph g, float x, float y, float d,
				  struct vec4 color, float weight)
{
	if (!fx || g == SBK_GLYPH_NONE || d < 1.0f)
		return;
	sbk_set_vec2(fx, "size", d, d);
	sbk_set_float(fx, "mark", (float)g);
	sbk_set_float(fx, "weight", weight);
	sbk_set_vec4(fx, "color", &color);

	gs_matrix_push();
	gs_matrix_translate3f(x, y, 0.0f);
	gs_technique_t *tech = gs_effect_get_technique(fx, "Draw");
	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);
	gs_draw_sprite(NULL, 0, (uint32_t)d, (uint32_t)d);
	gs_technique_end_pass(tech);
	gs_technique_end(tech);
	gs_matrix_pop();
}

/*  The platforms.

    A preset is a colour, a name and the closest generic mark — never a logo.
    Picking one fills the three fields in; every one of them is still yours to
    change afterwards, which is the point of storing them as ordinary settings
    rather than as a mode the source switches on.  */
struct sbk_platform {
	const char *id;
	const char *label;
	const char *name;
	const char *glyph;
	uint32_t colour; /* ABGR, as everything in the kit is */
};

static const struct sbk_platform SBK_PLATFORMS[] = {
	{"youtube", "YouTube", "YouTube", "play", 0xFF0000FFu},
	{"instagram", "Instagram", "Instagram", "camera", 0xFF5144E4u},
	{"x", "X", "X", "at", 0xFF1A1A1Au},
	{"linkedin", "LinkedIn", "LinkedIn", "person", 0xFFB1770Au},
	{"github", "GitHub", "GitHub", "code", 0xFF24211Fu},
	{"twitch", "Twitch", "Twitch", "chat", 0xFFF46B92u},
	{"tiktok", "TikTok", "TikTok", "star", 0xFF45204Eu},
	{"discord", "Discord", "Discord", "chat", 0xFFF08B58u},
	{"web", "Website", "imswarnil.com", "globe", 0xFF3F27F5u},
	{"news", "Newsletter", "Newsletter", "bookmark", 0xFF3F27F5u},
	{"custom", "Custom — set it yourself", "", "", 0u},
};
#define SBK_N_PLATFORMS (sizeof(SBK_PLATFORMS) / sizeof(SBK_PLATFORMS[0]))

static inline const struct sbk_platform *sbk_platform_from(const char *id)
{
	if (!id || !*id)
		return NULL;
	for (size_t i = 0; i < SBK_N_PLATFORMS; i++)
		if (astrcmpi(id, SBK_PLATFORMS[i].id) == 0)
			return &SBK_PLATFORMS[i];
	return NULL;
}

static inline void sbk_platform_list(obs_property_t *p)
{
	for (size_t i = 0; i < SBK_N_PLATFORMS; i++)
		obs_property_list_add_string(p, SBK_PLATFORMS[i].label, SBK_PLATFORMS[i].id);
}
