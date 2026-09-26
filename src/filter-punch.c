/*  SBK Punch — a zoom into the picture, on a hotkey.

    This is the one filter here that earns its place purely by being native. In
    a tutorial you constantly want to push in on what you are pointing at and
    come back out, and doing that by hand means grabbing a scene item mid-
    sentence. A key press instead: it eases in, holds, and eases back.

    The window is kept inside the frame, so a punch near an edge slides along
    rather than showing the clamped edge pixel smeared across a third of the
    picture.  */

#include "sbk-common.h"

enum punch_state { PUNCH_OUT = 0, PUNCH_IN_PROGRESS, PUNCH_HELD, PUNCH_OUT_PROGRESS };

struct punch {
	obs_source_t *self;
	gs_effect_t *fx;

	float zoom_target;      /* what a full punch reaches */
	float in_secs, out_secs, hold_secs;
	float focus_x, focus_y; /* 0..1 */
	bool auto_out;

	enum punch_state state;
	float t;        /* 0..1 through the current ease */
	float held;     /* seconds held so far */
	float zoom;     /* what is drawn now */

	obs_hotkey_id in_id, out_id, toggle_id;
	uint32_t cx, cy;
};

static const char *punch_name(void *u)
{
	UNUSED_PARAMETER(u);
	return obs_module_text("SBK.Punch");
}

/* fast out of the gate and settling at the end, which is how a camera operator
   moves and nothing like a linear ramp */
static float ease(float t)
{
	t = sbk_clampf(t, 0.0f, 1.0f);
	return t < 0.5f ? 4.0f * t * t * t : 1.0f - powf(-2.0f * t + 2.0f, 3.0f) / 2.0f;
}

static void punch_in(struct punch *p)
{
	if (p->state == PUNCH_IN_PROGRESS || p->state == PUNCH_HELD)
		return;
	/* carry the current zoom across rather than snapping back to 1 first */
	float span = p->zoom_target - 1.0f;
	p->t = span > 0.001f ? sbk_clampf((p->zoom - 1.0f) / span, 0.0f, 1.0f) : 0.0f;
	p->state = PUNCH_IN_PROGRESS;
	p->held = 0.0f;
}

static void punch_out(struct punch *p)
{
	if (p->state == PUNCH_OUT || p->state == PUNCH_OUT_PROGRESS)
		return;
	float span = p->zoom_target - 1.0f;
	p->t = span > 0.001f ? 1.0f - sbk_clampf((p->zoom - 1.0f) / span, 0.0f, 1.0f) : 0.0f;
	p->state = PUNCH_OUT_PROGRESS;
}

static void on_in(void *data, obs_hotkey_id id, obs_hotkey_t *hk, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hk);
	if (pressed)
		punch_in(data);
}
static void on_out(void *data, obs_hotkey_id id, obs_hotkey_t *hk, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hk);
	if (pressed)
		punch_out(data);
}
static void on_toggle(void *data, obs_hotkey_id id, obs_hotkey_t *hk, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hk);
	struct punch *p = data;
	if (!pressed)
		return;
	if (p->state == PUNCH_OUT || p->state == PUNCH_OUT_PROGRESS)
		punch_in(p);
	else
		punch_out(p);
}

static void punch_update(void *data, obs_data_t *s)
{
	struct punch *p = data;
	p->zoom_target = (float)obs_data_get_double(s, "zoom");
	p->in_secs = (float)obs_data_get_double(s, "in_secs");
	p->out_secs = (float)obs_data_get_double(s, "out_secs");
	p->hold_secs = (float)obs_data_get_double(s, "hold_secs");
	p->auto_out = obs_data_get_bool(s, "auto_out");
	p->focus_x = (float)obs_data_get_double(s, "focus_x") / 100.0f;
	p->focus_y = (float)obs_data_get_double(s, "focus_y") / 100.0f;
}

static void *punch_create(obs_data_t *s, obs_source_t *source)
{
	struct punch *p = bzalloc(sizeof(*p));
	p->self = source;
	p->fx = sbk_load_effect("effects/punch.effect");
	p->zoom = 1.0f;
	p->in_id = obs_hotkey_register_source(source, "SBK.Punch.In", "Broadcast Kit: punch in", on_in, p);
	p->out_id = obs_hotkey_register_source(source, "SBK.Punch.Out", "Broadcast Kit: punch out", on_out, p);
	p->toggle_id = obs_hotkey_register_source(source, "SBK.Punch.Toggle", "Broadcast Kit: punch in or out",
						  on_toggle, p);
	punch_update(p, s);
	return p;
}

