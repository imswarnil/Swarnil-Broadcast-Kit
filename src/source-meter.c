/*  SBK Meter — a broadcast level meter for any audio OBS carries, the program
    mix included.

    It is a meter rather than a decoration: the scale is in dB, the zones are
    where a broadcaster expects them (comfortable below −18, loud by −6, hot at
    the top), and the peak sits where the loudest recent moment was rather than
    falling with the bar. A streamer can see at a glance that they are clipping,
    which no pretty bouncing bar has ever told anyone.  */

#include "sbk-common.h"
#include "sbk-text.h"
#include "sbk-stage.h"
#include "sbk-anim.h"
#include "sbk-audio.h"

struct meter {
	obs_source_t *self;
	struct sbk_look look;
	struct sbk_anim anim;
	struct sbk_stage stage;
	gs_effect_t *card_fx;
	struct sbk_audio audio;
	struct sbk_text label, readout;

	char *s_label;
	bool vertical, segments, show_peak, show_db, show_zones;
	int segment_count;
	float warn_db, hot_db;
	enum sbk_surface surf;
	uint32_t width, height, cx, cy;
	float track_x, track_y, track_w, track_h;
};

static const char *meter_name(void *u)
{
	UNUSED_PARAMETER(u);
	return obs_module_text("SBK.Meter");
}

static void meter_update(void *data, obs_data_t *s)
{
	struct meter *m = data;
	sbk_look_read(&m->look, s);
	sbk_anim_read(&m->anim, s, 4.0f * sbk_u(&m->look));
	sbk_audio_set_source(&m->audio, obs_data_get_string(s, "source"));
	bfree(m->s_label);
	m->s_label = bstrdup(obs_data_get_string(s, "label"));
	m->vertical = astrcmpi(obs_data_get_string(s, "orientation"), "vertical") == 0;
	const char *st = obs_data_get_string(s, "style");
	m->segments = astrcmpi(st, "segments") == 0;
	m->segment_count = (int)obs_data_get_int(s, "segment_count");
	m->show_peak = obs_data_get_bool(s, "show_peak");
	m->show_db = obs_data_get_bool(s, "show_db");
	m->show_zones = obs_data_get_bool(s, "show_zones");
	m->warn_db = (float)obs_data_get_double(s, "warn_db");
	m->hot_db = (float)obs_data_get_double(s, "hot_db");
	m->surf = sbk_surface_from(obs_data_get_string(s, "variant"));
	m->width = (uint32_t)obs_data_get_int(s, "width");
	m->height = (uint32_t)obs_data_get_int(s, "height");
	m->audio.gain = (float)obs_data_get_double(s, "gain");
	m->audio.floor_db = (float)obs_data_get_double(s, "floor_db");
	m->audio.smoothing = (float)obs_data_get_double(s, "smoothing");
	m->audio.decay = (float)obs_data_get_double(s, "decay");
}

static void *meter_create(obs_data_t *s, obs_source_t *source)
{
	struct meter *m = bzalloc(sizeof(*m));
	m->self = source;
	m->card_fx = sbk_load_effect("effects/card.effect");
	sbk_stage_init(&m->stage);
	sbk_audio_init(&m->audio);
	meter_update(m, s);
	sbk_anim_play(&m->anim);
	return m;
}

static void meter_destroy(void *data)
{
	struct meter *m = data;
	sbk_audio_free(&m->audio);
	sbk_text_free(&m->label);
	sbk_text_free(&m->readout);
	sbk_stage_free(&m->stage);
	sbk_free_effect(&m->card_fx);
	bfree(m->s_label);
	bfree(m);
}

/* where a dB value sits along the meter, 0..1 */
static float at_db(const struct meter *m, float db)
{
	float floor_db = m->audio.floor_db;
	return sbk_clampf((db - floor_db) / (0.0f - floor_db), 0.0f, 1.0f);
}

