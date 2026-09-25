/*  SBK Prompt — the like-and-subscribe card, on a timer.

    Asking is the part everyone forgets and nobody wants to say out loud for the
    ninth time. This says it instead: a small card slides in from an edge, holds
    for a few seconds, and slides back out, every couple of minutes, working
    through whatever lines you give it.

    Two decisions make it bearable rather than irritating. It is off screen
    between showings — genuinely off, not faded to nothing behind your camera —
    and it can be told to stay quiet unless you are actually live, so a
    rehearsal is not spent being asked to subscribe. There is a hotkey to bring
    it in now, for the moment you have just said something worth pinning it to.

    A line is `mark: Title | Body`. The mark is any of the kit's generic marks
    (see sbk-glyph.h); leave it off and the card uses the heart.  */

#include "sbk-common.h"
#include "sbk-glyph.h"
#include "sbk-text.h"
#include "sbk-stage.h"
#include "sbk-state.h"

#define MAX_LINES 8

/* seconds after the source is shown before the first card comes in */
#define SBK_PROMPT_FIRST 2.5f

enum prompt_edge { PE_LEFT = 0, PE_RIGHT, PE_TOP, PE_BOTTOM };

enum prompt_phase { PP_AWAY = 0, PP_IN, PP_HOLD, PP_OUT };

struct line {
	char *title;
	char *body;
	enum sbk_glyph glyph;
};

struct prompt {
	obs_source_t *self;
	struct sbk_look look;
	struct sbk_stage stage;
	gs_effect_t *card_fx, *glyph_fx;

	struct line line[MAX_LINES];
	int n, at;
	struct sbk_text title, body;
	char *s_lines;

	enum sbk_surface surf;
	enum prompt_edge edge;
	float every, hold, travel;
	int width;
	bool only_live;

	enum prompt_phase phase;
	float t;      /* seconds in the current phase */
	float waited; /* seconds since the last showing ended */

	obs_hotkey_id hk_now;
	uint32_t cx, cy;
	float title_y, body_y, mark_d;
};

static const char *prompt_label(void *u)
{
	UNUSED_PARAMETER(u);
	return obs_module_text("SBK.Prompt");
}

static void clear_lines(struct prompt *p)
{
	for (int i = 0; i < p->n; i++) {
		bfree(p->line[i].title);
		bfree(p->line[i].body);
	}
	memset(p->line, 0, sizeof(p->line));
	p->n = 0;
}

static void parse_lines(struct prompt *p)
{
	clear_lines(p);
	const char *s = p->s_lines ? p->s_lines : "";
	while (*s && p->n < MAX_LINES) {
		const char *nl = strchr(s, '\n');
		size_t len = nl ? (size_t)(nl - s) : strlen(s);
		while (len && (s[len - 1] == '\r' || s[len - 1] == ' '))
			len--;
		if (len) {
			char *raw = bstrdup_n(s, len);
			char *rest = raw;
			enum sbk_glyph g = SBK_GLYPH_HEART;

			/* an optional "mark:" in front, but only if it names one —
			   otherwise a title with a colon in it would lose its head */
			char *colon = strchr(raw, ':');
			if (colon) {
				*colon = 0;
				enum sbk_glyph maybe = sbk_glyph_from(raw);
				if (maybe != SBK_GLYPH_NONE || astrcmpi(raw, "none") == 0) {
					g = maybe;
					rest = colon + 1;
					while (*rest == ' ')
						rest++;
				} else {
					*colon = ':';
				}
			}

			struct line *ln = &p->line[p->n++];
			ln->glyph = g;
			char *bar = strchr(rest, '|');
			if (bar) {
				*bar = 0;
				const char *b = bar + 1;
				while (*b == ' ')
					b++;
				ln->body = bstrdup(b);
				size_t tl = strlen(rest);
				while (tl && rest[tl - 1] == ' ')
					rest[--tl] = 0;
				ln->title = bstrdup(rest);
			} else {
				ln->title = bstrdup(rest);
				ln->body = bstrdup("");
			}
			bfree(raw);
		}
		if (!nl)
			break;
		s = nl + 1;
	}
	if (p->at >= p->n)
		p->at = 0;
}