static void punch_destroy(void *data)
{
	struct punch *p = data;
	const obs_hotkey_id keys[] = {p->in_id, p->out_id, p->toggle_id};
	for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++)
		if (keys[i] != OBS_INVALID_HOTKEY_ID)
			obs_hotkey_unregister(keys[i]);
	sbk_free_effect(&p->fx);
	bfree(p);
}

static void punch_tick(void *data, float seconds)
{
	struct punch *p = data;
	switch (p->state) {
	case PUNCH_IN_PROGRESS:
		p->t += seconds / fmaxf(p->in_secs, 0.01f);
		if (p->t >= 1.0f) {
			p->t = 1.0f;
			p->state = PUNCH_HELD;
			p->held = 0.0f;
		}
		p->zoom = 1.0f + (p->zoom_target - 1.0f) * ease(p->t);
		break;
	case PUNCH_HELD:
		p->zoom = p->zoom_target;
		if (p->auto_out) {
			p->held += seconds;
			if (p->held >= p->hold_secs)
				punch_out(p);
		}
		break;
	case PUNCH_OUT_PROGRESS:
		p->t += seconds / fmaxf(p->out_secs, 0.01f);
		if (p->t >= 1.0f) {
			p->t = 1.0f;
			p->state = PUNCH_OUT;
		}
		p->zoom = 1.0f + (p->zoom_target - 1.0f) * (1.0f - ease(p->t));
		break;
	default:
		p->zoom = 1.0f;
		break;
	}
}

static void punch_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);
	struct punch *p = data;
	obs_source_t *target = obs_filter_get_target(p->self);
	if (!p->fx || !target) {
		obs_source_skip_video_filter(p->self);
		return;
	}
	p->cx = obs_source_get_base_width(target);
	p->cy = obs_source_get_base_height(target);
	if (!p->cx || !p->cy) {
		obs_source_skip_video_filter(p->self);
		return;
	}
	/* nothing to do at rest, and skipping keeps the picture bit-identical */
	if (p->zoom <= 1.0001f) {
		obs_source_skip_video_filter(p->self);
		return;
	}
	if (!obs_source_process_filter_begin(p->self, GS_RGBA, OBS_ALLOW_DIRECT_RENDERING))
		return;

	/* keep the visible window inside the frame: at zoom z the window is 1/z
	   wide, so its centre cannot be nearer an edge than half of that */
	float half = 0.5f / p->zoom;
	float fx = sbk_clampf(p->focus_x, half, 1.0f - half);
	float fy = sbk_clampf(p->focus_y, half, 1.0f - half);

	sbk_set_vec2(p->fx, "focus", fx, fy);
	sbk_set_float(p->fx, "zoom", p->zoom);
	obs_source_process_filter_end(p->self, p->fx, p->cx, p->cy);
}

static obs_properties_t *punch_properties(void *data)
{
	UNUSED_PARAMETER(data);
	obs_properties_t *p = obs_properties_create();
	obs_properties_add_float_slider(p, "zoom", "Zoom", 1.0, 4.0, 0.05);
	obs_properties_add_float_slider(p, "focus_x", "Focus across (%)", 0.0, 100.0, 1.0);
	obs_properties_add_float_slider(p, "focus_y", "Focus down (%)", 0.0, 100.0, 1.0);
	obs_properties_add_float_slider(p, "in_secs", "In (seconds)", 0.05, 3.0, 0.05);
	obs_properties_add_float_slider(p, "out_secs", "Out (seconds)", 0.05, 3.0, 0.05);
	obs_properties_add_bool(p, "auto_out", "Come back out on its own");
	obs_properties_add_float_slider(p, "hold_secs", "Hold for (seconds)", 0.5, 30.0, 0.5);
	obs_properties_add_text(p, "hint",
				"Bind \"Broadcast Kit: punch in or out\" under Settings → Hotkeys — one key that "
				"pushes in on what you are pointing at and brings you back. The focus point is "
				"where in the picture stays still, not where the camera goes.",
				OBS_TEXT_INFO);
	return p;
}

static void punch_defaults(obs_data_t *s)
{
	obs_data_set_default_double(s, "zoom", 1.6);
	obs_data_set_default_double(s, "focus_x", 50.0);
	obs_data_set_default_double(s, "focus_y", 45.0);
	obs_data_set_default_double(s, "in_secs", 0.45);
	obs_data_set_default_double(s, "out_secs", 0.55);
	obs_data_set_default_bool(s, "auto_out", false);
	obs_data_set_default_double(s, "hold_secs", 6.0);
}

struct obs_source_info sbk_punch_info = {
	.id = "sbk_punch",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_VIDEO,
	.get_name = punch_name,
	.create = punch_create,
	.destroy = punch_destroy,
	.update = punch_update,
	.get_defaults = punch_defaults,
	.get_properties = punch_properties,
	.video_tick = punch_tick,
	.video_render = punch_render,
};
