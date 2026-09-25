/*  SBK QR — a code on screen for the thing you are asking people to do:
    subscribe, become a member, donate, open the site.

    The code is generated here rather than fetched from a web service, so it
    works with no connection, nothing is logged by a third party, and the link
    is not silently rewritten by whoever owns that service.

    Two details that decide whether a phone can actually read it off a stream:
    the quiet zone is never dropped — a code butted against the picture is a
    code that does not scan — and a logo cut-out forces the error-correction
    level up to H, because punching a hole in a level-L code destroys it.  */

#include "sbk-common.h"
#include "sbk-text.h"
#include "sbk-stage.h"
#include "sbk-anim.h"
#include "sbk-qr.h"

struct qr_src {
	obs_source_t *self;
	struct sbk_look look;
	struct sbk_anim anim;
	struct sbk_stage stage;
	gs_effect_t *card_fx, *qr_fx;
	gs_texture_t *tex;
	struct sbk_text caption, sub;

	char *text, *s_caption, *s_sub;
	enum sbk_qr_ecc ecc;
	enum sbk_surface surf;
	bool round, invert, accent_code;
	float hole, quiet;
	uint32_t code_px, cx, cy;
	int modules;
	bool ok;

	float code_x, code_y, cap_y, sub_y;
	struct sbk_qr qr;
	bool dirty;
};

static const char *qr_name(void *u)
{
	UNUSED_PARAMETER(u);
	return obs_module_text("SBK.QR");
}

static void qr_regen(struct qr_src *q)
{
	q->ok = sbk_qr_encode(&q->qr, q->text ? q->text : "", q->ecc);
	if (!q->ok) {
		SBK_LOG(LOG_WARNING, "QR: that text is too long — shorten the link or lower the error correction");
		q->modules = 0;
		return;
	}
	q->modules = q->qr.size;
	q->dirty = true;
}

static void qr_update(void *data, obs_data_t *s)
{
	struct qr_src *q = data;
	sbk_look_read(&q->look, s);
	sbk_anim_read(&q->anim, s, 4.0f * sbk_u(&q->look));

	const char *lvl = obs_data_get_string(s, "ecc");
	enum sbk_qr_ecc ecc = astrcmpi(lvl, "L") == 0   ? SBK_QR_L
			      : astrcmpi(lvl, "Q") == 0 ? SBK_QR_Q
			      : astrcmpi(lvl, "H") == 0 ? SBK_QR_H
							: SBK_QR_M;
	q->hole = (float)obs_data_get_double(s, "hole");
	/* a hole in a low-correction code is an unreadable code */
	if (q->hole > 0.01f && ecc < SBK_QR_Q)
		ecc = SBK_QR_H;

	const char *text = obs_data_get_string(s, "text");
	bool changed = !q->text || strcmp(q->text, text) != 0 || ecc != q->ecc;
	bfree(q->text);
	q->text = bstrdup(text);
	q->ecc = ecc;

	bfree(q->s_caption);
	bfree(q->s_sub);
	q->s_caption = bstrdup(obs_data_get_string(s, "caption"));
	q->s_sub = bstrdup(obs_data_get_string(s, "sub"));

	q->surf = sbk_surface_from(obs_data_get_string(s, "variant"));
	q->round = obs_data_get_bool(s, "round");
	q->invert = obs_data_get_bool(s, "invert");
	q->accent_code = obs_data_get_bool(s, "accent_code");
	q->quiet = (float)obs_data_get_int(s, "quiet");
	q->code_px = (uint32_t)obs_data_get_int(s, "code_size");

	if (changed || !q->modules)
		qr_regen(q);
}

static void *qr_create(obs_data_t *s, obs_source_t *source)
{
	struct qr_src *q = bzalloc(sizeof(*q));
	q->self = source;
	q->card_fx = sbk_load_effect("effects/card.effect");
	q->qr_fx = sbk_load_effect("effects/qr.effect");
	sbk_stage_init(&q->stage);
	qr_update(q, s);
	sbk_anim_play(&q->anim);
	return q;
}

static void qr_destroy(void *data)
{
	struct qr_src *q = data;
	sbk_text_free(&q->caption);
	sbk_text_free(&q->sub);
	sbk_stage_free(&q->stage);
	sbk_free_effect(&q->card_fx);
	sbk_free_effect(&q->qr_fx);
	obs_enter_graphics();
	if (q->tex)
		gs_texture_destroy(q->tex);
	obs_leave_graphics();
	bfree(q->text);
	bfree(q->s_caption);
	bfree(q->s_sub);
	bfree(q);
}

