/*  SBK Light — the tally light, the small red lamp on a studio camera that
    says THIS ONE IS ON AIR.

    LIVE while streaming, REC while recording, LIVE · REC for both, OFF AIR
    otherwise — read from OBS itself, so the lamp turns red when you go live,
    not when you remember to click something. The dot glows and breathes while
    lit.

    Five shapes, because where the light goes decides what it should look like:
    a pill or a badge in a corner, a bare dot when the corner is crowded, a bar
    across the head of the frame, or the edge of the whole canvas lit up.  */

#include "sbk-common.h"
#include "sbk-text.h"
#include "sbk-stage.h"
#include "sbk-anim.h"
#include "sbk-state.h"

enum shape { SHAPE_PILL = 0, SHAPE_BADGE, SHAPE_DOT, SHAPE_BAR, SHAPE_EDGE };

struct onair {
	obs_source_t *self;
	struct sbk_look look;
	struct sbk_anim anim;
	struct sbk_stage stage;
	gs_effect_t *card_fx;
	struct sbk_text label;

	enum shape shape;
	char *word_off, *word_live, *word_rec, *word_both;
	bool hide_when_off, breathe, fill_when_live;
	uint32_t bar_w, bar_h;
	float t;
	uint32_t cx, cy;
};

static const char *onair_name(void *u)
{
	UNUSED_PARAMETER(u);
	return obs_module_text("SBK.OnAir");
}

static enum shape shape_from(const char *id)
{
	if (!id)
		return SHAPE_PILL;
	if (astrcmpi(id, "badge") == 0)
		return SHAPE_BADGE;
	if (astrcmpi(id, "dot") == 0)
		return SHAPE_DOT;
	if (astrcmpi(id, "bar") == 0)
		return SHAPE_BAR;
	if (astrcmpi(id, "edge") == 0)
		return SHAPE_EDGE;
	return SHAPE_PILL;
}

static void onair_update(void *data, obs_data_t *s)
{
	struct onair *o = data;
	sbk_look_read(&o->look, s);
	sbk_anim_read(&o->anim, s, 4.0f * sbk_u(&o->look));
	bfree(o->word_off);
	bfree(o->word_live);
	bfree(o->word_rec);
	bfree(o->word_both);
	o->word_off = bstrdup(obs_data_get_string(s, "word_off"));
	o->word_live = bstrdup(obs_data_get_string(s, "word_live"));
	o->word_rec = bstrdup(obs_data_get_string(s, "word_rec"));
	o->word_both = bstrdup(obs_data_get_string(s, "word_both"));
	o->shape = shape_from(obs_data_get_string(s, "shape"));
	o->hide_when_off = obs_data_get_bool(s, "hide_when_off");
	o->breathe = obs_data_get_bool(s, "breathe");
	o->fill_when_live = obs_data_get_bool(s, "fill_when_live");
	o->bar_w = (uint32_t)obs_data_get_int(s, "width");
	o->bar_h = (uint32_t)obs_data_get_int(s, "height");
}

static void *onair_create(obs_data_t *s, obs_source_t *source)
{
	struct onair *o = bzalloc(sizeof(*o));
	o->self = source;
	o->card_fx = sbk_load_effect("effects/card.effect");
	sbk_stage_init(&o->stage);
	onair_update(o, s);
	sbk_anim_play(&o->anim);
	return o;
}

static void onair_destroy(void *data)
{
	struct onair *o = data;
	sbk_text_free(&o->label);
	sbk_stage_free(&o->stage);
	sbk_free_effect(&o->card_fx);
	bfree(o->word_off);
	bfree(o->word_live);
	bfree(o->word_rec);
	bfree(o->word_both);
	bfree(o);
}

static const char *word_for(struct onair *o, const char *state)
{
	if (strcmp(state, "both") == 0)
		return o->word_both;
	if (strcmp(state, "live") == 0)
		return o->word_live;
	if (strcmp(state, "rec") == 0)
		return o->word_rec;
	return o->word_off;
}

/* the lamp: the accent while streaming, the record orange while only
   recording, a flat grey when neither */
static struct vec4 lamp_color(struct onair *o, const char *state)
{
	if (strcmp(state, "off") == 0)
		return sbk_vec(SBK_OFF);
	if (strcmp(state, "rec") == 0)
		return sbk_vec(SBK_REC);
	return o->look.accent;
}

