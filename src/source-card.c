/*  SBK Card — the announcement: an eyebrow with the recording-light dot, a
    title, a line of body, and a row of chips. Starting soon, Be right back and
    Ending are all this card with different words.  */

#include "sbk-common.h"
#include "sbk-text.h"
#include "sbk-stage.h"
#include "sbk-anim.h"

#define MAX_CHIPS 8

struct card {
	obs_source_t *self;
	struct sbk_look look;
	struct sbk_anim anim;
	struct sbk_stage stage;
	gs_effect_t *card_fx;
	struct sbk_text eyebrow, title, body, chips[MAX_CHIPS];
	char *s_eyebrow, *s_title, *s_body, *s_chips;
	char *chip[MAX_CHIPS];
	int n_chips;
	enum sbk_surface surf;
	bool dot, centred, accent_rule;
	float t;
	uint32_t width, cx, cy;
	/* layout, measured in tick and drawn in render */
	float y_eyebrow, y_title, y_body, y_chips;
	float chip_x[MAX_CHIPS], chip_y[MAX_CHIPS], chip_w[MAX_CHIPS], chip_h;
};

static const char *card_name(void *u)
{
	UNUSED_PARAMETER(u);
	return obs_module_text("SBK.Card");
}

static void split_chips(struct card *c)
{
	for (int i = 0; i < c->n_chips; i++) {
		bfree(c->chip[i]);
		c->chip[i] = NULL;
	}
	c->n_chips = 0;
	const char *p = c->s_chips ? c->s_chips : "";
	while (*p && c->n_chips < MAX_CHIPS) {
		const char *bar = strchr(p, '|');
		size_t len = bar ? (size_t)(bar - p) : strlen(p);
		const char *s = p;
		while (len && *s == ' ') { s++; len--; }
		while (len && s[len - 1] == ' ') len--;
		if (len)
			c->chip[c->n_chips++] = bstrdup_n(s, len);
		if (!bar)
			break;
		p = bar + 1;
	}
}

static void card_update(void *data, obs_data_t *s)
{
	struct card *c = data;
	sbk_look_read(&c->look, s);
	sbk_anim_read(&c->anim, s, 6.0f * sbk_u(&c->look));
	bfree(c->s_eyebrow);
	bfree(c->s_title);
	bfree(c->s_body);
	bfree(c->s_chips);
	c->s_eyebrow = bstrdup(obs_data_get_string(s, "eyebrow"));
	c->s_title = bstrdup(obs_data_get_string(s, "title"));
	c->s_body = bstrdup(obs_data_get_string(s, "body"));
	c->s_chips = bstrdup(obs_data_get_string(s, "chips"));
	c->dot = obs_data_get_bool(s, "dot");
	c->surf = sbk_surface_from(obs_data_get_string(s, "variant"));
	c->centred = astrcmpi(obs_data_get_string(s, "align"), "centre") == 0;
	c->accent_rule = astrcmpi(obs_data_get_string(s, "variant"), "split") == 0;
	if (c->accent_rule)
		c->surf = SBK_SURF_CARD;
	c->width = (uint32_t)obs_data_get_int(s, "width");
	split_chips(c);
}

static void *card_create(obs_data_t *s, obs_source_t *source)
{
	struct card *c = bzalloc(sizeof(*c));
	c->self = source;
	c->card_fx = sbk_load_effect("effects/card.effect");
	sbk_stage_init(&c->stage);
	card_update(c, s);
	sbk_anim_play(&c->anim);
	return c;
}

static void card_destroy(void *data)
{
	struct card *c = data;
	sbk_text_free(&c->eyebrow);
	sbk_text_free(&c->title);
	sbk_text_free(&c->body);
	for (int i = 0; i < MAX_CHIPS; i++) {
		sbk_text_free(&c->chips[i]);
		bfree(c->chip[i]);
	}
	sbk_stage_free(&c->stage);
	sbk_free_effect(&c->card_fx);
	bfree(c->s_eyebrow);
	bfree(c->s_title);
	bfree(c->s_body);
	bfree(c->s_chips);
	bfree(c);
}

