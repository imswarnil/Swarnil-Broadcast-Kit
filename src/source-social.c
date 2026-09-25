/*  SBK Social — where to find you, in three shapes.

    One source covers the three things a channel actually needs: a bar of every
    handle along an edge, a stack of them in a corner, and a lower third that
    shows one platform at a time and changes on a timer. The third is the one
    worth having. A row of six handles is read by nobody; the same six shown for
    eight seconds each, arriving with the kit's own motion, are read by everyone.

    Accounts are typed one per line as `platform: handle`. A known platform
    brings its colour and the closest generic mark with it; anything else is
    used as the name, in the accent. The marks are deliberately not logos — see
    sbk-glyph.h.  */

#include "sbk-common.h"
#include "sbk-glyph.h"
#include "sbk-text.h"
#include "sbk-stage.h"
#include "sbk-anim.h"

#define MAX_ACCOUNTS 8

enum social_mode { SM_ROTATE = 0, SM_BAR, SM_STACK };

struct account {
	char *name;
	char *handle;
	enum sbk_glyph glyph;
	struct vec4 colour;
	bool branded;
};

struct social {
	obs_source_t *self;
	struct sbk_look look;
	struct sbk_anim anim;
	struct sbk_stage stage;
	gs_effect_t *card_fx, *glyph_fx;

	struct account acct[MAX_ACCOUNTS];
	int n;

	struct sbk_text name[MAX_ACCOUNTS], handle[MAX_ACCOUNTS];
	char *s_accounts;
	enum social_mode mode;
	enum sbk_surface surf;
	bool brand, show_name;
	float rotate_s, since;
	int at;

	/* laid out in tick, drawn in render */
	float row_x[MAX_ACCOUNTS], row_y[MAX_ACCOUNTS], row_w[MAX_ACCOUNTS];
	uint32_t cx, cy;
};

static const char *social_name(void *u)
{
	UNUSED_PARAMETER(u);
	return obs_module_text("SBK.Social");
}

static void clear_accounts(struct social *s)
{
	for (int i = 0; i < s->n; i++) {
		bfree(s->acct[i].name);
		bfree(s->acct[i].handle);
	}
	memset(s->acct, 0, sizeof(s->acct));
	s->n = 0;
}

/* one line: "platform: handle", or "Any name: handle" for something we do not
   know about. A line with no colon is taken as a handle on its own. */
static void parse_accounts(struct social *s)
{
	clear_accounts(s);
	const char *p = s->s_accounts ? s->s_accounts : "";
	while (*p && s->n < MAX_ACCOUNTS) {
		const char *nl = strchr(p, '\n');
		size_t len = nl ? (size_t)(nl - p) : strlen(p);
		while (len && (p[len - 1] == '\r' || p[len - 1] == ' '))
			len--;
		if (len) {
			char *line = bstrdup_n(p, len);
			char *colon = strchr(line, ':');
			const char *left = "", *right = line;
			if (colon) {
				*colon = 0;
				left = line;
				right = colon + 1;
				while (*right == ' ')
					right++;
			}
			const struct sbk_platform *plat = sbk_platform_from(left);
			struct account *a = &s->acct[s->n++];
			a->handle = bstrdup(right);
			if (plat && plat->colour) {
				a->name = bstrdup(plat->name);
				a->glyph = sbk_glyph_from(plat->glyph);
				a->colour = sbk_vec(plat->colour);
				a->branded = true;
			} else {
				a->name = bstrdup(left);
				a->glyph = SBK_GLYPH_AT;
				a->branded = false;
			}
			bfree(line);
		}
		if (!nl)
			break;
		p = nl + 1;
	}
	if (s->at >= s->n)
		s->at = 0;
}

/*  A brand colour that cannot be seen is not a brand colour.

    Two of the platforms here are black, which is fine on their own white pages
    and invisible on a dark overlay. Anything too dark to read against the
    surface falls back to the Look's ink, so the mark is always there and the
    six that do have a usable colour still carry it.  */
static struct vec4 acct_colour(const struct social *s, int i)
{
	if (!s->brand || !s->acct[i].branded)
		return s->look.accent;
	struct vec4 c = s->acct[i].colour;
	float lum = 0.2126f * c.x + 0.7152f * c.y + 0.0722f * c.z;
	if (lum < 0.22f)
		return s->look.ink;
	return c;
}