static void onair_tick(void *data, float seconds)
{
	struct onair *o = data;
	o->t += seconds;
	sbk_anim_tick(&o->anim, seconds);

	const struct sbk_look *l = &o->look;
	const float u = sbk_u(l);
	const char *state = sbk_state_word();
	bool off = strcmp(state, "off") == 0;
	bool lit = !off && o->fill_when_live;

	struct dstr w = {0};
	dstr_copy(&w, word_for(o, state));
	sbk_caps(&w);
	sbk_text_set(&o->label, o->shape == SHAPE_DOT ? "" : (w.array ? w.array : ""), l->face, "SemiBold",
		       (int)(5.5f * u), lit ? l->on_accent : (off ? l->ink_dim : l->ink));
	dstr_free(&w);

	float pad_x = 5.0f * u, pad_y = 3.0f * u, dot = 3.5f * u, gap = 2.5f * u;
	float tw = (float)sbk_text_w(&o->label), th = (float)sbk_text_h(&o->label);

	switch (o->shape) {
	case SHAPE_DOT:
		o->cx = o->cy = (uint32_t)(dot + 6.0f * u + 0.5f); /* room for the glow */
		break;
	case SHAPE_BAR:
	case SHAPE_EDGE:
		o->cx = o->bar_w;
		o->cy = o->shape == SHAPE_BAR ? (uint32_t)(pad_y * 2.0f + fmaxf(th, dot) + 0.5f) : o->bar_h;
		break;
	default:
		o->cx = (uint32_t)(pad_x * 2.0f + dot + gap + tw + 0.5f);
		o->cy = (uint32_t)(pad_y * 2.0f + fmaxf(th, dot) + 0.5f);
		break;
	}
	if (o->hide_when_off && off)
		o->cx = o->cy = 0;
}

static uint32_t onair_width(void *d) { return ((struct onair *)d)->cx; }
static uint32_t onair_height(void *d) { return ((struct onair *)d)->cy; }

static void onair_render(void *data, gs_effect_t *unused)
{
	UNUSED_PARAMETER(unused);
	struct onair *o = data;
	if (!o->card_fx || !sbk_stage_begin(&o->stage, o->cx, o->cy))
		return;

	const struct sbk_look *l = &o->look;
	const float u = sbk_u(l);
	const char *state = sbk_state_word();
	bool off = strcmp(state, "off") == 0;
	struct vec4 none = SBK_NONE;
	struct vec4 lamp = lamp_color(o, state);
	float pulse = o->breathe && !off ? 0.5f + 0.5f * sinf(o->t * 6.2831853f / SBK_PULSE_SECS) : 1.0f;
	float glow = off ? 0.0f : 3.0f * u;
	bool lit = !off && o->fill_when_live;

	float pad_x = 5.0f * u, dot = 3.5f * u, gap = 2.5f * u;
	float th = (float)sbk_text_h(&o->label);

	if (o->shape == SHAPE_EDGE) {
		/* the whole canvas edge is the lamp — impossible to miss, and it
		   costs no room in the layout */
		float weight = 3.0f * u;
		sbk_card_ex(o->card_fx, 0, 0, (float)o->cx, (float)o->cy, 2.0f * u, none, none, 0.0f,
			      sbk_alpha(lamp, off ? 0.35f : 0.55f + 0.45f * pulse), weight,
			      sbk_alpha(lamp, off ? 0.0f : 0.30f * pulse), off ? 0.0f : 8.0f * u);
		goto done;
	}

	if (o->shape == SHAPE_DOT) {
		sbk_dot(o->card_fx, 3.0f * u, 3.0f * u, dot, lamp, glow, 0.35f + 0.45f * pulse);
		goto done;
	}

	{
		float radius = o->shape == SHAPE_BADGE ? 1.5f * u : (float)o->cy * 0.5f;
		if (o->shape == SHAPE_BAR)
			radius = 0.0f;
		struct vec4 fill = lit ? lamp : l->glass;
		sbk_card_ex(o->card_fx, 0, 0, (float)o->cx, (float)o->cy, radius, fill, fill, 0.0f,
			      lit ? none : l->glass_line, lit ? 0.0f : 1.0f, none, 0.0f);

		float x = pad_x;
		if (o->shape == SHAPE_BAR) {
			/* centred, because a bar spans the frame and a left-hugging
			   label on one looks like a mistake */
			float total = dot + gap + (float)sbk_text_w(&o->label);
			x = ((float)o->cx - total) * 0.5f;
		}
		sbk_dot(o->card_fx, x, ((float)o->cy - dot) * 0.5f, dot, lit ? l->on_accent : lamp,
			  lit ? 0.0f : glow, 0.35f + 0.45f * pulse);
		sbk_text_draw(&o->label, x + dot + gap, ((float)o->cy - th) * 0.5f);
	}

done:
	sbk_stage_end(&o->stage);
	sbk_stage_present_anim(&o->stage, sbk_anim_eval(&o->anim));
}