static void card_tick(void *data, float seconds)
{
	struct card *c = data;
	c->t += seconds;
	sbk_anim_tick(&c->anim, seconds);
	const struct sbk_look *l = &c->look;
	const float u = sbk_u(l);

	float pad_x = c->surf == SBK_SURF_NONE ? 0.0f : 14.0f * u;
	float pad_y = c->surf == SBK_SURF_NONE ? 0.0f : 12.0f * u;
	if (c->accent_rule)
		pad_x += 4.0f * u;
	int inner = (int)((float)c->width - pad_x * 2.0f);
	if (inner < 40)
		inner = 40;

	const bool bare = c->surf == SBK_SURF_NONE;
	struct vec4 ink = sbk_surface_ink(c->surf, l, false);
	struct vec4 ink_dim = sbk_surface_ink(c->surf, l, true);
	struct vec4 eyebrow_ink = c->surf == SBK_SURF_ACCENT ? l->on_accent : l->accent;

	struct dstr eb = {0};
	dstr_copy(&eb, c->s_eyebrow ? c->s_eyebrow : "");
	sbk_caps(&eb);
	sbk_text_set_full(&c->eyebrow, eb.array ? eb.array : "", l->face, "SemiBold", (int)(4.5f * u), eyebrow_ink, 0, bare);
	dstr_free(&eb);
	/* Wrapping costs the measurement. A text_ft2 source given a custom width
	   reports THAT width, not the width of its glyphs, and lays the type out
	   from the left inside it — so a centred card measured this way would come
	   out left-aligned and the centring maths would be a no-op. Set it
	   unwrapped first, and only turn wrapping on for the line that actually
	   overruns the measure. */
	sbk_text_set_full(&c->title, c->s_title, l->face, "Bold", (int)(16.0f * u), ink, 0, bare);
	if ((int)sbk_text_w(&c->title) > inner)
		sbk_text_set_full(&c->title, c->s_title, l->face, "Bold", (int)(16.0f * u), ink, inner, bare);
	sbk_text_set_full(&c->body, c->s_body, l->face, "Regular", (int)(6.5f * u), ink_dim, 0, bare);
	if ((int)sbk_text_w(&c->body) > inner)
		sbk_text_set_full(&c->body, c->s_body, l->face, "Regular", (int)(6.5f * u), ink_dim, inner, bare);
	for (int i = 0; i < MAX_CHIPS; i++)
		sbk_text_set_full(&c->chips[i], i < c->n_chips ? c->chip[i] : "", l->face, "Medium", (int)(5.0f * u),
				    ink_dim, 0, bare);

	float y = pad_y;
	if (c->eyebrow.text.len) {
		c->y_eyebrow = y;
		y += (float)sbk_text_h(&c->eyebrow) + 4.0f * u;
	}
	c->y_title = y;
	y += (float)sbk_text_h(&c->title);
	if (c->body.text.len) {
		y += 3.0f * u;
		c->y_body = y;
		y += (float)sbk_text_h(&c->body);
	}
	if (c->n_chips) {
		y += 8.0f * u;
		c->y_chips = y;
		float x = pad_x, row_h = 0.0f;
		c->chip_h = (float)sbk_text_h(&c->chips[0]) + 3.0f * u;
		for (int i = 0; i < c->n_chips; i++) {
			float w = (float)sbk_text_w(&c->chips[i]) + 7.0f * u;
			if (x + w > pad_x + (float)inner && x > pad_x) {
				x = pad_x;
				y += c->chip_h + 2.0f * u;
			}
			c->chip_x[i] = x;
			c->chip_y[i] = y;
			c->chip_w[i] = w;
			x += w + 2.0f * u;
			row_h = c->chip_h;
		}
		y += row_h;
	}
	y += pad_y;
	c->cx = c->width;
	c->cy = (uint32_t)(y + 0.5f);
}

static uint32_t card_width(void *d) { return ((struct card *)d)->cx; }
static uint32_t card_height(void *d) { return ((struct card *)d)->cy; }

