#pragma once

#include <math.h>
#include <stdbool.h>

/*  The small amount of signal processing the audio filters need, kept here so
    the two of them agree on what a decibel and an envelope are.

    Everything is per-channel and stateful, so every struct below is held once
    per channel and never shared between them — a compressor whose envelope is
    the sum of two channels pumps on anything panned.  */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SBK_MAX_CH 8

static inline float sbk_db2lin(float db)
{
	return powf(10.0f, db / 20.0f);
}
static inline float sbk_lin2db(float lin)
{
	return 20.0f * log10f(fmaxf(lin, 1e-9f));
}

/* A time constant as a one-pole coefficient. The usual definition: the time to
   travel 1 - 1/e of the way, which is what every compressor spec sheet means. */
static inline float sbk_coef(float ms, float sample_rate)
{
	if (ms <= 0.0f)
		return 0.0f;
	return expf(-1.0f / (0.001f * ms * sample_rate));
}

/* ---- biquad ---------------------------------------------------------------- */

struct sbk_biquad {
	float b0, b1, b2, a1, a2;
	float x1, x2, y1, y2;
};

static inline void sbk_biquad_reset(struct sbk_biquad *f)
{
	f->x1 = f->x2 = f->y1 = f->y2 = 0.0f;
}

static inline void sbk_biquad_hp(struct sbk_biquad *f, float freq, float q, float sr)
{
	float w0 = 2.0f * (float)M_PI * fmaxf(freq, 1.0f) / sr;
	float c = cosf(w0), s = sinf(w0);
	float alpha = s / (2.0f * fmaxf(q, 0.1f));
	float a0 = 1.0f + alpha;
	f->b0 = ((1.0f + c) * 0.5f) / a0;
	f->b1 = (-(1.0f + c)) / a0;
	f->b2 = ((1.0f + c) * 0.5f) / a0;
	f->a1 = (-2.0f * c) / a0;
	f->a2 = (1.0f - alpha) / a0;
}

static inline void sbk_biquad_lp(struct sbk_biquad *f, float freq, float q, float sr)
{
	float w0 = 2.0f * (float)M_PI * fmaxf(freq, 1.0f) / sr;
	float c = cosf(w0), s = sinf(w0);
	float alpha = s / (2.0f * fmaxf(q, 0.1f));
	float a0 = 1.0f + alpha;
	f->b0 = ((1.0f - c) * 0.5f) / a0;
	f->b1 = (1.0f - c) / a0;
	f->b2 = ((1.0f - c) * 0.5f) / a0;
	f->a1 = (-2.0f * c) / a0;
	f->a2 = (1.0f - alpha) / a0;
}

static inline void sbk_biquad_peak(struct sbk_biquad *f, float freq, float q, float gain_db, float sr)
{
	float A = powf(10.0f, gain_db / 40.0f);
	float w0 = 2.0f * (float)M_PI * fmaxf(freq, 1.0f) / sr;
	float c = cosf(w0), s = sinf(w0);
	float alpha = s / (2.0f * fmaxf(q, 0.1f));
	float a0 = 1.0f + alpha / A;
	f->b0 = (1.0f + alpha * A) / a0;
	f->b1 = (-2.0f * c) / a0;
	f->b2 = (1.0f - alpha * A) / a0;
	f->a1 = (-2.0f * c) / a0;
	f->a2 = (1.0f - alpha / A) / a0;
}

static inline float sbk_biquad_run(struct sbk_biquad *f, float x)
{
	float y = f->b0 * x + f->b1 * f->x1 + f->b2 * f->x2 - f->a1 * f->y1 - f->a2 * f->y2;
	f->x2 = f->x1;
	f->x1 = x;
	f->y2 = f->y1;
	f->y1 = y;
	/* a biquad that has been handed a NaN stays broken for ever, and a stream
	   with silent audio for the rest of the night is a bad way to find out */
	if (!isfinite(y)) {
		sbk_biquad_reset(f);
		return 0.0f;
	}
	return y;
}

/* ---- compressor ------------------------------------------------------------ */

struct sbk_comp {
	float env;      /* the detector, in linear */
	float gain_db;  /* the smoothed gain reduction */
};

/* A feed-forward peak compressor with a soft knee, which is what a voice wants:
   the knee is the difference between "processed" and "obviously compressed". */
static inline float sbk_comp_run(struct sbk_comp *c, float x, float threshold_db, float ratio, float knee_db,
				 float attack, float release)
{
	float level = fabsf(x);
	/* attack on the way up, release on the way down */
	float coef = level > c->env ? attack : release;
	c->env = level + coef * (c->env - level);

	float in_db = sbk_lin2db(c->env);
	float over = in_db - threshold_db;
	float reduction = 0.0f;
	if (knee_db > 0.01f && over > -knee_db * 0.5f && over < knee_db * 0.5f) {
		/* quadratic through the knee */
		float t = over + knee_db * 0.5f;
		reduction = (1.0f / ratio - 1.0f) * t * t / (2.0f * knee_db);
	} else if (over >= knee_db * 0.5f) {
		reduction = (1.0f / ratio - 1.0f) * over;
	}
	c->gain_db = reduction;
	return x * sbk_db2lin(reduction);
}

/* ---- limiter --------------------------------------------------------------- */

struct sbk_limiter {
	float env;
};

/* A ceiling, not a sound: instant attack, slow release, so it only ever acts on
   the peaks the compressor did not catch. */
static inline float sbk_limit_run(struct sbk_limiter *l, float x, float ceiling_lin, float release)
{
	float level = fabsf(x);
	if (level > l->env)
		l->env = level;
	else
		l->env = level + release * (l->env - level);
	if (l->env <= ceiling_lin)
		return x;
	return x * (ceiling_lin / fmaxf(l->env, 1e-9f));
}

/* ---- gate ------------------------------------------------------------------ */

struct sbk_gate {
	float env, gain;
	float hold_left;
};

static inline float sbk_gate_run(struct sbk_gate *g, float x, float open_lin, float close_lin, float attack,
				 float release, float hold_s, float sr)
{
	float level = fabsf(x);
	g->env = fmaxf(level, level + 0.999f * (g->env - level));

	bool want_open = g->env > (g->gain > 0.5f ? close_lin : open_lin);
	if (want_open)
		g->hold_left = hold_s;
	else if (g->hold_left > 0.0f)
		g->hold_left -= 1.0f / sr;

	float target = (want_open || g->hold_left > 0.0f) ? 1.0f : 0.0f;
	float coef = target > g->gain ? attack : release;
	g->gain = target + coef * (g->gain - target);
	return x * g->gain;
}

/* ---- saturation ------------------------------------------------------------ */

/* tanh, scaled so drive 0 is exactly unity rather than nearly it */
static inline float sbk_saturate(float x, float drive)
{
	if (drive <= 0.0001f)
		return x;
	float k = 1.0f + drive * 8.0f;
	return tanhf(x * k) / tanhf(k);
}
