/*  SBK Radio — the character filters: a telephone, a radio, a megaphone, a
    tannoy. Band-limit the voice, squash it, and add the distortion the medium
    would have added.

    It exists because the alternative is stacking three of OBS's filters and
    guessing at the frequencies, and because a stream that cuts to "we are
    having technical difficulties" over a tannoy voice is funnier than one that
    does not.  */

#include <obs-module.h>
#include <media-io/audio-io.h>

#include "sbk-common.h"
#include "sbk-dsp.h"

struct radio {
	obs_source_t *self;
	size_t channels;
	float sr;

	float low_hz, high_hz, drive, squash, noise, mix, out_db;
	struct sbk_biquad hp[SBK_MAX_CH], lp[SBK_MAX_CH], peak[SBK_MAX_CH];
	struct sbk_comp comp[SBK_MAX_CH];
	float a_coef, r_coef;
	uint32_t rng;
};

static const char *radio_name(void *u)
{
	UNUSED_PARAMETER(u);
	return obs_module_text("SBK.Radio");
}

static void radio_recalc(struct radio *r)
{
	for (size_t c = 0; c < SBK_MAX_CH; c++) {
		sbk_biquad_hp(&r->hp[c], r->low_hz, 0.8f, r->sr);
		sbk_biquad_lp(&r->lp[c], r->high_hz, 0.8f, r->sr);
		/* the honk in the middle is what makes a small speaker sound small */
		sbk_biquad_peak(&r->peak[c], sqrtf(r->low_hz * r->high_hz), 1.2f, 6.0f, r->sr);
	}
	r->a_coef = sbk_coef(1.0f, r->sr);
	r->r_coef = sbk_coef(60.0f, r->sr);
}

static void radio_update(void *data, obs_data_t *s)
{
	struct radio *r = data;
	r->low_hz = (float)obs_data_get_double(s, "low");
	r->high_hz = (float)obs_data_get_double(s, "high");
	if (r->high_hz <= r->low_hz * 1.2f)
		r->high_hz = r->low_hz * 1.2f;
	r->drive = (float)obs_data_get_double(s, "drive");
	r->squash = (float)obs_data_get_double(s, "squash");
	r->noise = (float)obs_data_get_double(s, "noise");
	r->mix = (float)obs_data_get_double(s, "mix");
	r->out_db = (float)obs_data_get_double(s, "out");
	radio_recalc(r);
}

static void *radio_create(obs_data_t *s, obs_source_t *source)
{
	struct radio *r = bzalloc(sizeof(*r));
	r->self = source;
	audio_t *a = obs_get_audio();
	r->sr = a ? (float)audio_output_get_sample_rate(a) : 48000.0f;
	r->channels = a ? audio_output_get_channels(a) : 2;
	if (r->channels > SBK_MAX_CH)
		r->channels = SBK_MAX_CH;
	r->rng = 0x9e3779b9u;
	radio_update(r, s);
	return r;
}

static void radio_destroy(void *data)
{
	bfree(data);
}

/* xorshift: the noise only has to sound like hiss, and a real PRNG per sample
   would cost more than the rest of the filter */
static inline float radio_noise(struct radio *r)
{
	r->rng ^= r->rng << 13;
	r->rng ^= r->rng >> 17;
	r->rng ^= r->rng << 5;
	return ((float)(r->rng & 0xFFFF) / 32768.0f) - 1.0f;
}

static struct obs_audio_data *radio_filter(void *data, struct obs_audio_data *audio)
{
	struct radio *r = data;
	const float out = sbk_db2lin(r->out_db);
	/* squash 0..1 maps onto a threshold and ratio that go from "barely" to
	   "pinned", because two more sliders would not help anyone */
	const float thresh = -6.0f - r->squash * 30.0f;
	const float ratio = 1.5f + r->squash * 14.0f;

	for (size_t c = 0; c < r->channels; c++) {
		float *samples = (float *)audio->data[c];
		if (!samples)
			continue;
		for (uint32_t i = 0; i < audio->frames; i++) {
			const float dry = samples[i];
			float x = dry;

			x = sbk_biquad_run(&r->hp[c], x);
			x = sbk_biquad_run(&r->lp[c], x);
			x = sbk_biquad_run(&r->peak[c], x);
			if (r->squash > 0.001f)
				x = sbk_comp_run(&r->comp[c], x, thresh, ratio, 3.0f, r->a_coef, r->r_coef);
			x = sbk_saturate(x, r->drive);
			if (r->noise > 0.0001f)
				x += radio_noise(r) * r->noise * 0.05f;
			x *= out;

			samples[i] = dry + (x - dry) * r->mix;
		}
	}
	return audio;
}

