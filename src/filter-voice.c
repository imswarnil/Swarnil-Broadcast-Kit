/*  SBK Voice — the chain a spoken voice wants, in one filter with one set of
    presets, instead of four filters each with its own dialog.

    High-pass, gate, compressor, a presence lift, saturation and a limiter, in
    that order. Nothing here is exotic; the value is that the order is right,
    the defaults are sane, and a preset moves all of it at once. OBS ships every
    one of these separately and most people never chain them because doing so
    means understanding six dialogs before you sound better.

    Everything is per channel. A compressor whose detector is the sum of two
    channels pumps audibly on anything panned.  */

#include <obs-module.h>
#include <media-io/audio-io.h>

#include "sbk-common.h"
#include "sbk-dsp.h"

struct voice {
	obs_source_t *self;
	size_t channels;
	float sr;

	bool hp_on, gate_on, comp_on, presence_on, sat_on, limit_on;
	float hp_freq;
	float gate_open_db, gate_close_db, gate_hold_ms;
	float thresh_db, ratio, knee_db, attack_ms, release_ms, makeup_db;
	float presence_db, presence_hz;
	float drive;
	float ceiling_db;

	struct sbk_biquad hp[SBK_MAX_CH], presence[SBK_MAX_CH];
	struct sbk_comp comp[SBK_MAX_CH];
	struct sbk_limiter lim[SBK_MAX_CH];
	struct sbk_gate gate[SBK_MAX_CH];

	float a_coef, r_coef, g_attack, g_release, lim_release;
};

static const char *voice_name(void *u)
{
	UNUSED_PARAMETER(u);
	return obs_module_text("SBK.Voice");
}

static void recalc(struct voice *v)
{
	for (size_t c = 0; c < SBK_MAX_CH; c++) {
		sbk_biquad_hp(&v->hp[c], v->hp_freq, 0.707f, v->sr);
		sbk_biquad_peak(&v->presence[c], v->presence_hz, 0.9f, v->presence_db, v->sr);
	}
	v->a_coef = sbk_coef(v->attack_ms, v->sr);
	v->r_coef = sbk_coef(v->release_ms, v->sr);
	v->g_attack = sbk_coef(2.0f, v->sr);
	v->g_release = sbk_coef(120.0f, v->sr);
	v->lim_release = sbk_coef(60.0f, v->sr);
}

static void voice_update(void *data, obs_data_t *s)
{
	struct voice *v = data;
	v->hp_on = obs_data_get_bool(s, "hp_on");
	v->hp_freq = (float)obs_data_get_double(s, "hp_freq");
	v->gate_on = obs_data_get_bool(s, "gate_on");
	v->gate_open_db = (float)obs_data_get_double(s, "gate_open");
	v->gate_close_db = (float)obs_data_get_double(s, "gate_close");
	v->gate_hold_ms = (float)obs_data_get_double(s, "gate_hold");
	v->comp_on = obs_data_get_bool(s, "comp_on");
	v->thresh_db = (float)obs_data_get_double(s, "threshold");
	v->ratio = (float)obs_data_get_double(s, "ratio");
	v->knee_db = (float)obs_data_get_double(s, "knee");
	v->attack_ms = (float)obs_data_get_double(s, "attack");
	v->release_ms = (float)obs_data_get_double(s, "release");
	v->makeup_db = (float)obs_data_get_double(s, "makeup");
	v->presence_on = obs_data_get_bool(s, "presence_on");
	v->presence_db = (float)obs_data_get_double(s, "presence");
	v->presence_hz = (float)obs_data_get_double(s, "presence_hz");
	v->sat_on = obs_data_get_bool(s, "sat_on");
	v->drive = (float)obs_data_get_double(s, "drive");
	v->limit_on = obs_data_get_bool(s, "limit_on");
	v->ceiling_db = (float)obs_data_get_double(s, "ceiling");
	recalc(v);
}

static void *voice_create(obs_data_t *s, obs_source_t *source)
{
	struct voice *v = bzalloc(sizeof(*v));
	v->self = source;
	audio_t *a = obs_get_audio();
	v->sr = a ? (float)audio_output_get_sample_rate(a) : 48000.0f;
	v->channels = a ? audio_output_get_channels(a) : 2;
	if (v->channels > SBK_MAX_CH)
		v->channels = SBK_MAX_CH;
	voice_update(v, s);
	return v;
}

static void voice_destroy(void *data)
{
	bfree(data);
}

static struct obs_audio_data *voice_filter(void *data, struct obs_audio_data *audio)
{
	struct voice *v = data;
	const float makeup = sbk_db2lin(v->makeup_db);
	const float ceiling = sbk_db2lin(v->ceiling_db);
	const float gate_open = sbk_db2lin(v->gate_open_db);
	const float gate_close = sbk_db2lin(v->gate_close_db);

