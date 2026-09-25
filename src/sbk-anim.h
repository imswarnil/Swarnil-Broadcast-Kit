#pragma once

#include "sbk-common.h"

/*  The arrival. A component eases in when it appears — fades, and slides a
    short way from one side — and holds. There is no exit: OBS hides a scene
    item instantly, so an exit could only ever play on a source that stays.

    Fast-then-settling (ease-out) on entry, because that is how physical
    things arrive; a linear slide reads as mechanical.  */

enum sbk_enter { SBK_ENTER_NONE = 0, SBK_ENTER_FADE, SBK_ENTER_UP, SBK_ENTER_DOWN, SBK_ENTER_LEFT, SBK_ENTER_RIGHT };

struct sbk_anim {
	enum sbk_enter kind;
	float secs, travel;
	bool replay_on_show;
	float t; /* 0..1 */
	bool playing;
};

struct sbk_anim_out {
	float alpha, dx, dy;
};

static inline enum sbk_enter sbk_enter_from(const char *id)
{
	if (!id || astrcmpi(id, "fade") == 0)
		return SBK_ENTER_FADE;
	if (astrcmpi(id, "up") == 0)
		return SBK_ENTER_UP;
	if (astrcmpi(id, "down") == 0)
		return SBK_ENTER_DOWN;
	if (astrcmpi(id, "left") == 0)
		return SBK_ENTER_LEFT;
	if (astrcmpi(id, "right") == 0)
		return SBK_ENTER_RIGHT;
	return SBK_ENTER_NONE;
}

static inline void sbk_anim_defaults(obs_data_t *s, const char *kind)
{
	obs_data_set_default_string(s, "enter", kind);
	obs_data_set_default_double(s, "enter_secs", SBK_ENTER_SECS);
	obs_data_set_default_bool(s, "enter_on_show", true);
}

static inline void sbk_anim_props(obs_properties_t *props)
{
	obs_properties_t *g = obs_properties_create();
	obs_property_t *l = obs_properties_add_list(g, "enter", "Arrives by", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(l, "Appearing", "none");
	obs_property_list_add_string(l, "Fading in", "fade");
	obs_property_list_add_string(l, "Rising", "up");
	obs_property_list_add_string(l, "Dropping in", "down");
	obs_property_list_add_string(l, "Sliding from the left", "left");
	obs_property_list_add_string(l, "Sliding from the right", "right");
	obs_properties_add_float_slider(g, "enter_secs", "Over (seconds)", 0.1, 2.0, 0.05);
	obs_properties_add_bool(g, "enter_on_show", "Play again each time the source is shown");
	obs_properties_add_group(props, "motion", "Motion", OBS_GROUP_NORMAL, g);
}

static inline void sbk_anim_read(struct sbk_anim *a, obs_data_t *s, float travel)
{
	a->kind = sbk_enter_from(obs_data_get_string(s, "enter"));
	a->secs = (float)obs_data_get_double(s, "enter_secs");
	if (a->secs < 0.05f)
		a->secs = 0.05f;
	a->travel = travel;
	a->replay_on_show = obs_data_get_bool(s, "enter_on_show");
}

static inline void sbk_anim_play(struct sbk_anim *a)
{
	a->t = 0.0f;
	a->playing = a->kind != SBK_ENTER_NONE;
}

static inline void sbk_anim_on_show(struct sbk_anim *a)
{
	if (a->replay_on_show)
		sbk_anim_play(a);
}

static inline void sbk_anim_tick(struct sbk_anim *a, float seconds)
{
	if (!a->playing)
		return;
	a->t += seconds / a->secs;
	if (a->t >= 1.0f) {
		a->t = 1.0f;
		a->playing = false;
	}
}

static inline struct sbk_anim_out sbk_anim_eval(const struct sbk_anim *a)
{
	struct sbk_anim_out o = {1.0f, 0.0f, 0.0f};
	if (!a->playing || a->kind == SBK_ENTER_NONE)
		return o;
	float p = sbk_ease_out(a->t);
	o.alpha = p;
	float rest = (1.0f - p) * a->travel;
	switch (a->kind) {
	case SBK_ENTER_UP: o.dy = rest; break;
	case SBK_ENTER_DOWN: o.dy = -rest; break;
	case SBK_ENTER_LEFT: o.dx = -rest; break;
	case SBK_ENTER_RIGHT: o.dx = rest; break;
	default: break;
	}
	return o;
}