static void qr_tick(void *data, float seconds)
{
	struct qr_src *q = data;
	sbk_anim_tick(&q->anim, seconds);
	const struct sbk_look *l = &q->look;
	const float u = sbk_u(l);
	const bool bare = q->surf == SBK_SURF_NONE;

	sbk_text_set_full(&q->caption, q->s_caption, l->face, "SemiBold", (int)(6.0f * u),
			  sbk_surface_ink(q->surf, l, false), 0, bare);
	sbk_text_set_full(&q->sub, q->s_sub, l->face, "Regular", (int)(4.5f * u),
			  sbk_surface_ink(q->surf, l, true), 0, bare);

	float pad = bare ? 0.0f : 6.0f * u;
	float code = (float)q->code_px;
	float cap_h = q->caption.text.len ? (float)sbk_text_h(&q->caption) + 3.0f * u : 0.0f;
	float sub_h = q->sub.text.len ? (float)sbk_text_h(&q->sub) + 1.0f * u : 0.0f;

	float w = fmaxf(code, fmaxf((float)sbk_text_w(&q->caption), (float)sbk_text_w(&q->sub)));
	q->cx = (uint32_t)(w + pad * 2.0f + 0.5f);
	q->code_x = ((float)q->cx - code) * 0.5f;
	q->code_y = pad;
	q->cap_y = pad + code + 3.0f * u;
	q->sub_y = q->cap_y + (q->caption.text.len ? (float)sbk_text_h(&q->caption) + 1.0f * u : 0.0f);
	q->cy = (uint32_t)(pad * 2.0f + code + cap_h + sub_h + 0.5f);
}

static uint32_t qr_width(void *d) { return ((struct qr_src *)d)->cx; }
static uint32_t qr_height(void *d) { return ((struct qr_src *)d)->cy; }

static void qr_render(void *data, gs_effect_t *unused)
{
	UNUSED_PARAMETER(unused);
	struct qr_src *q = data;
	if (!q->card_fx || !q->qr_fx || !q->ok || !sbk_stage_begin(&q->stage, q->cx, q->cy))
		return;
	const struct sbk_look *l = &q->look;
	const float u = sbk_u(l);

	/* the matrix as a one-channel texture, rebuilt only when the text changes */
	if (q->dirty || !q->tex || (int)gs_texture_get_width(q->tex) != q->modules) {
		if (q->tex)
			gs_texture_destroy(q->tex);
		/* RGBA rather than R8 on purpose. A one-byte-per-module texture has
		   rows of whatever the module count is — 29, 33, 37 — and the upload
		   path wants each row on a four-byte boundary, so an odd width shifts
		   every row a little further than the last. The result still looks
		   like a QR code and cannot be scanned, which is the worst kind of
		   bug. Four bytes per module is always aligned. */
		uint8_t *px = bmalloc((size_t)(q->modules * q->modules) * 4);
		for (int y = 0; y < q->modules; y++)
			for (int x = 0; x < q->modules; x++) {
				uint8_t v = sbk_qr_at(&q->qr, x, y) ? 255 : 0;
				size_t i = ((size_t)y * q->modules + x) * 4;
				px[i] = px[i + 1] = px[i + 2] = v;
				px[i + 3] = 255;
			}
		q->tex = gs_texture_create((uint32_t)q->modules, (uint32_t)q->modules, GS_RGBA, 1,
					   (const uint8_t **)&px, 0);
		bfree(px);
		q->dirty = false;
	}
	if (!q->tex) {
		sbk_stage_end(&q->stage);
		return;
	}

	sbk_surface_draw(q->card_fx, q->surf, l, 0, 0, (float)q->cx, (float)q->cy, 4.0f * u);

	/* a code has to be light-on-dark or dark-on-light with real contrast, so
	   the plate under it is always drawn, whatever the card behind is doing */
	struct vec4 ink = q->invert ? sbk_vec(SBK_INK) : sbk_vec(0xFF0A0A0Au);
	struct vec4 plate = q->invert ? sbk_vec(0xFF0A0A0Au) : sbk_vec(SBK_INK);
	if (q->accent_code)
		ink = l->accent;

	float code = (float)q->code_px;
	sbk_fill(q->card_fx, q->code_x, q->code_y, code, code, 3.0f * u, plate);

	gs_eparam_t *p = gs_effect_get_param_by_name(q->qr_fx, "modules");
	if (p)
		gs_effect_set_texture(p, q->tex);
	sbk_set_float(q->qr_fx, "count", (float)q->modules);
	sbk_set_float(q->qr_fx, "quiet", q->quiet);
	sbk_set_vec2(q->qr_fx, "size", code, code);
	sbk_set_vec4(q->qr_fx, "dark", &ink);
	struct vec4 clear = SBK_NONE;
	sbk_set_vec4(q->qr_fx, "light", &clear);
	sbk_set_float(q->qr_fx, "radius", q->round ? 0.42f : 0.0f);
	sbk_set_float(q->qr_fx, "hole", q->hole);

	gs_matrix_push();
	gs_matrix_translate3f(q->code_x, q->code_y, 0.0f);
	while (gs_effect_loop(q->qr_fx, "Draw"))
		gs_draw_sprite(NULL, 0, (uint32_t)code, (uint32_t)code);
	gs_matrix_pop();

	if (q->caption.text.len)
		sbk_text_draw(&q->caption, ((float)q->cx - (float)sbk_text_w(&q->caption)) * 0.5f, q->cap_y);
	if (q->sub.text.len)
		sbk_text_draw(&q->sub, ((float)q->cx - (float)sbk_text_w(&q->sub)) * 0.5f, q->sub_y);

	sbk_stage_end(&q->stage);
	sbk_stage_present_anim(&q->stage, sbk_anim_eval(&q->anim));
}

