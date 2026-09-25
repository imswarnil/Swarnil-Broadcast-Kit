/*  SBK Chip — one small badge. A handle, a hashtag, "Q&A", "New video",
    a follower count.

    It is the piece every overlay set needs a dozen of and nobody wants to
    build a dozen times: a label, optionally a second segment holding a value,
    optionally a dot in front that can be wired to the tally state so the chip
    itself says LIVE.  */

#include "sbk-common.h"
#include "sbk-text.h"
#include "sbk-stage.h"
#include "sbk-anim.h"
#include "sbk-state.h"

enum dot_mode { DOT_NONE = 0, DOT_ACCENT, DOT_PULSE, DOT_LIVE };

struct chip {
	obs_source_t *self;
	struct sbk_look look;
	struct sbk_anim anim;
	struct sbk_stage stage;
	gs_effect_t *card_fx;
	struct sbk_text label, value;
	char *s_label, *s_value;
	enum sbk_surface surf;
	enum dot_mode dot;
	bool hide_when_off;
	float t;
	uint32_t cx, cy;
	float value_x, value_w;
};

static const char *chip_name(void *u)
{
	UNUSED_PARAMETER(u);
	return obs_module_text("SBK.Chip");
}

static enum dot_mode dot_from(const char *id)
{
	if (!id || astrcmpi(id, "none") == 0)
		return DOT_NONE;
	if (astrcmpi(id, "pulse") == 0)
		return DOT_PULSE;
	if (astrcmpi(id, "live") == 0)
		return DOT_LIVE;
	return DOT_ACCENT;
}

static void chip_update(void *data, obs_data_t *s)
{
	struct chip *c = data;
	sbk_look_read(&c->look, s);
	sbk_anim_read(&c->anim, s, 4.0f * sbk_u(&c->look));
	bfree(c->s_label);
	bfree(c->s_value);
	c->s_label = bstrdup(obs_data_get_string(s, "label"));
	c->s_value = bstrdup(obs_data_get_string(s, "value"));
	c->surf = sbk_surface_from(obs_data_get_string(s, "variant"));
	c->dot = dot_from(obs_data_get_string(s, "dot"));
	c->hide_when_off = obs_data_get_bool(s, "hide_when_off");
}

static void *chip_create(obs_data_t *s, obs_source_t *source)
{
	struct chip *c = bzalloc(sizeof(*c));
	c->self = source;
	c->card_fx = sbk_load_effect("effects/card.effect");
	sbk_stage_init(&c->stage);
	chip_update(c, s);
	sbk_anim_play(&c->anim);
	return c;
}

static void chip_destroy(void *data)
{
	struct chip *c = data;
	sbk_text_free(&c->label);
	sbk_text_free(&c->value);
	sbk_stage_free(&c->stage);
	sbk_free_effect(&c->card_fx);
	bfree(c->s_label);
	bfree(c->s_value);
	bfree(c);
}

static void chip_tick(void *data, float seconds)
{
	struct chip *c = data;
	c->t += seconds;
	sbk_anim_tick(&c->anim, seconds);
	const struct sbk_look *l = &c->look;
	const float u = sbk_u(l);
	const bool bare = c->surf == SBK_SURF_NONE;

	sbk_text_set_full(&c->label, c->s_label, l->face, "SemiBold", (int)(5.0f * u),
			    sbk_surface_ink(c->surf, l, false), 0, bare);
	sbk_text_set_full(&c->value, c->s_value, l->mono, "Bold", (int)(5.0f * u), l->on_accent, 0, false);

	float pad_x = bare ? 0.0f : 5.0f * u, pad_y = bare ? 0.0f : 3.0f * u;
	float dot = 2.5f * u, gap = 2.5f * u;
	float lead = c->dot == DOT_NONE ? 0.0f : dot + gap;
	float lw = (float)sbk_text_w(&c->label), lh = (float)sbk_text_h(&c->label);

	c->cy = (uint32_t)(pad_y * 2.0f + lh + 0.5f);
	c->value_w = c->value.text.len ? (float)sbk_text_w(&c->value) + 6.0f * u : 0.0f;
	c->value_x = pad_x + lead + lw + (c->value_w > 0.0f ? 4.0f * u : 0.0f);
	c->cx = (uint32_t)(c->value_x + c->value_w + pad_x + 0.5f);

	if (c->hide_when_off && c->dot == DOT_LIVE && strcmp(sbk_state_word(), "off") == 0)
		c->cx = c->cy = 0;
}

