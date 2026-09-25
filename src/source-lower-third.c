/*  SBK Lower Third — a name and a line under it.

    Five variants. They differ in where the weight sits: a card carries it on
    glass, a split puts the title on the accent, an underline carries none at
    all and leans on the rule. Rises into place when shown; a hotkey plays the
    arrival again mid-stream.  */

#include "sbk-common.h"
#include "sbk-text.h"
#include "sbk-stage.h"
#include "sbk-anim.h"

enum lower_variant { LOWER_CARD = 0, LOWER_PILL, LOWER_SPLIT, LOWER_MINIMAL, LOWER_UNDERLINE };

struct lower {
	obs_source_t *self;
	struct sbk_look look;
	struct sbk_anim anim;
	struct sbk_stage stage;
	gs_effect_t *card_fx;
	struct sbk_text name, title;
	char *s_name, *s_title;
	enum lower_variant variant;
	bool bar;
	obs_hotkey_id replay_id;
	uint32_t cx, cy;
	/* measured in tick, drawn in render */
	float pad_x, pad_y, text_x, name_y, title_y, split_y, split_h;
};

static const char *lower_name(void *u)
{
	UNUSED_PARAMETER(u);
	return obs_module_text("SBK.LowerThird");
}

static enum lower_variant variant_from(const char *id)
{
	if (!id)
		return LOWER_CARD;
	if (astrcmpi(id, "pill") == 0)
		return LOWER_PILL;
	if (astrcmpi(id, "split") == 0)
		return LOWER_SPLIT;
	if (astrcmpi(id, "minimal") == 0)
		return LOWER_MINIMAL;
	if (astrcmpi(id, "underline") == 0)
		return LOWER_UNDERLINE;
	return LOWER_CARD;
}

static void on_replay(void *data, obs_hotkey_id id, obs_hotkey_t *hk, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hk);
	if (pressed)
		sbk_anim_play(&((struct lower *)data)->anim);
}

static void lower_update(void *data, obs_data_t *s)
{
	struct lower *w = data;
	sbk_look_read(&w->look, s);
	sbk_anim_read(&w->anim, s, 6.0f * sbk_u(&w->look));
	bfree(w->s_name);
	bfree(w->s_title);
	w->s_name = bstrdup(obs_data_get_string(s, "name"));
	w->s_title = bstrdup(obs_data_get_string(s, "title"));
	w->variant = variant_from(obs_data_get_string(s, "variant"));
	w->bar = obs_data_get_bool(s, "bar");
}

static void *lower_create(obs_data_t *s, obs_source_t *source)
{
	struct lower *w = bzalloc(sizeof(*w));
	w->self = source;
	w->card_fx = sbk_load_effect("effects/card.effect");
	sbk_stage_init(&w->stage);
	w->replay_id = obs_hotkey_register_source(source, "SBK.LowerThird.Replay", "Broadcast Kit: play the lower third in again",
						  on_replay, w);
	lower_update(w, s);
	sbk_anim_play(&w->anim);
	return w;
}

static void lower_destroy(void *data)
{
	struct lower *w = data;
	if (w->replay_id != OBS_INVALID_HOTKEY_ID)
		obs_hotkey_unregister(w->replay_id);
	sbk_text_free(&w->name);
	sbk_text_free(&w->title);
	sbk_stage_free(&w->stage);
	sbk_free_effect(&w->card_fx);
	bfree(w->s_name);
	bfree(w->s_title);
	bfree(w);
}

static void lower_tick(void *data, float seconds)
{
	struct lower *w = data;
	sbk_anim_tick(&w->anim, seconds);
	const struct sbk_look *l = &w->look;
	const float u = sbk_u(l);
	const bool bare = w->variant == LOWER_MINIMAL || w->variant == LOWER_UNDERLINE;

	struct vec4 title_ink = w->variant == LOWER_SPLIT ? l->on_accent : l->ink_dim;
	sbk_text_set_full(&w->name, w->s_name, l->face, "SemiBold", (int)(10.0f * u), l->ink, 0, bare);
	sbk_text_set_full(&w->title, w->s_title, l->face, "Regular", (int)(5.5f * u), title_ink, 0, bare);

	const bool has_title = w->title.text.len > 0;
	float nw = (float)sbk_text_w(&w->name), nh = (float)sbk_text_h(&w->name);
	float tw = has_title ? (float)sbk_text_w(&w->title) : 0.0f;
	float th = has_title ? (float)sbk_text_h(&w->title) : 0.0f;

	w->pad_x = bare ? 0.0f : 7.0f * u;
	w->pad_y = bare ? 0.0f : 5.0f * u;
	float bar_w = w->bar && w->variant != LOWER_UNDERLINE ? 1.5f * u : 0.0f;
	float bar_gap = bar_w > 0.0f ? 4.0f * u : 0.0f;
	w->text_x = w->pad_x + bar_w + bar_gap;

	if (w->variant == LOWER_SPLIT) {
		/* the title gets its own accent slab under the name's card */
		w->name_y = w->pad_y;
		w->split_y = w->pad_y + nh + 4.0f * u;
		w->split_h = has_title ? th + 4.0f * u : 0.0f;
		w->title_y = w->split_y + 2.0f * u;
		w->cx = (uint32_t)(w->text_x + fmaxf(nw, tw + 6.0f * u) + w->pad_x + 0.5f);
		w->cy = (uint32_t)(w->split_y + w->split_h + (has_title ? w->pad_y : 0.0f) + 0.5f);
		return;
	}

	w->name_y = w->pad_y;
	w->title_y = w->pad_y + nh + 1.0f * u;
	float rule = w->variant == LOWER_UNDERLINE ? 3.0f * u : 0.0f;
	w->cx = (uint32_t)(w->text_x + fmaxf(nw, tw) + w->pad_x + 0.5f);
	w->cy = (uint32_t)(w->pad_y * 2.0f + nh + (has_title ? th + 1.0f * u : 0.0f) + rule + 0.5f);
}

