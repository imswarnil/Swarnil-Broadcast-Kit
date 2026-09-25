/*  SBK Stats — how the stream is actually going: how long it has been up, the
    bitrate leaving the machine, dropped frames, and whether the renderer is
    keeping pace.

    This is the source that could never be a web overlay. Uptime, dropped
    frames and congestion live inside OBS; a page in a Browser Source has no
    way to reach any of them. Put it in a scene only you see — a projector on a
    second monitor — or on the stream if you want the numbers public.

    The bitrate is measured the only way it can be: bytes sent since the last
    sample, over the time since the last sample. OBS does not publish a rate.  */

#include <obs-frontend-api.h>

#include "sbk-common.h"
#include "sbk-text.h"
#include "sbk-stage.h"
#include "sbk-anim.h"
#include "sbk-state.h"

#define ROWS 4

struct stats {
	obs_source_t *self;
	struct sbk_look look;
	struct sbk_anim anim;
	struct sbk_stage stage;
	gs_effect_t *card_fx;
	struct sbk_text key[ROWS], val[ROWS];
	int n_rows;

	bool show_uptime, show_bitrate, show_dropped, show_fps, compact;
	enum sbk_surface surf;
	uint32_t width, cx, cy;
	float row_h, pad_x, pad_y;

	/* the bitrate is a delta, so the last sample has to be kept */
	uint64_t last_bytes;
	uint64_t last_ns;
	double kbps;
	/* lagged frames are a total since OBS started; only a rising total means
	   anything, so the panel reports the change rather than the count */
	uint32_t last_lagged;
	uint32_t lag_recent;
	float lag_window;
	float health; /* 0 fine, 1 trouble */
	float t;
};

static const char *stats_name(void *u)
{
	UNUSED_PARAMETER(u);
	return obs_module_text("SBK.Stats");
}

static void stats_update(void *data, obs_data_t *s)
{
	struct stats *g = data;
	sbk_look_read(&g->look, s);
	sbk_anim_read(&g->anim, s, 4.0f * sbk_u(&g->look));
	g->show_uptime = obs_data_get_bool(s, "show_uptime");
	g->show_bitrate = obs_data_get_bool(s, "show_bitrate");
	g->show_dropped = obs_data_get_bool(s, "show_dropped");
	g->show_fps = obs_data_get_bool(s, "show_fps");
	g->compact = obs_data_get_bool(s, "compact");
	g->surf = sbk_surface_from(obs_data_get_string(s, "variant"));
	g->width = (uint32_t)obs_data_get_int(s, "width");
}

static void *stats_create(obs_data_t *s, obs_source_t *source)
{
	struct stats *g = bzalloc(sizeof(*g));
	g->self = source;
	g->card_fx = sbk_load_effect("effects/card.effect");
	sbk_stage_init(&g->stage);
	stats_update(g, s);
	sbk_anim_play(&g->anim);
	return g;
}

static void stats_destroy(void *data)
{
	struct stats *g = data;
	for (int i = 0; i < ROWS; i++) {
		sbk_text_free(&g->key[i]);
		sbk_text_free(&g->val[i]);
	}
	sbk_stage_free(&g->stage);
	sbk_free_effect(&g->card_fx);
	bfree(g);
}

static void clock_str(char *buf, size_t n, double secs)
{
	int s = (int)secs;
	snprintf(buf, n, "%d:%02d:%02d", s / 3600, (s / 60) % 60, s % 60);
}