static void card_render(void *data, gs_effect_t *unused)
{
	UNUSED_PARAMETER(unused);
	struct card *c = data;
	if (!c->card_fx || !sbk_stage_begin(&c->stage, c->cx, c->cy))
		return;
	const struct sbk_look *l = &c->look;
	const float u = sbk_u(l);
	struct vec4 none = {{{0.0f, 0.0f, 0.0f, 0.0f}}};
	float pad_x = c->surf == SBK_SURF_NONE ? 0.0f : 14.0f * u;
	if (c->accent_rule)
		pad_x += 4.0f * u;

	sbk_surface_draw(c->card_fx, c->surf, l, 0, 0, (float)c->cx, (float)c->cy, 6.0f * u);
	if (c->accent_rule) {
		/* a stripe down the inside edge — the split variant */
		float w = 1.5f * u;
		sbk_fill(c->card_fx, 8.0f * u, 10.0f * u, w, (float)c->cy - 20.0f * u, w * 0.5f, l->accent);
	}

	float mid = (float)c->cx * 0.5f;
	float x = pad_x;
	if (c->eyebrow.text.len) {
		if (c->centred) {
			float total = (float)sbk_text_w(&c->eyebrow) + (c->dot ? 2.5f * u + 2.5f * u : 0.0f);
			x = mid - total * 0.5f;
		}
		float eh = (float)sbk_text_h(&c->eyebrow);
		if (c->dot) {
			float d = 2.5f * u;
			float pulse = 0.5f + 0.5f * sinf(c->t * 6.2831853f / SBK_PULSE_SECS);
			sbk_dot(c->card_fx, x, c->y_eyebrow + (eh - d) * 0.5f, d, l->accent, 2.0f * u, 0.3f + 0.4f * pulse);
			x += d + 2.5f * u;
		}
		sbk_text_draw(&c->eyebrow, x, c->y_eyebrow);
	}
	sbk_text_draw(&c->title, c->centred ? mid - (float)sbk_text_w(&c->title) * 0.5f : pad_x, c->y_title);
	if (c->body.text.len)
		sbk_text_draw(&c->body, c->centred ? mid - (float)sbk_text_w(&c->body) * 0.5f : pad_x, c->y_body);
	for (int i = 0; i < c->n_chips; i++) {
		float cx = c->chip_x[i];
		if (c->centred) {
			/* shift the whole row this chip belongs to */
			float row_w = 0.0f, row_start = -1.0f;
			for (int j = 0; j < c->n_chips; j++)
				if (c->chip_y[j] == c->chip_y[i]) {
					if (row_start < 0.0f)
						row_start = c->chip_x[j];
					row_w = c->chip_x[j] + c->chip_w[j] - row_start;
				}
			cx += mid - (row_start + row_w * 0.5f);
		}
		struct vec4 chip_fill = c->surf == SBK_SURF_ACCENT ? sbk_alpha(l->on_accent, 0.18f) : l->wash;
		sbk_card(c->card_fx, cx, c->chip_y[i], c->chip_w[i], c->chip_h, c->chip_h * 0.5f, chip_fill,
			   l->glass_line, 1.0f, none, 0.0f);
		sbk_text_draw(&c->chips[i], cx + 3.5f * u, c->chip_y[i] + 1.5f * u);
	}

	sbk_stage_end(&c->stage);
	struct sbk_anim_out a = sbk_anim_eval(&c->anim);
	sbk_stage_present(&c->stage, a.alpha, a.dx, a.dy);
}

static void card_show(void *d) { sbk_anim_on_show(&((struct card *)d)->anim); }
static void card_enum(void *d, obs_source_enum_proc_t cb, void *p)
{
	struct card *c = d;
	sbk_text_enum(&c->eyebrow, c->self, cb, p);
	sbk_text_enum(&c->title, c->self, cb, p);
	sbk_text_enum(&c->body, c->self, cb, p);
	for (int i = 0; i < MAX_CHIPS; i++)
		sbk_text_enum(&c->chips[i], c->self, cb, p);
}

static obs_properties_t *card_properties(void *data)
{
	UNUSED_PARAMETER(data);
	obs_properties_t *p = obs_properties_create();
	obs_properties_add_text(p, "eyebrow", "Eyebrow", OBS_TEXT_DEFAULT);
	obs_properties_add_bool(p, "dot", "The recording light before it");
	obs_properties_add_text(p, "title", "Title", OBS_TEXT_DEFAULT);
	obs_properties_add_text(p, "body", "Body", OBS_TEXT_MULTILINE);
	obs_properties_add_text(p, "chips", "Chips, separated with |", OBS_TEXT_DEFAULT);
	obs_properties_add_int(p, "width", "Width", 320, 3840, 2);
	obs_property_t *v = obs_properties_add_list(p, "variant", "Variant", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(v, "Panel — on glass", "card");
	obs_property_list_add_string(v, "Split — an accent stripe inside", "split");
	obs_property_list_add_string(v, "Outline", "outline");
	obs_property_list_add_string(v, "Accent — the whole card", "accent");
	obs_property_list_add_string(v, "Plain — straight on the video", "none");
	obs_property_t *al = obs_properties_add_list(p, "align", "Align", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(al, "Left", "left");
	obs_property_list_add_string(al, "Centre", "centre");
	sbk_look_props(p, false);
	sbk_anim_props(p);
	return p;
}

static void card_defaults(obs_data_t *s)
{
	obs_data_set_default_string(s, "eyebrow", "Starting soon");
	obs_data_set_default_bool(s, "dot", true);
	obs_data_set_default_string(s, "title", "Building a Salesforce app live");
	obs_data_set_default_string(s, "body", "Grab a coffee. We begin at the top of the hour.");
	obs_data_set_default_string(s, "chips", "@imswarnil | youtube.com/@imswarnil | imswarnil.com");
	obs_data_set_default_int(s, "width", 1100);
	obs_data_set_default_string(s, "variant", "card");
	obs_data_set_default_string(s, "align", "left");
	sbk_look_defaults(s);
	sbk_anim_defaults(s, "up");
}

struct obs_source_info sbk_card_info = {
	.id = "sbk_card",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
	.get_name = card_name,
	.create = card_create,
	.destroy = card_destroy,
	.update = card_update,
	.get_defaults = card_defaults,
	.get_properties = card_properties,
	.get_width = card_width,
	.get_height = card_height,
	.video_tick = card_tick,
	.video_render = card_render,
	.show = card_show,
	.enum_active_sources = card_enum,
	.icon_type = OBS_ICON_TYPE_TEXT,
};
