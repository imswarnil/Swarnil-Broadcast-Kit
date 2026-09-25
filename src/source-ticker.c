/*  SBK Ticker — a strip along the foot of the screen: a tag in the accent,
    then items sliding past.

    Items are kept apart rather than joined into one string, so each can be
    drawn as its own chip, and the separator between them is a drawn dot rather
    than a character — which means it is the accent, and the same size at every
    scale.

    The lane is its own render target. That is what clips the text, and what
    lets the ends fade instead of being cut off square.  */

#include "sbk-common.h"
#include "sbk-text.h"
#include "sbk-stage.h"
#include "sbk-anim.h"

#define MAX_ITEMS 12

enum ticker_variant { TICK_STRIP = 0, TICK_BARE, TICK_CHIPS };

struct ticker {
	obs_source_t *self;
	struct sbk_look look;
	struct sbk_anim anim;
	struct sbk_stage stage, lane;
	gs_effect_t *card_fx;
	struct sbk_text tag, item[MAX_ITEMS];
	char *s_tag, *s_text;
	char *raw[MAX_ITEMS];
	int n_items;
	enum ticker_variant variant;
	float speed, x, run;   /* run = the width of one pass of the items */
	bool rtl, fade;
	uint32_t width, cx, cy;
};

static const char *ticker_name(void *u)
{
	UNUSED_PARAMETER(u);
	return obs_module_text("SBK.Ticker");
}

static enum ticker_variant variant_from(const char *id)
{
	if (id && astrcmpi(id, "bare") == 0)
		return TICK_BARE;
	if (id && astrcmpi(id, "chips") == 0)
		return TICK_CHIPS;
	return TICK_STRIP;
}

static void split_items(struct ticker *t)
{
	for (int i = 0; i < t->n_items; i++) {
		bfree(t->raw[i]);
		t->raw[i] = NULL;
	}
	t->n_items = 0;
	const char *p = t->s_text ? t->s_text : "";
	while (*p && t->n_items < MAX_ITEMS) {
		const char *bar = strchr(p, '|');
		size_t len = bar ? (size_t)(bar - p) : strlen(p);
		const char *s = p;
		while (len && (*s == ' ' || *s == '\n')) { s++; len--; }
		while (len && (s[len - 1] == ' ' || s[len - 1] == '\n')) len--;
		if (len)
			t->raw[t->n_items++] = bstrdup_n(s, len);
		if (!bar)
			break;
		p = bar + 1;
	}
}

static void ticker_update(void *data, obs_data_t *s)
{
	struct ticker *t = data;
	sbk_look_read(&t->look, s);
	sbk_anim_read(&t->anim, s, 4.0f * sbk_u(&t->look));
	bfree(t->s_tag);
	bfree(t->s_text);
	t->s_tag = bstrdup(obs_data_get_string(s, "tag"));
	t->s_text = bstrdup(obs_data_get_string(s, "text"));
	split_items(t);
	t->variant = variant_from(obs_data_get_string(s, "variant"));
	t->speed = (float)obs_data_get_double(s, "speed");
	t->width = (uint32_t)obs_data_get_int(s, "width");
	t->rtl = astrcmpi(obs_data_get_string(s, "direction"), "right") == 0;
	t->fade = obs_data_get_bool(s, "fade");
}

static void *ticker_create(obs_data_t *s, obs_source_t *source)
{
	struct ticker *t = bzalloc(sizeof(*t));
	t->self = source;
	t->card_fx = sbk_load_effect("effects/card.effect");
	sbk_stage_init(&t->stage);
	sbk_stage_init(&t->lane);
	ticker_update(t, s);
	sbk_anim_play(&t->anim);
	return t;
}

static void ticker_destroy(void *data)
{
	struct ticker *t = data;
	sbk_text_free(&t->tag);
	for (int i = 0; i < MAX_ITEMS; i++) {
		sbk_text_free(&t->item[i]);
		bfree(t->raw[i]);
	}
	sbk_stage_free(&t->stage);
	sbk_stage_free(&t->lane);
	sbk_free_effect(&t->card_fx);
	bfree(t->s_tag);
	bfree(t->s_text);
	bfree(t);
}