	for (size_t c = 0; c < v->channels; c++) {
		float *samples = (float *)audio->data[c];
		if (!samples)
			continue;
		for (uint32_t i = 0; i < audio->frames; i++) {
			float x = samples[i];

			if (v->hp_on)
				x = sbk_biquad_run(&v->hp[c], x);
			if (v->gate_on)
				x = sbk_gate_run(&v->gate[c], x, gate_open, gate_close, v->g_attack, v->g_release,
						 v->gate_hold_ms * 0.001f, v->sr);
			if (v->comp_on) {
				x = sbk_comp_run(&v->comp[c], x, v->thresh_db, v->ratio, v->knee_db, v->a_coef,
						 v->r_coef);
				x *= makeup;
			}
			if (v->presence_on)
				x = sbk_biquad_run(&v->presence[c], x);
			if (v->sat_on)
				x = sbk_saturate(x, v->drive);
			if (v->limit_on)
				x = sbk_limit_run(&v->lim[c], x, ceiling, v->lim_release);

			samples[i] = x;
		}
	}
	return audio;
}

/* ---- presets --------------------------------------------------------------- */

struct vpreset {
	const char *id, *label;
	float hp, thresh, ratio, attack, release, makeup, presence, drive;
	bool gate;
	float gate_open, gate_close;
};

static const struct vpreset VPRESETS[] = {
	{"stream", "Stream — even, close, forgiving", 85.0f, -22.0f, 3.5f, 6.0f, 90.0f, 7.0f, 2.5f, 0.06f, false, -45.0f, -55.0f},
	{"podcast", "Podcast — gentler, keeps the dynamics", 70.0f, -18.0f, 2.5f, 12.0f, 140.0f, 4.0f, 1.5f, 0.0f, false, -45.0f, -55.0f},
	{"noisy", "Noisy room — gated and firm", 110.0f, -24.0f, 4.5f, 5.0f, 80.0f, 9.0f, 3.0f, 0.10f, true, -38.0f, -46.0f},
	{"quiet", "Quiet mic — a lot of lift", 80.0f, -30.0f, 4.0f, 8.0f, 110.0f, 14.0f, 3.0f, 0.08f, false, -50.0f, -58.0f},
};
#define N_VPRESETS (sizeof(VPRESETS) / sizeof(VPRESETS[0]))

static bool on_vpreset(obs_properties_t *props, obs_property_t *p, obs_data_t *s)
{
	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(p);
	const char *want = obs_data_get_string(s, "preset");
	if (!want || astrcmpi(want, "custom") == 0)
		return false;
	for (size_t i = 0; i < N_VPRESETS; i++) {
		if (astrcmpi(VPRESETS[i].id, want) != 0)
			continue;
		const struct vpreset *v = &VPRESETS[i];
		obs_data_set_bool(s, "hp_on", true);
		obs_data_set_double(s, "hp_freq", v->hp);
		obs_data_set_bool(s, "gate_on", v->gate);
		obs_data_set_double(s, "gate_open", v->gate_open);
		obs_data_set_double(s, "gate_close", v->gate_close);
		obs_data_set_bool(s, "comp_on", true);
		obs_data_set_double(s, "threshold", v->thresh);
		obs_data_set_double(s, "ratio", v->ratio);
		obs_data_set_double(s, "attack", v->attack);
		obs_data_set_double(s, "release", v->release);
		obs_data_set_double(s, "makeup", v->makeup);
		obs_data_set_bool(s, "presence_on", v->presence > 0.01f);
		obs_data_set_double(s, "presence", v->presence);
		obs_data_set_bool(s, "sat_on", v->drive > 0.001f);
		obs_data_set_double(s, "drive", v->drive);
		obs_data_set_bool(s, "limit_on", true);
		obs_data_set_string(s, "preset", "custom");
		return true;
	}
	return false;
}