static void qr_show(void *d) { sbk_anim_on_show(&((struct qr_src *)d)->anim); }
static void qr_enum(void *d, obs_source_enum_proc_t cb, void *p)
{
	struct qr_src *q = d;
	sbk_text_enum(&q->caption, q->self, cb, p);
	sbk_text_enum(&q->sub, q->self, cb, p);
}

static obs_properties_t *qr_properties(void *data)
{
	UNUSED_PARAMETER(data);
	obs_properties_t *p = obs_properties_create();
	obs_property_t *t = obs_properties_add_text(p, "text", "Link or text", OBS_TEXT_DEFAULT);
	obs_property_set_long_description(t,
		"Anything a phone camera should open: a membership page, a donation link, a channel URL. "
		"Up to about a thousand characters — a shorter link scans from further away.");
	obs_properties_add_text(p, "caption", "Caption", OBS_TEXT_DEFAULT);
	obs_properties_add_text(p, "sub", "Second line", OBS_TEXT_DEFAULT);
	obs_properties_add_int(p, "code_size", "Code size (px)", 80, 1200, 4);
	obs_property_t *e = obs_properties_add_list(p, "ecc", "Error correction", OBS_COMBO_TYPE_LIST,
						    OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(e, "L — smallest code", "L");
	obs_property_list_add_string(e, "M — the usual choice", "M");
	obs_property_list_add_string(e, "Q — survives more damage", "Q");
	obs_property_list_add_string(e, "H — most robust, needed for a logo hole", "H");
	obs_properties_add_int_slider(p, "quiet", "Quiet zone (modules)", 2, 8, 1);
	obs_properties_add_bool(p, "round", "Round the modules");
	obs_properties_add_bool(p, "invert", "Light code on a dark plate");
	obs_properties_add_bool(p, "accent_code", "Draw the code in the accent");
	obs_property_t *h = obs_properties_add_float_slider(p, "hole", "Logo hole", 0.0, 0.26, 0.01);
	obs_property_set_long_description(h,
		"Clears a square in the middle to drop a logo image on top. Anything above zero pins the "
		"error correction to H, which is what makes the code survive the hole.");
	sbk_surface_list(p, "variant", "Variant");
	sbk_look_props(p, false);
	sbk_anim_props(p);
	obs_properties_add_text(p, "hint",
				"Give people time: a code needs a few seconds on screen and enough size to fill "
				"a phone's viewfinder from the sofa. 240 px on a 1080p canvas is about the floor.",
				OBS_TEXT_INFO);
	return p;
}

static void qr_defaults(obs_data_t *s)
{
	obs_data_set_default_string(s, "text", "https://imswarnil.com");
	obs_data_set_default_string(s, "caption", "Scan to visit");
	obs_data_set_default_string(s, "sub", "imswarnil.com");
	obs_data_set_default_int(s, "code_size", 300);
	obs_data_set_default_string(s, "ecc", "M");
	obs_data_set_default_int(s, "quiet", 4);
	obs_data_set_default_bool(s, "round", true);
	obs_data_set_default_bool(s, "invert", false);
	obs_data_set_default_bool(s, "accent_code", false);
	obs_data_set_default_double(s, "hole", 0.0);
	obs_data_set_default_string(s, "variant", "card");
	sbk_look_defaults(s);
	sbk_anim_defaults(s, "fade");
}

struct obs_source_info sbk_qr_info = {
	.id = "sbk_qr",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
	.get_name = qr_name,
	.create = qr_create,
	.destroy = qr_destroy,
	.update = qr_update,
	.get_defaults = qr_defaults,
	.get_properties = qr_properties,
	.get_width = qr_width,
	.get_height = qr_height,
	.video_tick = qr_tick,
	.video_render = qr_render,
	.show = qr_show,
	.enum_active_sources = qr_enum,
	.icon_type = OBS_ICON_TYPE_IMAGE,
};