static void start_showing(struct prompt *p)
{
	if (p->n == 0)
		return;
	p->phase = PP_IN;
	p->t = 0.0f;
	p->waited = 0.0f;
}

static void hotkey_now(void *data, obs_hotkey_id id, obs_hotkey_t *key, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(key);
	if (!pressed)
		return;
	struct prompt *p = data;
	if (p->phase == PP_AWAY)
		start_showing(p);
}

static void prompt_update(void *data, obs_data_t *st)
{
	struct prompt *p = data;
	sbk_look_read(&p->look, st);

	const char *e = obs_data_get_string(st, "edge");
	p->edge = astrcmpi(e, "left") == 0	? PE_LEFT
		  : astrcmpi(e, "top") == 0	? PE_TOP
		  : astrcmpi(e, "bottom") == 0	? PE_BOTTOM
						: PE_RIGHT;
	p->surf = sbk_surface_from(obs_data_get_string(st, "variant"));
	p->every = (float)obs_data_get_double(st, "every");
	p->hold = (float)obs_data_get_double(st, "hold");
	p->travel = (float)obs_data_get_double(st, "travel");
	p->width = (int)obs_data_get_int(st, "width");
	p->only_live = obs_data_get_bool(st, "only_live");

	const char *lines = obs_data_get_string(st, "lines");
	if (!p->s_lines || strcmp(p->s_lines, lines) != 0) {
		bfree(p->s_lines);
		p->s_lines = bstrdup(lines);
		parse_lines(p);
	}
}

static void *prompt_create(obs_data_t *st, obs_source_t *source)
{
	struct prompt *p = bzalloc(sizeof(*p));
	p->self = source;
	p->card_fx = sbk_load_effect("effects/card.effect");
	p->glyph_fx = sbk_load_effect("effects/glyph.effect");
	sbk_stage_init(&p->stage);
	prompt_update(p, st);
	p->hk_now = obs_hotkey_register_source(source, "SBK.Prompt.Now", "Show the prompt now", hotkey_now, p);
	/* The first one comes in almost at once. Someone who has just added the
	   source needs to see what they configured; waiting three minutes to find
	   out whether it works reads as broken. After that it keeps its own time. */
	p->waited = fmaxf(0.0f, p->every - SBK_PROMPT_FIRST);
	return p;
}

static void prompt_destroy(void *data)
{
	struct prompt *p = data;
	if (p->hk_now != OBS_INVALID_HOTKEY_ID)
		obs_hotkey_unregister(p->hk_now);
	sbk_text_free(&p->title);
	sbk_text_free(&p->body);
	clear_lines(p);
	sbk_stage_free(&p->stage);
	sbk_free_effect(&p->card_fx);
	sbk_free_effect(&p->glyph_fx);
	bfree(p->s_lines);
	bfree(p);
}

