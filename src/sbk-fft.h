#pragma once

#include <math.h>

/*  A radix-2 FFT, small enough to keep in the plugin. The visualizer needs a
    spectrum every frame; a library for sixty lines of butterflies would mean
    linking something other than libobs, and the build having a download step.
    n must be a power of two; the input is real, so bins above n/2 mirror.  */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static inline void sbk_fft(float *re, float *im, int n)
{
	for (int i = 1, j = 0; i < n; i++) {
		int bit = n >> 1;
		for (; j & bit; bit >>= 1)
			j ^= bit;
		j ^= bit;
		if (i < j) {
			float tr = re[i]; re[i] = re[j]; re[j] = tr;
			float ti = im[i]; im[i] = im[j]; im[j] = ti;
		}
	}
	for (int len = 2; len <= n; len <<= 1) {
		const float ang = -2.0f * (float)M_PI / (float)len;
		const float wr = cosf(ang), wi = sinf(ang);
		for (int i = 0; i < n; i += len) {
			float cur_r = 1.0f, cur_i = 0.0f;
			for (int k = 0; k < len / 2; k++) {
				const int a = i + k, b = a + len / 2;
				const float xr = re[b] * cur_r - im[b] * cur_i;
				const float xi = re[b] * cur_i + im[b] * cur_r;
				re[b] = re[a] - xr;
				im[b] = im[a] - xi;
				re[a] += xr;
				im[a] += xi;
				const float nr = cur_r * wr - cur_i * wi;
				cur_i = cur_r * wi + cur_i * wr;
				cur_r = nr;
			}
		}
	}
}

/* Hann, so a tone between two bins does not smear across the spectrum */
static inline void sbk_window_hann(float *buf, int n)
{
	for (int i = 0; i < n; i++)
		buf[i] *= 0.5f * (1.0f - cosf(2.0f * (float)M_PI * (float)i / (float)(n - 1)));
}

static inline int sbk_bin_for_hz(float hz, int n, float sample_rate)
{
	int bin = (int)(hz * (float)n / sample_rate);
	return bin < 0 ? 0 : bin;
}