/* one item's width including the space that follows it */
static float item_span(struct ticker *t, int i, float u)
{
	float w = (float)sbk_text_w(&t->item[i]);
	return t->variant == TICK_CHIPS ? w + 7.0f * u + 2.5f * u : w + 9.0f * u;
}

static void ticker_tick(void *data, float seconds)
{
	struct ticker *t = data;
	sbk_anim_tick(&t->anim, seconds);
	const struct sbk_look *l = &t->look;
	const float u = sbk_u(l);
	const bool bare = t->variant == TICK_BARE;

	struct dstr tag = {0};
	dstr_copy(&tag, t->s_tag ? t->s_tag : "");
	sbk_caps(&tag);
	sbk_text_set(&t->tag, tag.array ? tag.array : "", l->face, "Bold", (int)(4.5f * u), l->on_accent);
	dstr_free(&tag);

	for (int i = 0; i < MAX_ITEMS; i++)
		sbk_text_set_full(&t->item[i], i < t->n_items ? t->raw[i] : "", l->face, "Medium", (int)(5.5f * u),
				    l->ink, 0, bare);

	t->cx = t->width;
	t->cy = (uint32_t)(14.0f * u + 0.5f);

	t->run = 0.0f;
	for (int i = 0; i < t->n_items; i++)
		t->run += item_span(t, i, u);

	float dx = t->speed * l->scale * seconds;
	t->x += t->rtl ? dx : -dx;
	if (t->run > 0.0f) {
		if (t->x <= -t->run)
			t->x += t->run;
		if (t->x >= 0.0f)
			t->x -= t->run;
	}
}

static uint32_t ticker_width(void *d) { return ((struct ticker *)d)->cx; }
static uint32_t ticker_height(void *d) { return ((struct ticker *)d)->cy; }

static void ticker_render(void *data, gs_effect_t *unused)
{
	UNUSED_PARAMETER(unused);
	struct ticker *t = data;
	if (!t->card_fx || !sbk_stage_begin(&t->stage, t->cx, t->cy))
		return;
	const struct sbk_look *l = &t->look;
	const float u = sbk_u(l);
	struct vec4 none = SBK_NONE;

	if (t->variant == TICK_STRIP)
		sbk_card(t->card_fx, 0, 0, (float)t->cx, (float)t->cy, 3.0f * u, l->glass, l->glass_line, 1.0f, none, 0.0f);

	float inset = 3.0f * u;
	float lane_x = inset;
	if (t->tag.text.len) {
		float tw = (float)sbk_text_w(&t->tag), th = (float)sbk_text_h(&t->tag);
		float pw = tw + 8.0f * u, ph = th + 3.0f * u;
		float py = ((float)t->cy - ph) * 0.5f;
		sbk_fill(t->card_fx, inset, py, pw, ph, ph * 0.5f, l->accent);
		sbk_text_draw(&t->tag, inset + 4.0f * u, py + 1.5f * u);
		lane_x = inset + pw + 4.0f * u;
	}

	float lane_w = (float)t->cx - lane_x - inset;
	if (lane_w > 1.0f && t->run > 0.0f && sbk_stage_begin(&t->lane, (uint32_t)lane_w, t->cy)) {
		/* two passes of the run, so whatever leaves one end is already
		   entering the other */
		for (float base = t->x; base < lane_w; base += t->run) {
			float x = base;
			for (int i = 0; i < t->n_items; i++) {
				float iw = (float)sbk_text_w(&t->item[i]);
				float ih = (float)sbk_text_h(&t->item[i]);
				float y = ((float)t->cy - ih) * 0.5f;
				if (x + item_span(t, i, u) > 0.0f && x < lane_w) {
					if (t->variant == TICK_CHIPS) {
						float ph = ih + 3.0f * u;
						sbk_card(t->card_fx, x, ((float)t->cy - ph) * 0.5f, iw + 7.0f * u, ph,
							   ph * 0.5f, l->wash, l->glass_line, 1.0f, none, 0.0f);
						sbk_text_draw(&t->item[i], x + 3.5f * u, y);
					} else {
						sbk_text_draw(&t->item[i], x, y);
						/* the separator: the accent, not a character */
						float d = 1.5f * u;
						sbk_fill(t->card_fx, x + iw + 3.75f * u, ((float)t->cy - d) * 0.5f, d, d,
							   d * 0.5f, l->accent);
					}
				}
				x += item_span(t, i, u);
			}
		}
		sbk_stage_end(&t->lane);
		sbk_stage_present_ex(&t->lane, 1.0f, lane_x, 0.0f, t->fade ? 10.0f * u : 0.0f);
	}

	sbk_stage_end(&t->stage);
	struct sbk_anim_out a = sbk_anim_eval(&t->anim);
	sbk_stage_present(&t->stage, a.alpha, a.dx, a.dy);
}