static void prompt_tick(void *data, float seconds)
{
	struct prompt *p = data;
	const struct sbk_look *l = &p->look;
	const float u = sbk_u(l);
	const bool bare = p->surf == SBK_SURF_NONE;

	if (p->n == 0) {
		p->cx = p->cy = 0;
		return;
	}

	const float trav = fmaxf(0.05f, p->travel);
	p->t += seconds;
	switch (p->phase) {
	case PP_AWAY:
		p->waited += seconds;
		if (p->every > 0.5f && p->waited >= p->every &&
		    (!p->only_live || strcmp(sbk_state_word(), "off") != 0))
			start_showing(p);
		break;
	case PP_IN:
		if (p->t >= trav) {
			p->phase = PP_HOLD;
			p->t = 0.0f;
		}
		break;
	case PP_HOLD:
		if (p->t >= fmaxf(0.5f, p->hold)) {
			p->phase = PP_OUT;
			p->t = 0.0f;
		}
		break;
	case PP_OUT:
		if (p->t >= trav) {
			p->phase = PP_AWAY;
			p->t = 0.0f;
			p->waited = 0.0f;
			p->at = (p->at + 1) % p->n; /* the next line next time */
		}
		break;
	}

	const struct line *ln = &p->line[p->at];
	p->mark_d = 9.0f * u;
	const float pad_x = bare ? 0.0f : 5.0f * u, pad_y = bare ? 0.0f : 4.5f * u;
	int wrap = p->width > 0 ? (int)((float)p->width - pad_x * 2.0f - p->mark_d - 3.0f * u) : 0;
	if (wrap > 0 && wrap < 80)
		wrap = 80;

	sbk_text_set_ex(&p->title, ln->title, l->face, "Bold", (int)(5.4f * u),
			sbk_surface_ink(p->surf, l, false), wrap);
	sbk_text_set_ex(&p->body, ln->body, l->face, "Regular", (int)(4.4f * u),
			sbk_alpha(sbk_surface_ink(p->surf, l, true), 0.78f), wrap);

	float th = (float)sbk_text_h(&p->title);
	float bh = p->body.text.len ? (float)sbk_text_h(&p->body) + 1.0f * u : 0.0f;
	float text_h = th + bh;
	float h = fmaxf(text_h, p->mark_d);

	p->title_y = pad_y + (h - text_h) * 0.5f;
	p->body_y = p->title_y + th + 1.0f * u;

	float text_w = fmaxf((float)sbk_text_w(&p->title), (float)sbk_text_w(&p->body));
	float w = p->width > 0 ? (float)p->width : pad_x * 2.0f + p->mark_d + 3.0f * u + text_w;
	p->cx = (uint32_t)(w + 0.5f);
	p->cy = (uint32_t)(h + pad_y * 2.0f + 0.5f);
}

static uint32_t prompt_width(void *d) { return ((struct prompt *)d)->cx; }
static uint32_t prompt_height(void *d) { return ((struct prompt *)d)->cy; }

/* 0 fully away, 1 fully in */
static float prompt_progress(const struct prompt *p)
{
	const float trav = fmaxf(0.05f, p->travel);
	switch (p->phase) {
	case PP_IN:
		return sbk_ease_out(sbk_clampf(p->t / trav, 0.0f, 1.0f));
	case PP_HOLD:
		return 1.0f;
	case PP_OUT:
		return 1.0f - sbk_ease_out(sbk_clampf(p->t / trav, 0.0f, 1.0f));
	default:
		return 0.0f;
	}
}

static void prompt_render(void *data, gs_effect_t *unused)
{
	UNUSED_PARAMETER(unused);
	struct prompt *p = data;
	const float k = prompt_progress(p);
	if (k <= 0.001f || !p->card_fx || !p->glyph_fx)
		return;
	if (!sbk_stage_begin(&p->stage, p->cx, p->cy))
		return;

	const struct sbk_look *l = &p->look;
	const float u = sbk_u(l);
	const bool bare = p->surf == SBK_SURF_NONE;
	const float pad_x = bare ? 0.0f : 5.0f * u;

	sbk_surface_draw(p->card_fx, p->surf, l, 0, 0, (float)p->cx, (float)p->cy, 4.0f * u);

	const struct line *ln = &p->line[p->at];
	float my = ((float)p->cy - p->mark_d) * 0.5f;
	sbk_fill(p->card_fx, pad_x, my, p->mark_d, p->mark_d, p->mark_d * 0.30f, sbk_alpha(l->accent, 0.20f));
	sbk_glyph_draw(p->glyph_fx, ln->glyph, pad_x + p->mark_d * 0.24f, my + p->mark_d * 0.24f,
		       p->mark_d * 0.52f, l->accent, fmaxf(1.5f, 0.55f * u));

	float tx = pad_x + p->mark_d + 3.0f * u;
	sbk_text_draw(&p->title, tx, p->title_y);
	if (p->body.text.len)
		sbk_text_draw(&p->body, tx, p->body_y);

	sbk_stage_end(&p->stage);

	/* slides in from its edge and back out the same way */
	const float away = 1.0f - k;
	struct sbk_anim_out a = {k, 0.0f, 0.0f, 1.0f};
	switch (p->edge) {
	case PE_LEFT:
		a.dx = -((float)p->cx + 6.0f * u) * away;
		break;
	case PE_RIGHT:
		a.dx = ((float)p->cx + 6.0f * u) * away;
		break;
	case PE_TOP:
		a.dy = -((float)p->cy + 6.0f * u) * away;
		break;
	case PE_BOTTOM:
		a.dy = ((float)p->cy + 6.0f * u) * away;
		break;
	}
	sbk_stage_present_anim(&p->stage, a);
}