static uint32_t chip_width(void *d) { return ((struct chip *)d)->cx; }
static uint32_t chip_height(void *d) { return ((struct chip *)d)->cy; }

static void chip_render(void *data, gs_effect_t *unused)
{
	UNUSED_PARAMETER(unused);
	struct chip *c = data;
	if (!c->card_fx || !sbk_stage_begin(&c->stage, c->cx, c->cy))
		return;
	const struct sbk_look *l = &c->look;
	const float u = sbk_u(l);
	const bool bare = c->surf == SBK_SURF_NONE;
	float pad_x = bare ? 0.0f : 5.0f * u, pad_y = bare ? 0.0f : 3.0f * u;

	sbk_surface_draw(c->card_fx, c->surf, l, 0, 0, (float)c->cx, (float)c->cy, (float)c->cy * 0.5f);

	float x = pad_x;
	if (c->dot != DOT_NONE) {
		float d = 2.5f * u;
		bool live = strcmp(sbk_state_word(), "off") != 0;
		struct vec4 col = c->dot == DOT_LIVE && !live ? sbk_vec(SBK_OFF)
				  : c->surf == SBK_SURF_ACCENT ? l->on_accent
								 : l->accent;
		bool beat = c->dot == DOT_PULSE || (c->dot == DOT_LIVE && live);
		float pulse = beat ? 0.5f + 0.5f * sinf(c->t * 6.2831853f / SBK_PULSE_SECS) : 0.0f;
		sbk_dot(c->card_fx, x, ((float)c->cy - d) * 0.5f, d, col, beat ? 2.5f * u : 0.0f, 0.25f + 0.45f * pulse);
		x += d + 2.5f * u;
	}
	sbk_text_draw(&c->label, x, pad_y);

	if (c->value_w > 0.0f) {
		/* the value rides in its own accent capsule, hard against the label */
		sbk_fill(c->card_fx, c->value_x, pad_y * 0.5f, c->value_w, (float)c->cy - pad_y,
			   ((float)c->cy - pad_y) * 0.5f, l->accent);
		sbk_text_draw(&c->value, c->value_x + 3.0f * u, pad_y);
	}

	sbk_stage_end(&c->stage);
	struct sbk_anim_out a = sbk_anim_eval(&c->anim);
	sbk_stage_present(&c->stage, a.alpha, a.dx, a.dy);
}

static void chip_show(void *d) { sbk_anim_on_show(&((struct chip *)d)->anim); }
static void chip_enum(void *d, obs_source_enum_proc_t cb, void *p)
{
	struct chip *c = d;
	sbk_text_enum(&c->label, c->self, cb, p);
	sbk_text_enum(&c->value, c->self, cb, p);
}

static obs_properties_t *chip_properties(void *data)
{
	UNUSED_PARAMETER(data);
	obs_properties_t *p = obs_properties_create();
	obs_properties_add_text(p, "label", "Label", OBS_TEXT_DEFAULT);
	obs_properties_add_text(p, "value", "Value (empty for none)", OBS_TEXT_DEFAULT);
	sbk_surface_list(p, "variant", "Variant");
	obs_property_t *d = obs_properties_add_list(p, "dot", "Leading dot", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(d, "None", "none");
	obs_property_list_add_string(d, "Accent", "accent");
	obs_property_list_add_string(d, "Accent, breathing", "pulse");
	obs_property_list_add_string(d, "Lit only when on air", "live");
	obs_properties_add_bool(p, "hide_when_off", "Hide the chip when off air (with the live dot)");
	sbk_look_props(p, true);
	sbk_anim_props(p);
	return p;
}

static void chip_defaults(obs_data_t *s)
{
	obs_data_set_default_string(s, "label", "@imswarnil");
	obs_data_set_default_string(s, "value", "");
	obs_data_set_default_string(s, "variant", "pill");
	obs_data_set_default_string(s, "dot", "accent");
	obs_data_set_default_bool(s, "hide_when_off", false);
	sbk_look_defaults(s);
	sbk_anim_defaults(s, "fade");
}

struct obs_source_info sbk_chip_info = {
	.id = "sbk_chip",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
	.get_name = chip_name,
	.create = chip_create,
	.destroy = chip_destroy,
	.update = chip_update,
	.get_defaults = chip_defaults,
	.get_properties = chip_properties,
	.get_width = chip_width,
	.get_height = chip_height,
	.video_tick = chip_tick,
	.video_render = chip_render,
	.show = chip_show,
	.enum_active_sources = chip_enum,
	.icon_type = OBS_ICON_TYPE_TEXT,
};