static void stats_tick(void *data, float seconds)
{
	struct stats *g = data;
	g->t += seconds;
	sbk_anim_tick(&g->anim, seconds);
	const struct sbk_look *l = &g->look;
	const float u = sbk_u(l);
	const bool bare = g->surf == SBK_SURF_NONE;

	obs_output_t *out = obs_frontend_get_streaming_output();
	bool live = out && obs_output_active(out);
	int dropped = 0, total = 0;
	float congestion = 0.0f;

	if (out) {
		dropped = obs_output_get_frames_dropped(out);
		total = obs_output_get_total_frames(out);
		congestion = obs_output_get_congestion(out);
		uint64_t bytes = obs_output_get_total_bytes(out);
		uint64_t now = os_gettime_ns();
		if (g->last_ns && now > g->last_ns && bytes >= g->last_bytes) {
			double dt = (double)(now - g->last_ns) / 1000000000.0;
			if (dt >= 0.5) { /* sample twice a second: any faster is noise */
				g->kbps = (double)(bytes - g->last_bytes) * 8.0 / 1000.0 / dt;
				g->last_bytes = bytes;
				g->last_ns = now;
			}
		} else {
			g->last_bytes = bytes;
			g->last_ns = now;
		}
		obs_output_release(out);
	} else {
		g->kbps = 0.0;
		g->last_ns = 0;
	}

	double drop_pct = total > 0 ? (double)dropped * 100.0 / (double)total : 0.0;
	double fps = obs_get_active_fps();
	uint32_t lagged = obs_get_lagged_frames();
	if (!g->last_lagged)
		g->last_lagged = lagged;
	g->lag_window += seconds;
	if (lagged > g->last_lagged) {
		g->lag_recent += lagged - g->last_lagged;
		g->last_lagged = lagged;
	}
	if (g->lag_window >= 10.0f) { /* forget a stumble after ten quiet seconds */
		g->lag_window = 0.0f;
		g->lag_recent = 0;
	}

	/* one number for "is this going well": congestion is the network, lagged
	   frames are the renderer */
	g->health = sbk_clampf(fmaxf(congestion, (float)(drop_pct / 5.0)), 0.0f, 1.0f);
	if (g->lag_recent)
		g->health = fmaxf(g->health, 0.3f);

	char buf[64];
	int row = 0;
	struct vec4 key_ink = sbk_surface_ink(g->surf, l, true);
	struct vec4 val_ink = sbk_surface_ink(g->surf, l, false);
	int key_size = (int)(4.0f * u), val_size = (int)(5.0f * u);

#define ROW(name, text, warn)                                                                              \
	do {                                                                                               \
		if (row < ROWS) {                                                                          \
			sbk_text_set_full(&g->key[row], name, l->face, "Medium", key_size, key_ink, 0, bare); \
			sbk_text_set_full(&g->val[row], text, l->mono, "Bold", val_size,                   \
					  (warn) ? sbk_vec(SBK_REC) : val_ink, 0, bare);                   \
			row++;                                                                             \
		}                                                                                          \
	} while (0)

	if (g->show_uptime) {
		double up = sbk_state_uptime(false);
		if (up <= 0.0)
			up = sbk_state_uptime(true);
		clock_str(buf, sizeof(buf), up);
		ROW(sbk_status.streaming ? "Live for" : (sbk_status.recording ? "Recording" : "Uptime"), buf, false);
	}
	if (g->show_bitrate) {
		if (live)
			snprintf(buf, sizeof(buf), "%.0f kb/s", g->kbps);
		else
			snprintf(buf, sizeof(buf), "—");
		ROW("Bitrate", buf, live && congestion > 0.3f);
	}
	if (g->show_dropped) {
		snprintf(buf, sizeof(buf), "%.1f%%", drop_pct);
		ROW("Dropped", buf, drop_pct > 1.0);
	}
	if (g->show_fps) {
		if (g->lag_recent)
			snprintf(buf, sizeof(buf), "%.0f fps · %u lagged", fps, g->lag_recent);
		else
			snprintf(buf, sizeof(buf), "%.0f fps", fps);
		ROW("Render", buf, g->lag_recent > 0);
	}
#undef ROW

	for (int i = row; i < ROWS; i++) {
		sbk_text_set(&g->key[i], "", l->face, "Medium", key_size, key_ink);
		sbk_text_set(&g->val[i], "", l->mono, "Bold", val_size, val_ink);
	}
	g->n_rows = row;

	g->pad_x = bare ? 0.0f : 6.0f * u;
	g->pad_y = bare ? 0.0f : 5.0f * u;
	g->row_h = (float)sbk_text_h(&g->val[0]) + (g->compact ? 1.0f * u : 2.5f * u);
	g->cx = g->width;
	g->cy = (uint32_t)(g->pad_y * 2.0f + g->row_h * (float)(row ? row : 1) + 0.5f);
}

static uint32_t stats_width(void *d) { return ((struct stats *)d)->cx; }
static uint32_t stats_height(void *d) { return ((struct stats *)d)->cy; }