static void prompt_show(void *d)
{
	struct prompt *p = d;
	p->phase = PP_AWAY;
	p->t = 0.0f;
	p->waited = fmaxf(0.0f, p->every - SBK_PROMPT_FIRST);
}

static void prompt_enum(void *d, obs_source_enum_proc_t cb, void *param)
{
	struct prompt *p = d;
	sbk_text_enum(&p->title, p->self, cb, param);
	sbk_text_enum(&p->body, p->self, cb, param);
}

static obs_properties_t *prompt_properties(void *data)
{
	UNUSED_PARAMETER(data);
	obs_properties_t *p = obs_properties_create();
	obs_property_t *tl = obs_properties_add_text(p, "lines", "What it asks", OBS_TEXT_MULTILINE);
	obs_property_set_long_description(tl, "One per line, as mark: Title | Body. The mark is optional — "
					      "heart, bell, star, share, plus, chat and the rest.");

	obs_property_t *e = obs_properties_add_list(p, "edge", "Comes in from", OBS_COMBO_TYPE_LIST,
						    OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(e, "The left", "left");
	obs_property_list_add_string(e, "The right", "right");
	obs_property_list_add_string(e, "The top", "top");
	obs_property_list_add_string(e, "The bottom", "bottom");

	obs_properties_add_float_slider(p, "every", "Every (seconds)", 15.0, 900.0, 5.0);
	obs_properties_add_float_slider(p, "hold", "Stays for (seconds)", 1.0, 30.0, 0.5);
	obs_properties_add_float_slider(p, "travel", "Slide takes (seconds)", 0.1, 1.5, 0.05);
	obs_properties_add_int(p, "width", "Width (0 to fit the words)", 0, 1200, 10);
	obs_properties_add_bool(p, "only_live", "Only when on air");
	sbk_surface_list(p, "variant", "Variant");
	obs_properties_add_text(p, "hint",
				"It is genuinely off screen between showings, not parked behind something at "
				"zero opacity. Hotkey: Show the prompt now.",
				OBS_TEXT_INFO);
	sbk_look_props(p, true);
	return p;
}

static void prompt_defaults(obs_data_t *st)
{
	obs_data_set_default_string(st, "lines",
				    "heart: Enjoying this? | A like costs you nothing and helps a lot.\n"
				    "bell: Subscribe | There is a new build every Thursday.\n"
				    "share: Pass it on | Send this to someone who is stuck on the same thing.\n"
				    "chat: Questions | Drop them in the chat, I read all of them.");
	obs_data_set_default_string(st, "edge", "right");
	obs_data_set_default_double(st, "every", 180.0);
	obs_data_set_default_double(st, "hold", 8.0);
	obs_data_set_default_double(st, "travel", 0.45);
	obs_data_set_default_int(st, "width", 520);
	obs_data_set_default_bool(st, "only_live", false);
	obs_data_set_default_string(st, "variant", "card");
	sbk_look_defaults(st);
}

struct obs_source_info sbk_prompt_info = {
	.id = "sbk_prompt",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
	.get_name = prompt_label,
	.create = prompt_create,
	.destroy = prompt_destroy,
	.update = prompt_update,
	.get_defaults = prompt_defaults,
	.get_properties = prompt_properties,
	.get_width = prompt_width,
	.get_height = prompt_height,
	.video_tick = prompt_tick,
	.video_render = prompt_render,
	.show = prompt_show,
	.enum_active_sources = prompt_enum,
	.icon_type = OBS_ICON_TYPE_TEXT,
};
