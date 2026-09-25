#pragma once

#include "sbk-common.h"

/*  The stage: a component draws itself into an offscreen texture of its own
    size, then that texture is put on the canvas with an opacity and an offset.

    Two things need this. Enter animations fade and slide the whole component,
    text children included, and a text_ft2 child has no opacity of its own. And
    the ticker has to clip: a render target is the one clip that survives the
    scene item's transform.

    The offscreen pass writes premultiplied alpha (separate blend, ONE for the
    alpha channel) so translucent glass over a cleared target keeps the right
    coverage, and the present pass blends ONE / INVSRCALPHA to match.  */

struct sbk_stage {
	gs_texrender_t *tr;
	gs_effect_t *blit;
	uint32_t cx, cy;
};

static inline void sbk_stage_init(struct sbk_stage *st)
{
	st->blit = sbk_load_effect("effects/blit.effect");
}

static inline void sbk_stage_free(struct sbk_stage *st)
{
	obs_enter_graphics();
	if (st->tr)
		gs_texrender_destroy(st->tr);
	st->tr = NULL;
	if (st->blit)
		gs_effect_destroy(st->blit);
	st->blit = NULL;
	obs_leave_graphics();
}

/* Begin drawing into a cx × cy stage. Returns false if there is nothing to
   draw into; the caller then skips its drawing and the present. */
static inline bool sbk_stage_begin(struct sbk_stage *st, uint32_t cx, uint32_t cy)
{
	if (!cx || !cy)
		return false;
	if (!st->tr)
		st->tr = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	gs_texrender_reset(st->tr);
	if (!gs_texrender_begin(st->tr, cx, cy))
		return false;
	st->cx = cx;
	st->cy = cy;
	struct vec4 clear = {{{0.0f, 0.0f, 0.0f, 0.0f}}};
	gs_clear(GS_CLEAR_COLOR, &clear, 0.0f, 0);
	gs_ortho(0.0f, (float)cx, 0.0f, (float)cy, -100.0f, 100.0f);
	gs_blend_state_push();
	gs_blend_function_separate(GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA, GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);
	return true;
}

static inline void sbk_stage_end(struct sbk_stage *st)
{
	gs_blend_state_pop();
	gs_texrender_end(st->tr);
}

/* Put the stage on the canvas at (dx, dy) with the given opacity. fade_x
   softens the left and right edges over that many pixels — the ticker's lane
   leaves without a hard cut. */
static inline void sbk_stage_present_ex(struct sbk_stage *st, float alpha, float dx, float dy, float fade_x)
{
	gs_texture_t *tex = st->tr ? gs_texrender_get_texture(st->tr) : NULL;
	if (!tex || !st->blit || alpha <= 0.001f)
		return;
	gs_eparam_t *img = gs_effect_get_param_by_name(st->blit, "image");
	if (img)
		gs_effect_set_texture(img, tex);
	sbk_set_float(st->blit, "alpha", alpha);
	sbk_set_float(st->blit, "fade_x", fade_x);
	sbk_set_vec2(st->blit, "size", (float)st->cx, (float)st->cy);

	gs_matrix_push();
	gs_matrix_translate3f(dx, dy, 0.0f);
	gs_blend_state_push();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);
	while (gs_effect_loop(st->blit, "Draw"))
		gs_draw_sprite(tex, 0, st->cx, st->cy);
	gs_blend_state_pop();
	gs_matrix_pop();
}

static inline void sbk_stage_present(struct sbk_stage *st, float alpha, float dx, float dy)
{
	sbk_stage_present_ex(st, alpha, dx, dy, 0.0f);
}