static void social_update(void *data, obs_data_t *st)
{
	struct social *s = data;
	sbk_look_read(&s->look, st);
	sbk_anim_read(&s->anim, st, 4.0f * sbk_u(&s->look));

	const char *m = obs_data_get_string(st, "mode");
	s->mode = astrcmpi(m, "bar") == 0 ? SM_BAR : astrcmpi(m, "stack") == 0 ? SM_STACK : SM_ROTATE;
	s->surf = sbk_surface_from(obs_data_get_string(st, "variant"));
	s->brand = obs_data_get_bool(st, "brand");
	s->show_name = obs_data_get_bool(st, "show_name");
	s->rotate_s = (float)obs_data_get_double(st, "rotate");

	const char *acc = obs_data_get_string(st, "accounts");
	if (!s->s_accounts || strcmp(s->s_accounts, acc) != 0) {
		bfree(s->s_accounts);
		s->s_accounts = bstrdup(acc);
		parse_accounts(s);
	}
}

static void *social_create(obs_data_t *st, obs_source_t *source)
{
	struct social *s = bzalloc(sizeof(*s));
	s->self = source;
	s->card_fx = sbk_load_effect("effects/card.effect");
	s->glyph_fx = sbk_load_effect("effects/glyph.effect");
	sbk_stage_init(&s->stage);
	social_update(s, st);
	sbk_anim_play(&s->anim);
	return s;
}

static void social_destroy(void *data)
{
	struct social *s = data;
	for (int i = 0; i < MAX_ACCOUNTS; i++) {
		sbk_text_free(&s->name[i]);
		sbk_text_free(&s->handle[i]);
	}
	clear_accounts(s);
	sbk_stage_free(&s->stage);
	sbk_free_effect(&s->card_fx);
	sbk_free_effect(&s->glyph_fx);
	bfree(s->s_accounts);
	bfree(s);
}

static void social_tick(void *data, float seconds)
{
	struct social *s = data;
	sbk_anim_tick(&s->anim, seconds);
	const struct sbk_look *l = &s->look;
	const float u = sbk_u(l);
	const bool bare = s->surf == SBK_SURF_NONE;

	if (s->n == 0) {
		s->cx = s->cy = 0;
		return;
	}

	/* the rotating one changes account on its own clock, and replays the
	   arrival each time so a change reads as a change */
	if (s->mode == SM_ROTATE && s->n > 1 && s->rotate_s > 0.5f) {
		s->since += seconds;
		if (s->since >= s->rotate_s) {
			s->since = 0.0f;
			s->at = (s->at + 1) % s->n;
			sbk_anim_play(&s->anim);
		}
	}

	const float pad_x = bare ? 0.0f : 5.0f * u, pad_y = bare ? 0.0f : 3.5f * u;
	const float gap = 3.0f * u;

	if (s->mode == SM_ROTATE) {
		const int i = s->at;
		const float mark = 9.0f * u;
		sbk_text_set(&s->name[i], s->show_name ? s->acct[i].name : "", l->face, "SemiBold",
			     (int)(3.6f * u), sbk_alpha(sbk_surface_ink(s->surf, l, true), 0.75f));
		sbk_text_set(&s->handle[i], s->acct[i].handle, l->face, "Bold", (int)(6.4f * u),
			     sbk_surface_ink(s->surf, l, false));

		float th = (float)sbk_text_h(&s->handle[i]);
		if (s->name[i].text.len)
			th += (float)sbk_text_h(&s->name[i]) + 0.5f * u;
		float h = fmaxf(th, mark);
		float w = pad_x + mark + gap +
			  fmaxf((float)sbk_text_w(&s->handle[i]), (float)sbk_text_w(&s->name[i])) + pad_x;
		s->row_x[i] = pad_x;
		s->row_y[i] = pad_y;
		s->cx = (uint32_t)(w + 0.5f);
		s->cy = (uint32_t)(h + pad_y * 2.0f + 0.5f);
		return;
	}

	/* bar and stack share their row layout; only the axis differs */
	const float mark = 6.0f * u;
	float run = 0.0f, tallest = 0.0f;
	for (int i = 0; i < s->n; i++) {
		sbk_text_set(&s->name[i], "", l->face, "SemiBold", (int)(3.6f * u), l->ink);
		sbk_text_set(&s->handle[i], s->acct[i].handle, l->face, "SemiBold", (int)(4.4f * u),
			     sbk_surface_ink(s->surf, l, false));
		s->row_w[i] = mark + 2.0f * u + (float)sbk_text_w(&s->handle[i]);
		tallest = fmaxf(tallest, fmaxf((float)sbk_text_h(&s->handle[i]), mark));
		if (s->mode == SM_BAR) {
			s->row_x[i] = pad_x + run;
			s->row_y[i] = pad_y;
			run += s->row_w[i] + 6.0f * u;
		} else {
			s->row_x[i] = pad_x;
			s->row_y[i] = pad_y + run;
			run += fmaxf((float)sbk_text_h(&s->handle[i]), mark) + 3.5f * u;
		}
	}
	for (int i = s->n; i < MAX_ACCOUNTS; i++) {
		sbk_text_set(&s->name[i], "", l->face, "SemiBold", (int)(3.6f * u), l->ink);
		sbk_text_set(&s->handle[i], "", l->face, "SemiBold", (int)(4.4f * u), l->ink);
	}

	if (s->mode == SM_BAR) {
		s->cx = (uint32_t)(pad_x * 2.0f + run - 6.0f * u + 0.5f);
		s->cy = (uint32_t)(pad_y * 2.0f + tallest + 0.5f);
	} else {
		float widest = 0.0f;
		for (int i = 0; i < s->n; i++)
			widest = fmaxf(widest, s->row_w[i]);
		s->cx = (uint32_t)(pad_x * 2.0f + widest + 0.5f);
		s->cy = (uint32_t)(pad_y * 2.0f + run - 3.5f * u + 0.5f);
	}
}