static void ticker_show(void *d) { sbk_anim_on_show(&((struct ticker *)d)->anim); }
static void ticker_enum(void *d, obs_source_enum_proc_t cb, void *p)
{
	struct ticker *t = d;
	sbk_text_enum(&t->tag, t->self, cb, p);
	for (int i = 0; i < MAX_ITEMS; i++)
		sbk_text_enum(&t->item[i], t->self, cb, p);
}

static obs_properties_t *ticker_properties(void *data)
{
	UNUSED_PARAMETER(data);
	obs_properties_t *p = obs_properties_create();
	obs_properties_add_text(p, "tag", "Tag (empty for none)", OBS_TEXT_DEFAULT);
	obs_properties_add_text(p, "text", "Items, separated with |", OBS_TEXT_MULTILINE);
	obs_property_t *v = obs_properties_add_list(p, "variant", "Variant", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(v, "Strip — on glass", "strip");
	obs_property_list_add_string(v, "Bare — straight on the video", "bare");
	obs_property_list_add_string(v, "Chips — each item in its own pill", "chips");
	obs_property_t *d = obs_properties_add_list(p, "direction", "Direction", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(d, "Right to left", "left");
	obs_property_list_add_string(d, "Left to right", "right");
	obs_properties_add_float_slider(p, "speed", "Speed (px/s)", 20.0, 400.0, 5.0);
	obs_properties_add_int(p, "width", "Width", 200, 7680, 2);
	obs_properties_add_bool(p, "fade", "Fade the text out at both ends");
	sbk_look_props(p, false);
	sbk_anim_props(p);
	return p;
}

static void ticker_defaults(obs_data_t *s)
{
	obs_data_set_default_string(s, "tag", "Now");
	obs_data_set_default_string(s, "text", "Building a Salesforce app live | Questions in chat | imswarnil.com");
	obs_data_set_default_string(s, "variant", "strip");
	obs_data_set_default_string(s, "direction", "left");
	obs_data_set_default_double(s, "speed", 90.0);
	obs_data_set_default_int(s, "width", 1920);
	obs_data_set_default_bool(s, "fade", true);
	sbk_look_defaults(s);
	sbk_anim_defaults(s, "up");
}

struct obs_source_info sbk_ticker_info = {
	.id = "sbk_ticker",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
	.get_name = ticker_name,
	.create = ticker_create,
	.destroy = ticker_destroy,
	.update = ticker_update,
	.get_defaults = ticker_defaults,
	.get_properties = ticker_properties,
	.get_width = ticker_width,
	.get_height = ticker_height,
	.video_tick = ticker_tick,
	.video_render = ticker_render,
	.show = ticker_show,
	.enum_active_sources = ticker_enum,
	.icon_type = OBS_ICON_TYPE_TEXT,
};
