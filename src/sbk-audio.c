#include <math.h>
#include <string.h>

#include "sbk-common.h"
#include "sbk-audio.h"
#include "sbk-fft.h"

#define LO_HZ 40.0f
#define HI_HZ 16000.0f

/* ---- capture -------------------------------------------------------------- */

static void push_samples(struct sbk_audio *a, const float *l, const float *r, uint32_t frames)
{
	pthread_mutex_lock(&a->lock);
	for (uint32_t i = 0; i < frames; i++) {
		/* mono sum — a meter that ignores one channel looks broken the
		   moment anything is panned */
		a->ring[a->ring_pos] = r ? (l[i] + r[i]) * 0.5f : l[i];
		a->ring_pos = (a->ring_pos + 1) % SBK_FFT_SIZE;
	}
	pthread_mutex_unlock(&a->lock);
}

/* a single source's audio, after its own filters */
static void on_source_audio(void *param, obs_source_t *source, const struct audio_data *data, bool muted)
{
	UNUSED_PARAMETER(source);
	struct sbk_audio *a = param;
	if (muted || !data->frames || !data->data[0])
		return;
	push_samples(a, (const float *)data->data[0], (const float *)data->data[1], data->frames);
}

/* the master mix, mix 0 — what OBS is actually outputting */
static void on_raw_audio(void *param, size_t mix_idx, struct audio_data *data)
{
	UNUSED_PARAMETER(mix_idx);
	struct sbk_audio *a = param;
	if (!data->frames || !data->data[0])
		return;
	push_samples(a, (const float *)data->data[0], (const float *)data->data[1], data->frames);
}

static void detach(struct sbk_audio *a)
{
	if (a->raw_hooked) {
		obs_remove_raw_audio_callback(0, on_raw_audio, a);
		a->raw_hooked = false;
	}
	if (a->weak) {
		obs_source_t *src = obs_weak_source_get_source(a->weak);
		if (src) {
			obs_source_remove_audio_capture_callback(src, on_source_audio, a);
			obs_source_release(src);
		}
		obs_weak_source_release(a->weak);
		a->weak = NULL;
	}
}

/* "@desktop" and friends are OBS's own output channels, which have no stable
   name — what is on channel 1 is whatever the user picked in Settings. */
static obs_source_t *resolve(const char *name)
{
	if (!name || !*name)
		return NULL;
	if (strcmp(name, "@desktop") == 0)
		return obs_get_output_source(1);
	if (strcmp(name, "@desktop2") == 0)
		return obs_get_output_source(2);
	if (strcmp(name, "@mic") == 0)
		return obs_get_output_source(3);
	if (strcmp(name, "@mic2") == 0)
		return obs_get_output_source(4);
	if (strcmp(name, "@mic3") == 0)
		return obs_get_output_source(5);
	return obs_get_source_by_name(name);
}

static void attach(struct sbk_audio *a)
{
	detach(a);
	const char *name = a->source_name;
	if (!name || !*name)
		return;

	if (strcmp(name, "@program") == 0) {
		/* ask for exactly what the analysis wants, so there is no format
		   branch on the audio thread */
		struct audio_convert_info conv = {
			.format = AUDIO_FORMAT_FLOAT_PLANAR,
			.speakers = SPEAKERS_STEREO,
			.samples_per_sec = 48000,
		};
		obs_add_raw_audio_callback(0, &conv, on_raw_audio, a);
		a->raw_hooked = true;
		a->sample_rate = 48000;
		a->warned_missing = false;
		return;
	}

	obs_source_t *src = resolve(name);
	if (!src) {
		if (!a->warned_missing) {
			SBK_LOG(LOG_INFO, "visualizer: nothing on '%s' yet — painting the demo signal", name);
			a->warned_missing = true;
		}
		return;
	}
	a->warned_missing = false;
	obs_source_add_audio_capture_callback(src, on_source_audio, a);
	a->weak = obs_source_get_weak_source(src);
	obs_source_release(src);
}

void sbk_audio_init(struct sbk_audio *a)
{
	pthread_mutex_init(&a->lock, NULL);
	a->sample_rate = 48000;
	a->band_count = 48;
	a->gain = 1.0f;
	a->floor_db = -60.0f;
	a->smoothing = 0.8f;
	a->decay = 0.012f;
	a->level_db = -96.0f;
}