static void stats_render(void *data, gs_effect_t *unused)
{
	UNUSED_PARAMETER(unused);
	struct stats *g = data;
	if (!g->card_fx || !sbk_stage_begin(&g->stage, g->cx, g->cy))
		return;
	const struct sbk_look *l = &g->look;
	const float u = sbk_u(l);

	sbk_surface_draw(g->card_fx, g->surf, l, 0, 0, (float)g->cx, (float)g->cy, 3.0f * u);

	/* the health lamp: green while everything is fine, red when it is not */
	float d = 2.0f * u;
	struct vec4 lamp = g->health > 0.5f ? sbk_vec(SBK_REC)
			   : g->health > 0.15f ? sbk_vec(0xFF33C4FFu)
					       : sbk_vec(SBK_OK);
	if (!sbk_status.streaming && !sbk_status.recording)
		lamp = sbk_vec(SBK_OFF);
	float pulse = 0.5f + 0.5f * sinf(g->t * 6.2831853f / SBK_PULSE_SECS);
	sbk_dot(g->card_fx, (float)g->cx - g->pad_x - d, g->pad_y + d * 0.25f, d, lamp,
		g->health > 0.15f ? 2.0f * u : 0.0f, 0.3f + 0.4f * pulse);

	for (int i = 0; i < g->n_rows; i++) {
		float y = g->pad_y + g->row_h * (float)i;
		float kh = (float)sbk_text_h(&g->key[i]);
		float vh = (float)sbk_text_h(&g->val[i]);
		sbk_text_draw(&g->key[i], g->pad_x, y + (vh - kh) * 0.5f);
		sbk_text_draw(&g->val[i], (float)g->cx - g->pad_x - (float)sbk_text_w(&g->val[i]) -
						  (i == 0 ? d + 2.0f * u : 0.0f),
			      y);
	}

	sbk_stage_end(&g->stage);
	struct sbk_anim_out a = sbk_anim_eval(&g->anim);
	sbk_stage_present(&g->stage, a.alpha, a.dx, a.dy);
}

static void stats_show(void *d) { sbk_anim_on_show(&((struct stats *)d)->anim); }
static void stats_enum(void *d, obs_source_enum_proc_t cb, void *p)
{
	struct stats *g = d;
	for (int i = 0; i < ROWS; i++) {
		sbk_text_enum(&g->key[i], g->self, cb, p);
		sbk_text_enum(&g->val[i], g->self, cb, p);
	}
}

static obs_properties_t *stats_properties(void *data)
{
	UNUSED_PARAMETER(data);
	obs_properties_t *p = obs_properties_create();
	obs_properties_add_bool(p, "show_uptime", "How long it has been live");
	obs_properties_add_bool(p, "show_bitrate", "Bitrate");
	obs_properties_add_bool(p, "show_dropped", "Dropped frames");
	obs_properties_add_bool(p, "show_fps", "Render rate and lagged frames");
	obs_properties_add_bool(p, "compact", "Tighter rows");
	sbk_surface_list(p, "variant", "Variant");
	obs_properties_add_int(p, "width", "Width", 200, 1920, 2);
	sbk_look_props(p, true);
	sbk_anim_props(p);
	obs_properties_add_text(p, "hint",
				"Dropped frames and congestion come from the running output, so they only move "
				"while you are actually streaming. For a private read-out, put this in its own "
				"scene and open it as a windowed projector.",
				OBS_TEXT_INFO);
	return p;
}

static void stats_defaults(obs_data_t *s)
{
	obs_data_set_default_bool(s, "show_uptime", true);
	obs_data_set_default_bool(s, "show_bitrate", true);
	obs_data_set_default_bool(s, "show_dropped", true);
	obs_data_set_default_bool(s, "show_fps", false);
	obs_data_set_default_bool(s, "compact", false);
	obs_data_set_default_string(s, "variant", "card");
	obs_data_set_default_int(s, "width", 420);
	sbk_look_defaults(s);
	sbk_anim_defaults(s, "fade");
}

struct obs_source_info sbk_stats_info = {
	.id = "sbk_stats",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
	.get_name = stats_name,
	.create = stats_create,
	.destroy = stats_destroy,
	.update = stats_update,
	.get_defaults = stats_defaults,
	.get_properties = stats_properties,
	.get_width = stats_width,
	.get_height = stats_height,
	.video_tick = stats_tick,
	.video_render = stats_render,
	.show = stats_show,
	.enum_active_sources = stats_enum,
	.icon_type = OBS_ICON_TYPE_TEXT,
};