static void meter_tick(void *data, float seconds)
{
	struct meter *m = data;
	sbk_anim_tick(&m->anim, seconds);
	sbk_audio_tick(&m->audio, seconds);
	const struct sbk_look *l = &m->look;
	const float u = sbk_u(l);
	const bool bare = m->surf == SBK_SURF_NONE;

	struct dstr lab = {0};
	dstr_copy(&lab, m->s_label ? m->s_label : "");
	sbk_caps(&lab);
	sbk_text_set_full(&m->label, lab.array ? lab.array : "", l->face, "SemiBold", (int)(4.0f * u),
			  sbk_surface_ink(m->surf, l, true), 0, bare);
	dstr_free(&lab);

	char db[32] = {0};
	if (m->show_db) {
		float v = m->audio.demo ? m->audio.floor_db + m->audio.level * (0.0f - m->audio.floor_db)
					: m->audio.level_db;
		if (v <= m->audio.floor_db + 0.5f)
			snprintf(db, sizeof(db), "−∞");
		else
			snprintf(db, sizeof(db), "%.1f", v);
	}
	sbk_text_set_full(&m->readout, db, l->mono, "Bold", (int)(4.5f * u), sbk_surface_ink(m->surf, l, false), 0, bare);

	float pad = bare ? 0.0f : 4.0f * u;
	float head = fmaxf((float)sbk_text_h(&m->label), (float)sbk_text_h(&m->readout));
	bool has_head = m->label.text.len || m->readout.text.len;

	if (m->vertical) {
		m->cx = m->width;
		m->cy = m->height;
		m->track_x = pad;
		m->track_y = pad + (has_head ? head + 2.0f * u : 0.0f);
		m->track_w = (float)m->cx - pad * 2.0f;
		m->track_h = (float)m->cy - m->track_y - pad;
	} else {
		m->cx = m->width;
		m->track_x = pad;
		m->track_y = pad + (has_head ? head + 2.0f * u : 0.0f);
		m->track_w = (float)m->cx - pad * 2.0f;
		m->track_h = 4.0f * u;
		m->cy = (uint32_t)(m->track_y + m->track_h + pad + 0.5f);
	}
}

static uint32_t meter_width(void *d) { return ((struct meter *)d)->cx; }
static uint32_t meter_height(void *d) { return ((struct meter *)d)->cy; }

/* the colour at a point on the scale: calm, then warning, then hot */
static struct vec4 zone_at(const struct meter *m, const struct sbk_look *l, float pos)
{
	if (!m->show_zones)
		return l->accent;
	if (pos >= at_db(m, m->hot_db))
		return sbk_vec(SBK_REC);
	if (pos >= at_db(m, m->warn_db))
		return sbk_vec(0xFF33C4FFu); /* amber */
	return sbk_vec(SBK_OK);
}

static void meter_render(void *data, gs_effect_t *unused)
{
	UNUSED_PARAMETER(unused);
	struct meter *m = data;
	if (!m->card_fx || !sbk_stage_begin(&m->stage, m->cx, m->cy))
		return;
	const struct sbk_look *l = &m->look;
	const float u = sbk_u(l);
	float pad = m->surf == SBK_SURF_NONE ? 0.0f : 4.0f * u;

	sbk_surface_draw(m->card_fx, m->surf, l, 0, 0, (float)m->cx, (float)m->cy, 3.0f * u);

	if (m->label.text.len)
		sbk_text_draw(&m->label, pad, pad);
	if (m->readout.text.len)
		sbk_text_draw(&m->readout, (float)m->cx - pad - (float)sbk_text_w(&m->readout), pad);

	struct vec4 track = m->surf == SBK_SURF_ACCENT ? sbk_alpha(l->on_accent, 0.25f) : l->wash;
	float r = (m->vertical ? m->track_w : m->track_h) * 0.5f;
	sbk_fill(m->card_fx, m->track_x, m->track_y, m->track_w, m->track_h, r, track);

	const float level = m->audio.level;
	const float peak = m->audio.level_peak;

	if (m->segments) {
		int n = m->segment_count < 4 ? 4 : m->segment_count;
		float gap = 1.0f * u;
		float run = m->vertical ? m->track_h : m->track_w;
		float seg = (run - gap * (float)(n - 1)) / (float)n;
		for (int i = 0; i < n; i++) {
			float pos = ((float)i + 0.5f) / (float)n;
			bool on = level >= (float)i / (float)n;
			struct vec4 col = zone_at(m, l, pos);
			col = sbk_alpha(col, on ? 1.0f : 0.14f);
			if (m->vertical) {
				/* lit from the foot up */
				float y = m->track_y + m->track_h - (float)(i + 1) * seg - (float)i * gap;
				sbk_fill(m->card_fx, m->track_x, y, m->track_w, seg, r * 0.6f, col);
			} else {
				float x = m->track_x + (float)i * (seg + gap);
				sbk_fill(m->card_fx, x, m->track_y, seg, m->track_h, r * 0.6f, col);
			}
		}
	} else if (level > 0.001f) {
		struct vec4 col = zone_at(m, l, level);
		if (m->vertical) {
			float h = m->track_h * level;
			sbk_fill(m->card_fx, m->track_x, m->track_y + m->track_h - h, m->track_w, h, r, col);
		} else {
			sbk_fill(m->card_fx, m->track_x, m->track_y, m->track_w * level, m->track_h, r, col);
		}
	}

	if (m->show_peak && peak > 0.01f) {
		/* a hairline where the loudest recent moment reached */
		float t = 1.5f * u;
		struct vec4 col = zone_at(m, l, peak);
		if (m->vertical) {
			float y = m->track_y + m->track_h - m->track_h * peak - t * 0.5f;
			sbk_fill(m->card_fx, m->track_x, y, m->track_w, t, t * 0.5f, col);
		} else {
			float x = m->track_x + m->track_w * peak - t * 0.5f;
			sbk_fill(m->card_fx, x, m->track_y, t, m->track_h, t * 0.5f, col);
		}
	}

	sbk_stage_end(&m->stage);
	struct sbk_anim_out a = sbk_anim_eval(&m->anim);
	sbk_stage_present(&m->stage, a.alpha, a.dx, a.dy);
}