void sbk_audio_free(struct sbk_audio *a)
{
	detach(a);
	pthread_mutex_destroy(&a->lock);
	bfree(a->source_name);
	a->source_name = NULL;
}

void sbk_audio_set_source(struct sbk_audio *a, const char *name)
{
	if (a->source_name && name && strcmp(a->source_name, name) == 0)
		return;
	bfree(a->source_name);
	a->source_name = bstrdup(name ? name : "");
	a->warned_missing = false;
	attach(a);
}

/* ---- analysis ------------------------------------------------------------- */

/* graphics-thread only, so one shared pair of scratch buffers serves every instance */
static float g_re[SBK_FFT_SIZE];
static float g_im[SBK_FFT_SIZE];

static float norm_db(float magnitude, float gain, float floor_db)
{
	/* hearing is logarithmic; a linear bar barely twitches at speech level */
	float db = 20.0f * log10f(magnitude * gain + 1e-9f);
	return sbk_clampf((db - floor_db) / (0.0f - floor_db), 0.0f, 1.0f);
}

static void approach(float *value, float target, float smoothing)
{
	/* attack at once, release slowly — a meter that falls as fast as it
	   rises reads as jitter rather than level */
	*value = target > *value ? target : *value + (target - *value) * (1.0f - smoothing);
}

static void hold_peak(struct sbk_audio *a, int b)
{
	if (a->bands[b] >= a->peaks[b])
		a->peaks[b] = a->bands[b];
	else
		a->peaks[b] = a->peaks[b] > a->decay ? a->peaks[b] - a->decay : 0.0f;
}

/* speech-shaped: syllable-rate bursts with a rolling spectrum, pure maths */
static void demo_tick(struct sbk_audio *a, float seconds)
{
	a->demo_t += seconds;
	const float t = a->demo_t;
	float env = sinf(t * 1.1f) * 0.6f + sinf(t * 0.37f + 2.0f) * 0.5f + 0.3f;
	env = env < 0.0f ? 0.0f : powf(env, 1.4f);
	if (env > 1.0f)
		env = 1.0f;
	for (int b = 0; b < a->band_count; b++) {
		float f = (float)b / (float)(a->band_count > 1 ? a->band_count - 1 : 1);
		float shape = expf(-f * 2.1f) * (0.65f + 0.35f * sinf(t * 3.1f + (float)b * 0.7f));
		float flutter = 0.72f + 0.4f * sinf(t * 9.0f + (float)b * 0.5f);
		float target = sbk_clampf(env * (0.3f + shape * 1.2f) * flutter, 0.0f, 1.0f);
		approach(&a->bands[b], target, a->smoothing);
		hold_peak(a, b);
	}
	approach(&a->level, env * 0.85f, a->smoothing);
	a->level_db = a->floor_db + a->level * (0.0f - a->floor_db);
	a->level_peak = a->level > a->level_peak ? a->level : a->level_peak - a->decay * 0.5f;
	if (a->level_peak < 0.0f)
		a->level_peak = 0.0f;
}