static obs_properties_t *voice_properties(void *data)
{
	UNUSED_PARAMETER(data);
	obs_properties_t *p = obs_properties_create();
	obs_property_t *pr = obs_properties_add_list(p, "preset", "Start from", OBS_COMBO_TYPE_LIST,
						     OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(pr, "Custom", "custom");
	for (size_t i = 0; i < N_VPRESETS; i++)
		obs_property_list_add_string(pr, VPRESETS[i].label, VPRESETS[i].id);
	obs_property_set_modified_callback(pr, on_vpreset);

	obs_properties_t *g1 = obs_properties_create();
	obs_properties_add_bool(g1, "hp_on", "On");
	obs_property_t *hpf = obs_properties_add_float_slider(g1, "hp_freq", "Cut below (Hz)", 20.0, 250.0, 5.0);
	obs_property_set_long_description(hpf, "Desk thumps, footsteps and air conditioning live under about 80 Hz, "
					       "and a voice has nothing down there to lose.");
	obs_properties_add_group(p, "hp", "High-pass", OBS_GROUP_CHECKABLE, g1);

	obs_properties_t *g2 = obs_properties_create();
	obs_properties_add_bool(g2, "gate_on", "On");
	obs_properties_add_float_slider(g2, "gate_open", "Opens above (dB)", -70.0, -10.0, 0.5);
	obs_properties_add_float_slider(g2, "gate_close", "Closes below (dB)", -80.0, -20.0, 0.5);
	obs_properties_add_float_slider(g2, "gate_hold", "Hold open (ms)", 0.0, 800.0, 10.0);
	obs_properties_add_group(p, "gate", "Gate", OBS_GROUP_CHECKABLE, g2);

	obs_properties_t *g3 = obs_properties_create();
	obs_properties_add_bool(g3, "comp_on", "On");
	obs_properties_add_float_slider(g3, "threshold", "Threshold (dB)", -60.0, 0.0, 0.5);
	obs_properties_add_float_slider(g3, "ratio", "Ratio", 1.0, 20.0, 0.1);
	obs_properties_add_float_slider(g3, "knee", "Knee (dB)", 0.0, 24.0, 0.5);
	obs_properties_add_float_slider(g3, "attack", "Attack (ms)", 0.1, 100.0, 0.1);
	obs_properties_add_float_slider(g3, "release", "Release (ms)", 10.0, 1000.0, 5.0);
	obs_properties_add_float_slider(g3, "makeup", "Make-up (dB)", 0.0, 30.0, 0.5);
	obs_properties_add_group(p, "comp", "Compressor", OBS_GROUP_CHECKABLE, g3);

	obs_properties_t *g4 = obs_properties_create();
	obs_properties_add_bool(g4, "presence_on", "On");
	obs_properties_add_float_slider(g4, "presence", "Lift (dB)", -6.0, 12.0, 0.5);
	obs_properties_add_float_slider(g4, "presence_hz", "Around (Hz)", 1500.0, 8000.0, 100.0);
	obs_properties_add_group(p, "pres", "Presence", OBS_GROUP_CHECKABLE, g4);

	obs_properties_t *g5 = obs_properties_create();
	obs_properties_add_bool(g5, "sat_on", "On");
	obs_properties_add_float_slider(g5, "drive", "Drive", 0.0, 1.0, 0.01);
	obs_properties_add_group(p, "sat", "Saturation", OBS_GROUP_CHECKABLE, g5);

	obs_properties_t *g6 = obs_properties_create();
	obs_properties_add_bool(g6, "limit_on", "On");
	obs_properties_add_float_slider(g6, "ceiling", "Ceiling (dB)", -12.0, 0.0, 0.1);
	obs_properties_add_group(p, "lim", "Limiter", OBS_GROUP_CHECKABLE, g6);

	obs_properties_add_text(p, "hint",
				"Add it to the microphone under Filters. Watch SBK Meter while you talk: aim for "
				"the loud moments around −12 to −6 dB and nothing touching the top.",
				OBS_TEXT_INFO);
	return p;
}

static void voice_defaults(obs_data_t *s)
{
	const struct vpreset *v = &VPRESETS[0];
	obs_data_set_default_string(s, "preset", "custom");
	obs_data_set_default_bool(s, "hp_on", true);
	obs_data_set_default_double(s, "hp_freq", v->hp);
	obs_data_set_default_bool(s, "gate_on", false);
	obs_data_set_default_double(s, "gate_open", -45.0);
	obs_data_set_default_double(s, "gate_close", -55.0);
	obs_data_set_default_double(s, "gate_hold", 200.0);
	obs_data_set_default_bool(s, "comp_on", true);
	obs_data_set_default_double(s, "threshold", v->thresh);
	obs_data_set_default_double(s, "ratio", v->ratio);
	obs_data_set_default_double(s, "knee", 6.0);
	obs_data_set_default_double(s, "attack", v->attack);
	obs_data_set_default_double(s, "release", v->release);
	obs_data_set_default_double(s, "makeup", v->makeup);
	obs_data_set_default_bool(s, "presence_on", true);
	obs_data_set_default_double(s, "presence", v->presence);
	obs_data_set_default_double(s, "presence_hz", 3200.0);
	obs_data_set_default_bool(s, "sat_on", true);
	obs_data_set_default_double(s, "drive", v->drive);
	obs_data_set_default_bool(s, "limit_on", true);
	obs_data_set_default_double(s, "ceiling", -1.0);
}

struct obs_source_info sbk_voice_info = {
	.id = "sbk_voice",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_AUDIO,
	.get_name = voice_name,
	.create = voice_create,
	.destroy = voice_destroy,
	.update = voice_update,
	.get_defaults = voice_defaults,
	.get_properties = voice_properties,
	.filter_audio = voice_filter,
};