static uint32_t social_width(void *d) { return ((struct social *)d)->cx; }
static uint32_t social_height(void *d) { return ((struct social *)d)->cy; }

static void social_render(void *data, gs_effect_t *unused)
{
	UNUSED_PARAMETER(unused);
	struct social *s = data;
	if (!s->card_fx || !s->glyph_fx || !sbk_stage_begin(&s->stage, s->cx, s->cy))
		return;
	const struct sbk_look *l = &s->look;
	const float u = sbk_u(l);
	const float radius = s->mode == SM_BAR ? (float)s->cy * 0.5f : 4.0f * u;
	sbk_surface_draw(s->card_fx, s->surf, l, 0, 0, (float)s->cx, (float)s->cy, radius);

	if (s->mode == SM_ROTATE) {
		const int i = s->at;
		const float mark = 9.0f * u;
		float x = s->row_x[i], y = s->row_y[i];
		float h = (float)s->cy - y * 2.0f;
		sbk_fill(s->card_fx, x, y + (h - mark) * 0.5f, mark, mark, mark * 0.28f,
			 sbk_alpha(acct_colour(s, i), 0.22f));
		sbk_glyph_draw(s->glyph_fx, s->acct[i].glyph, x + mark * 0.22f, y + (h - mark) * 0.5f + mark * 0.22f,
			       mark * 0.56f, acct_colour(s, i), fmaxf(1.5f, 0.55f * u));
		float tx = x + mark + 3.0f * u;
		float th = (float)sbk_text_h(&s->handle[i]);
		if (s->name[i].text.len)
			th += (float)sbk_text_h(&s->name[i]) + 0.5f * u;
		float ty = y + (h - th) * 0.5f;
		if (s->name[i].text.len) {
			sbk_text_draw(&s->name[i], tx, ty);
			ty += (float)sbk_text_h(&s->name[i]) + 0.5f * u;
		}
		sbk_text_draw(&s->handle[i], tx, ty);
	} else {
		const float mark = 6.0f * u;
		for (int i = 0; i < s->n; i++) {
			float rh = fmaxf((float)sbk_text_h(&s->handle[i]), mark);
			float my = s->row_y[i] + (rh - mark) * 0.5f;
			sbk_glyph_draw(s->glyph_fx, s->acct[i].glyph, s->row_x[i], my, mark, acct_colour(s, i),
				       fmaxf(1.5f, 0.45f * u));
			sbk_text_draw(&s->handle[i], s->row_x[i] + mark + 2.0f * u,
				      s->row_y[i] + (rh - (float)sbk_text_h(&s->handle[i])) * 0.5f);
		}
	}

	sbk_stage_end(&s->stage);
	sbk_stage_present_anim(&s->stage, sbk_anim_eval(&s->anim));
}