void sbk_audio_tick(struct sbk_audio *a, float seconds)
{
	if (a->band_count < 4)
		a->band_count = 4;
	if (a->band_count > SBK_MAX_BANDS)
		a->band_count = SBK_MAX_BANDS;

	/* it may have appeared after we did: a collection loads its sources
	   after the plugin has built them */
	if (!a->weak && !a->raw_hooked && a->source_name && *a->source_name)
		attach(a);

	a->demo = !a->weak && !a->raw_hooked;
	if (a->demo) {
		demo_tick(a, seconds);
		return;
	}

	struct obs_audio_info oai;
	if (!a->raw_hooked && obs_get_audio_info(&oai))
		a->sample_rate = oai.samples_per_sec;

	float rms = 0.0f;
	pthread_mutex_lock(&a->lock);
	for (int i = 0; i < SBK_FFT_SIZE; i++) {
		float s = a->ring[(a->ring_pos + i) % SBK_FFT_SIZE];
		g_re[i] = s;
		g_im[i] = 0.0f;
		rms += s * s;
	}
	pthread_mutex_unlock(&a->lock);
	rms = sqrtf(rms / (float)SBK_FFT_SIZE);

	sbk_window_hann(g_re, SBK_FFT_SIZE);
	sbk_fft(g_re, g_im, SBK_FFT_SIZE);

	const float sr = a->sample_rate ? (float)a->sample_rate : 48000.0f;
	for (int b = 0; b < a->band_count; b++) {
		/* log spacing: every band covers the same musical interval */
		float t0 = (float)b / (float)a->band_count;
		float t1 = (float)(b + 1) / (float)a->band_count;
		int i0 = sbk_bin_for_hz(LO_HZ * powf(HI_HZ / LO_HZ, t0), SBK_FFT_SIZE, sr);
		int i1 = sbk_bin_for_hz(LO_HZ * powf(HI_HZ / LO_HZ, t1), SBK_FFT_SIZE, sr);
		if (i1 <= i0)
			i1 = i0 + 1;
		if (i1 > SBK_FFT_SIZE / 2)
			i1 = SBK_FFT_SIZE / 2;
		float peak = 0.0f;
		for (int i = i0; i < i1; i++) {
			float mag = sqrtf(g_re[i] * g_re[i] + g_im[i] * g_im[i]);
			if (mag > peak)
				peak = mag;
		}
		peak = peak * 2.0f / (float)SBK_FFT_SIZE;
		approach(&a->bands[b], norm_db(peak, a->gain, a->floor_db), a->smoothing);
		hold_peak(a, b);
	}

	a->level_db = 20.0f * log10f(rms * a->gain + 1e-9f);
	approach(&a->level, norm_db(rms, a->gain, a->floor_db), a->smoothing);
	/* the peak indicator falls at half the band decay: a peak that vanishes
	   as fast as the bar is no peak indicator at all */
	a->level_peak = a->level > a->level_peak ? a->level : a->level_peak - a->decay * 0.5f;
	if (a->level_peak < 0.0f)
		a->level_peak = 0.0f;
}

void sbk_audio_waveform(struct sbk_audio *a, float *out, int count, float gain)
{
	if (a->demo) {
		for (int i = 0; i < count; i++) {
			float p = (float)i / (float)count * 6.2831853f;
			float s = a->level * (sinf(p * 6.0f + a->demo_t * 9.0f) * 0.5f +
					      sinf(p * 13.0f + a->demo_t * 5.0f) * 0.3f);
			out[i] = 0.5f + sbk_clampf(s * gain, -1.0f, 1.0f) * 0.5f;
		}
		return;
	}
	pthread_mutex_lock(&a->lock);
	for (int i = 0; i < count; i++) {
		size_t back = (size_t)(count - i) * 4;
		size_t idx = (a->ring_pos + SBK_FFT_SIZE - back) % SBK_FFT_SIZE;
		out[i] = 0.5f + sbk_clampf(a->ring[idx] * gain, -1.0f, 1.0f) * 0.5f;
	}
	pthread_mutex_unlock(&a->lock);
}

/* ---- the picker ----------------------------------------------------------- */

static bool add_audio_source(void *param, obs_source_t *src)
{
	obs_property_t *list = param;
	if (obs_source_get_output_flags(src) & OBS_SOURCE_AUDIO) {
		const char *name = obs_source_get_name(src);
		if (name)
			obs_property_list_add_string(list, name, name);
	}
	return true;
}

/* the channel's own name where OBS has one, so the list reads like Settings */
static void add_channel(obs_property_t *list, const char *label, const char *key, uint32_t channel)
{
	obs_source_t *src = obs_get_output_source(channel);
	struct dstr s = {0};
	if (src) {
		dstr_printf(&s, "%s — %s", label, obs_source_get_name(src));
		obs_source_release(src);
	} else {
		dstr_printf(&s, "%s — not set", label);
	}
	obs_property_list_add_string(list, s.array, key);
	dstr_free(&s);
}

void sbk_audio_fill_source_list(obs_property_t *list)
{
	obs_property_list_add_string(list, "Program — everything OBS is outputting", "@program");
	add_channel(list, "Desktop Audio", "@desktop", 1);
	add_channel(list, "Desktop Audio 2", "@desktop2", 2);
	add_channel(list, "Mic/Aux", "@mic", 3);
	add_channel(list, "Mic/Aux 2", "@mic2", 4);
	add_channel(list, "Mic/Aux 3", "@mic3", 5);
	obs_enum_sources(add_audio_source, list);
	obs_property_list_add_string(list, "Demo signal — for setting the look up in silence", "");
}