static uint32_t lower_width(void *d) { return ((struct lower *)d)->cx; }
static uint32_t lower_height(void *d) { return ((struct lower *)d)->cy; }

static void lower_render(void *data, gs_effect_t *unused)
{
	UNUSED_PARAMETER(unused);
	struct lower *w = data;
	if (!w->card_fx || !sbk_stage_begin(&w->stage, w->cx, w->cy))
		return;
	const struct sbk_look *l = &w->look;
	const float u = sbk_u(l);
	struct vec4 none = SBK_NONE;
	const bool has_title = w->title.text.len > 0;

	switch (w->variant) {
	case LOWER_PILL:
		sbk_card(w->card_fx, 0, 0, (float)w->cx, (float)w->cy, (float)w->cy * 0.5f, l->glass, l->glass_line, 1.0f,
			   none, 0.0f);
		break;
	case LOWER_SPLIT:
		sbk_card(w->card_fx, 0, 0, (float)w->cx, (float)w->cy, 3.0f * u, l->glass, l->glass_line, 1.0f, none, 0.0f);
		if (has_title)
			sbk_fill(w->card_fx, w->text_x - 2.0f * u, w->split_y,
				   (float)sbk_text_w(&w->title) + 4.0f * u, w->split_h, 1.0f * u, l->accent);
		break;
	case LOWER_MINIMAL:
	case LOWER_UNDERLINE:
		break;
	default:
		sbk_card(w->card_fx, 0, 0, (float)w->cx, (float)w->cy, 3.0f * u, l->glass, l->glass_line, 1.0f, none, 0.0f);
		break;
	}

	if (w->bar && w->variant != LOWER_UNDERLINE) {
		float bw = 1.5f * u;
		float top = w->variant == LOWER_MINIMAL ? 0.0f : w->pad_y;
		sbk_fill(w->card_fx, w->pad_x, top, bw, (float)w->cy - top * 2.0f, bw * 0.5f, l->accent);
	}

	sbk_text_draw(&w->name, w->text_x, w->name_y);
	if (has_title)
		sbk_text_draw(&w->title, w->text_x, w->title_y);

	if (w->variant == LOWER_UNDERLINE) {
		float h = 1.0f * u;
		sbk_fill(w->card_fx, w->text_x, (float)w->cy - 2.0f * u, (float)w->cx - w->text_x, h, h * 0.5f, l->accent);
	}

	sbk_stage_end(&w->stage);
	struct sbk_anim_out a = sbk_anim_eval(&w->anim);
	sbk_stage_present(&w->stage, a.alpha, a.dx, a.dy);
}

static void lower_show(void *d) { sbk_anim_on_show(&((struct lower *)d)->anim); }
static void lower_enum(void *d, obs_source_enum_proc_t cb, void *p)
{
	struct lower *w = d;
	sbk_text_enum(&w->name, w->self, cb, p);
	sbk_text_enum(&w->title, w->self, cb, p);
}

static obs_properties_t *lower_properties(void *data)
{
	UNUSED_PARAMETER(data);
	obs_properties_t *p = obs_properties_create();
	obs_properties_add_text(p, "name", "Name", OBS_TEXT_DEFAULT);
	obs_properties_add_text(p, "title", "Line under it", OBS_TEXT_DEFAULT);
	obs_property_t *v = obs_properties_add_list(p, "variant", "Variant", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(v, "Card", "card");
	obs_property_list_add_string(v, "Pill", "pill");
	obs_property_list_add_string(v, "Split — the title on the accent", "split");
	obs_property_list_add_string(v, "Minimal — no card, shadowed type", "minimal");
	obs_property_list_add_string(v, "Underline — an accent rule, no card", "underline");
	obs_properties_add_bool(p, "bar", "Accent bar on the left");
	sbk_look_props(p, false);
	sbk_anim_props(p);
	obs_properties_add_text(p, "hint", "Bind \"Broadcast Kit: play the lower third in again\" under Settings → Hotkeys to re-run the arrival mid-stream.",
				OBS_TEXT_INFO);
	return p;
}

static void lower_defaults(obs_data_t *s)
{
	obs_data_set_default_string(s, "name", "Swarnil Singhai");
	obs_data_set_default_string(s, "title", "Salesforce Architect · imswarnil.com");
	obs_data_set_default_string(s, "variant", "card");
	obs_data_set_default_bool(s, "bar", true);
	sbk_look_defaults(s);
	sbk_anim_defaults(s, "up");
}

struct obs_source_info sbk_lower_third_info = {
	.id = "sbk_lower_third",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
	.get_name = lower_name,
	.create = lower_create,
	.destroy = lower_destroy,
	.update = lower_update,
	.get_defaults = lower_defaults,
	.get_properties = lower_properties,
	.get_width = lower_width,
	.get_height = lower_height,
	.video_tick = lower_tick,
	.video_render = lower_render,
	.show = lower_show,
	.enum_active_sources = lower_enum,
	.icon_type = OBS_ICON_TYPE_TEXT,
};