static void social_show(void *d)
{
	struct social *s = d;
	s->since = 0.0f;
	sbk_anim_on_show(&s->anim);
}

static void social_enum(void *d, obs_source_enum_proc_t cb, void *p)
{
	struct social *s = d;
	for (int i = 0; i < MAX_ACCOUNTS; i++) {
		sbk_text_enum(&s->name[i], s->self, cb, p);
		sbk_text_enum(&s->handle[i], s->self, cb, p);
	}
}

static bool on_mode_changed(obs_properties_t *props, obs_property_t *prop, obs_data_t *st)
{
	UNUSED_PARAMETER(prop);
	const char *m = obs_data_get_string(st, "mode");
	bool rotating = astrcmpi(m, "bar") != 0 && astrcmpi(m, "stack") != 0;
	obs_property_set_visible(obs_properties_get(props, "rotate"), rotating);
	obs_property_set_visible(obs_properties_get(props, "show_name"), rotating);
	return true;
}

static obs_properties_t *social_properties(void *data)
{
	UNUSED_PARAMETER(data);
	obs_properties_t *p = obs_properties_create();
	obs_property_t *ta = obs_properties_add_text(p, "accounts", "Accounts", OBS_TEXT_MULTILINE);
	obs_property_set_long_description(
		ta, "One per line, as platform: handle — youtube: @imswarnil. Known platforms bring their "
		    "colour with them; anything else is used as the name.");

	obs_property_t *m = obs_properties_add_list(p, "mode", "Shape", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(m, "One at a time — a lower third that changes", "rotate");
	obs_property_list_add_string(m, "A bar of all of them", "bar");
	obs_property_list_add_string(m, "A stack of all of them", "stack");
	obs_property_set_modified_callback(m, on_mode_changed);

	obs_properties_add_float_slider(p, "rotate", "Change every (seconds)", 0.0, 60.0, 0.5);
	obs_properties_add_bool(p, "show_name", "Show the platform's name above the handle");
	obs_properties_add_bool(p, "brand", "Use each platform's own colour");
	sbk_surface_list(p, "variant", "Variant");

	obs_properties_add_text(
		p, "hint",
		"The marks are generic — a play triangle, a camera, an at-sign — never a company's logo. "
		"A platform is told apart by its colour and its name, which keeps this kit free to give away.",
		OBS_TEXT_INFO);

	sbk_look_props(p, true);
	sbk_anim_props(p);
	return p;
}

static void social_defaults(obs_data_t *st)
{
	obs_data_set_default_string(st, "accounts", "youtube: @imswarnil\n"
						    "x: @imswarnil\n"
						    "instagram: @imswarnil\n"
						    "github: imswarnil\n"
						    "web: imswarnil.com");
	obs_data_set_default_string(st, "mode", "rotate");
	obs_data_set_default_double(st, "rotate", 8.0);
	obs_data_set_default_bool(st, "show_name", true);
	obs_data_set_default_bool(st, "brand", true);
	obs_data_set_default_string(st, "variant", "card");
	sbk_look_defaults(st);
	sbk_anim_defaults(st, "up");
}

struct obs_source_info sbk_social_info = {
	.id = "sbk_social",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
	.get_name = social_name,
	.create = social_create,
	.destroy = social_destroy,
	.update = social_update,
	.get_defaults = social_defaults,
	.get_properties = social_properties,
	.get_width = social_width,
	.get_height = social_height,
	.video_tick = social_tick,
	.video_render = social_render,
	.show = social_show,
	.enum_active_sources = social_enum,
	.icon_type = OBS_ICON_TYPE_TEXT,
};