static void meter_show(void *d) { sbk_anim_on_show(&((struct meter *)d)->anim); }
static void meter_enum(void *d, obs_source_enum_proc_t cb, void *p)
{
	struct meter *m = d;
	sbk_text_enum(&m->label, m->self, cb, p);
	sbk_text_enum(&m->readout, m->self, cb, p);
}

static obs_properties_t *meter_properties(void *data)
{
	UNUSED_PARAMETER(data);
	obs_properties_t *p = obs_properties_create();
	obs_property_t *src = obs_properties_add_list(p, "source", "Listen to", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	sbk_audio_fill_source_list(src);
	obs_properties_add_text(p, "label", "Label", OBS_TEXT_DEFAULT);
	obs_property_t *o = obs_properties_add_list(p, "orientation", "Orientation", OBS_COMBO_TYPE_LIST,
						    OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(o, "Horizontal", "horizontal");
	obs_property_list_add_string(o, "Vertical", "vertical");
	obs_property_t *st = obs_properties_add_list(p, "style", "Style", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(st, "Solid", "solid");
	obs_property_list_add_string(st, "Segments", "segments");
	obs_properties_add_int_slider(p, "segment_count", "How many segments", 4, 60, 1);
	obs_properties_add_bool(p, "show_peak", "Hold the peak");
	obs_properties_add_bool(p, "show_db", "Show the level in dB");
	obs_properties_add_bool(p, "show_zones", "Colour the zones (calm, warning, hot)");
	obs_properties_add_float_slider(p, "warn_db", "Warning above (dB)", -40.0, -1.0, 0.5);
	obs_properties_add_float_slider(p, "hot_db", "Hot above (dB)", -20.0, 0.0, 0.5);
	sbk_surface_list(p, "variant", "Variant");
	obs_properties_add_int(p, "width", "Width", 60, 3840, 2);
	obs_properties_add_int(p, "height", "Height (vertical)", 60, 2160, 2);

	obs_properties_t *a = obs_properties_create();
	obs_properties_add_float_slider(a, "gain", "Gain", 0.1, 20.0, 0.1);
	obs_properties_add_float_slider(a, "floor_db", "Floor (dB)", -96.0, -20.0, 1.0);
	obs_properties_add_float_slider(a, "smoothing", "Fall smoothing", 0.0, 0.98, 0.01);
	obs_properties_add_float_slider(a, "decay", "Peak fall", 0.001, 0.1, 0.001);
	obs_properties_add_group(p, "audio", "Audio", OBS_GROUP_NORMAL, a);

	sbk_look_props(p, true);
	sbk_anim_props(p);
	return p;
}

static void meter_defaults(obs_data_t *s)
{
	obs_data_set_default_string(s, "source", "@mic");
	obs_data_set_default_string(s, "label", "Mic");
	obs_data_set_default_string(s, "orientation", "horizontal");
	obs_data_set_default_string(s, "style", "segments");
	obs_data_set_default_int(s, "segment_count", 24);
	obs_data_set_default_bool(s, "show_peak", true);
	obs_data_set_default_bool(s, "show_db", true);
	obs_data_set_default_bool(s, "show_zones", true);
	obs_data_set_default_double(s, "warn_db", -18.0);
	obs_data_set_default_double(s, "hot_db", -6.0);
	obs_data_set_default_string(s, "variant", "card");
	obs_data_set_default_int(s, "width", 420);
	obs_data_set_default_int(s, "height", 320);
	obs_data_set_default_double(s, "gain", 1.0);
	obs_data_set_default_double(s, "floor_db", -60.0);
	obs_data_set_default_double(s, "smoothing", 0.7);
	obs_data_set_default_double(s, "decay", 0.006);
	sbk_look_defaults(s);
	sbk_anim_defaults(s, "fade");
}

struct obs_source_info sbk_meter_info = {
	.id = "sbk_meter",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
	.get_name = meter_name,
	.create = meter_create,
	.destroy = meter_destroy,
	.update = meter_update,
	.get_defaults = meter_defaults,
	.get_properties = meter_properties,
	.get_width = meter_width,
	.get_height = meter_height,
	.video_tick = meter_tick,
	.video_render = meter_render,
	.show = meter_show,
	.enum_active_sources = meter_enum,
	.icon_type = OBS_ICON_TYPE_AUDIO_OUTPUT,
};