static void onair_show(void *d) { sbk_anim_on_show(&((struct onair *)d)->anim); }
static void onair_enum(void *d, obs_source_enum_proc_t cb, void *p)
{
	struct onair *o = d;
	sbk_text_enum(&o->label, o->self, cb, p);
}

static obs_properties_t *onair_properties(void *data)
{
	UNUSED_PARAMETER(data);
	obs_properties_t *p = obs_properties_create();
	obs_property_t *sh = obs_properties_add_list(p, "shape", "Shape", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(sh, "Pill", "pill");
	obs_property_list_add_string(sh, "Badge — squared off", "badge");
	obs_property_list_add_string(sh, "Dot only", "dot");
	obs_property_list_add_string(sh, "Bar across the frame", "bar");
	obs_property_list_add_string(sh, "Edge — the whole canvas border", "edge");
	obs_properties_add_bool(p, "fill_when_live", "Fill with the accent while lit");
	obs_properties_add_int(p, "width", "Width (bar and edge)", 120, 7680, 2);
	obs_properties_add_int(p, "height", "Height (edge)", 120, 4320, 2);

	obs_properties_t *w = obs_properties_create();
	obs_properties_add_text(w, "word_live", "While streaming", OBS_TEXT_DEFAULT);
	obs_properties_add_text(w, "word_rec", "While recording", OBS_TEXT_DEFAULT);
	obs_properties_add_text(w, "word_both", "Both", OBS_TEXT_DEFAULT);
	obs_properties_add_text(w, "word_off", "Otherwise", OBS_TEXT_DEFAULT);
	obs_properties_add_bool(w, "hide_when_off", "Hide entirely when off air");
	obs_properties_add_bool(w, "breathe", "The lamp breathes while lit");
	obs_properties_add_group(p, "words", "Words", OBS_GROUP_NORMAL, w);

	sbk_look_props(p, false);
	sbk_anim_props(p);
	obs_properties_add_text(p, "hint", "Reads OBS's real state — nothing to click. Start streaming or recording and watch it.",
				OBS_TEXT_INFO);
	return p;
}

static void onair_defaults(obs_data_t *s)
{
	struct obs_video_info ovi;
	bool have = obs_get_video_info(&ovi);
	obs_data_set_default_string(s, "shape", "pill");
	obs_data_set_default_bool(s, "fill_when_live", false);
	obs_data_set_default_int(s, "width", have ? ovi.base_width : 1920);
	obs_data_set_default_int(s, "height", have ? ovi.base_height : 1080);
	obs_data_set_default_string(s, "word_live", "Live");
	obs_data_set_default_string(s, "word_rec", "Rec");
	obs_data_set_default_string(s, "word_both", "Live · Rec");
	obs_data_set_default_string(s, "word_off", "Off air");
	obs_data_set_default_bool(s, "hide_when_off", false);
	obs_data_set_default_bool(s, "breathe", true);
	sbk_look_defaults(s);
	sbk_anim_defaults(s, "fade");
}

struct obs_source_info sbk_onair_info = {
	.id = "sbk_onair",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
	.get_name = onair_name,
	.create = onair_create,
	.destroy = onair_destroy,
	.update = onair_update,
	.get_defaults = onair_defaults,
	.get_properties = onair_properties,
	.get_width = onair_width,
	.get_height = onair_height,
	.video_tick = onair_tick,
	.video_render = onair_render,
	.show = onair_show,
	.enum_active_sources = onair_enum,
	.icon_type = OBS_ICON_TYPE_COLOR,
};