struct rpreset {
	const char *id, *label;
	float low, high, drive, squash, noise, out;
};

static const struct rpreset RPRESETS[] = {
	{"phone", "Telephone", 400.0f, 3000.0f, 0.25f, 0.45f, 0.05f, 4.0f},
	{"radio", "AM radio", 250.0f, 4500.0f, 0.35f, 0.60f, 0.16f, 5.0f},
	{"megaphone", "Megaphone", 500.0f, 3800.0f, 0.70f, 0.75f, 0.02f, 6.0f},
	{"tannoy", "Tannoy — a big room", 300.0f, 3200.0f, 0.40f, 0.55f, 0.08f, 5.0f},
	{"walkie", "Walkie-talkie", 450.0f, 2800.0f, 0.85f, 0.85f, 0.28f, 6.0f},
};
#define N_RPRESETS (sizeof(RPRESETS) / sizeof(RPRESETS[0]))

static bool on_rpreset(obs_properties_t *props, obs_property_t *p, obs_data_t *s)
{
	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(p);
	const char *want = obs_data_get_string(s, "preset");
	if (!want || astrcmpi(want, "custom") == 0)
		return false;
	for (size_t i = 0; i < N_RPRESETS; i++) {
		if (astrcmpi(RPRESETS[i].id, want) != 0)
			continue;
		const struct rpreset *v = &RPRESETS[i];
		obs_data_set_double(s, "low", v->low);
		obs_data_set_double(s, "high", v->high);
		obs_data_set_double(s, "drive", v->drive);
		obs_data_set_double(s, "squash", v->squash);
		obs_data_set_double(s, "noise", v->noise);
		obs_data_set_double(s, "out", v->out);
		obs_data_set_string(s, "preset", "custom");
		return true;
	}
	return false;
}

static obs_properties_t *radio_properties(void *data)
{
	UNUSED_PARAMETER(data);
	obs_properties_t *p = obs_properties_create();
	obs_property_t *pr = obs_properties_add_list(p, "preset", "Start from", OBS_COMBO_TYPE_LIST,
						     OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(pr, "Custom", "custom");
	for (size_t i = 0; i < N_RPRESETS; i++)
		obs_property_list_add_string(pr, RPRESETS[i].label, RPRESETS[i].id);
	obs_property_set_modified_callback(pr, on_rpreset);

	obs_properties_add_float_slider(p, "low", "Cut below (Hz)", 100.0, 1200.0, 10.0);
	obs_properties_add_float_slider(p, "high", "Cut above (Hz)", 1000.0, 8000.0, 50.0);
	obs_properties_add_float_slider(p, "squash", "Squash", 0.0, 1.0, 0.01);
	obs_properties_add_float_slider(p, "drive", "Distortion", 0.0, 1.0, 0.01);
	obs_properties_add_float_slider(p, "noise", "Hiss", 0.0, 1.0, 0.01);
	obs_properties_add_float_slider(p, "out", "Output (dB)", -12.0, 18.0, 0.5);
	obs_property_t *mix = obs_properties_add_float_slider(p, "mix", "Amount", 0.0, 1.0, 0.01);
	obs_property_set_long_description(mix, "Blends against the untouched voice. Below 1 the effect sits behind "
					       "the real thing, which is usually more convincing than all of it.");
	obs_properties_add_text(p, "hint",
				"Add it to a microphone, or to a media source for a voice coming out of a radio in "
				"the scene. Turn Amount down to nothing to hear the difference while you set it.",
				OBS_TEXT_INFO);
	return p;
}

static void radio_defaults(obs_data_t *s)
{
	const struct rpreset *v = &RPRESETS[0];
	obs_data_set_default_string(s, "preset", "custom");
	obs_data_set_default_double(s, "low", v->low);
	obs_data_set_default_double(s, "high", v->high);
	obs_data_set_default_double(s, "drive", v->drive);
	obs_data_set_default_double(s, "squash", v->squash);
	obs_data_set_default_double(s, "noise", v->noise);
	obs_data_set_default_double(s, "out", v->out);
	obs_data_set_default_double(s, "mix", 1.0);
}

struct obs_source_info sbk_radio_info = {
	.id = "sbk_radio",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_AUDIO,
	.get_name = radio_name,
	.create = radio_create,
	.destroy = radio_destroy,
	.update = radio_update,
	.get_defaults = radio_defaults,
	.get_properties = radio_properties,
	.filter_audio = radio_filter,
};
