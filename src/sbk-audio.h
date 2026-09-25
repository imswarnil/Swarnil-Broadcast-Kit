#pragma once

#include <obs-module.h>
#include <pthread.h>
#include <stdbool.h>

/*  Listens to OBS's audio and turns it into values a shader draws: log-spaced
    bands with peak hold, a waveform, and a broadcast-style level in dB.

    What it listens to is chosen by name:

      "@program"   the master mix — literally what OBS is sending out, every
                   source and every filter, the thing a viewer hears. This is
                   the default, because a visualizer that ignores the desktop
                   audio and the music bed is a visualizer of the wrong stream.
      "@desktop"   Desktop Audio, and "@desktop2" the second one
      "@mic"       Mic/Aux, and "@mic2" / "@mic3" the others
      <a name>     any source in the scene collection that carries audio
      ""           the demo signal

    Audio arrives on OBS's audio thread and is drawn on the graphics thread, so
    the sample ring sits behind a mutex and the analysis runs in video_tick.
    Nothing is allocated per frame. When the chosen source is not there — a
    collection still loading, a device unplugged — the demo signal plays, so an
    overlay is never a dead rectangle on someone's stream.  */

#define SBK_FFT_SIZE 2048
#define SBK_MAX_BANDS 128

struct sbk_audio {
	char *source_name;
	obs_weak_source_t *weak;   /* a named source or an output channel */
	bool raw_hooked;           /* the master mix, which has no source object */
	bool warned_missing;
	bool demo;                 /* what is actually being drawn right now */

	pthread_mutex_t lock;
	float ring[SBK_FFT_SIZE];
	size_t ring_pos;
	uint32_t sample_rate;

	float bands[SBK_MAX_BANDS];
	float peaks[SBK_MAX_BANDS];
	int band_count;

	/* broadcast levels, for the meter: 0..1 mapped from floor_db to 0 dB */
	float level, level_peak, level_db;

	float gain, floor_db, smoothing, decay;
	float demo_t;
};

void sbk_audio_init(struct sbk_audio *a);
void sbk_audio_free(struct sbk_audio *a);
void sbk_audio_set_source(struct sbk_audio *a, const char *name);
void sbk_audio_tick(struct sbk_audio *a, float seconds);
/* newest samples decimated into count slots, centred on 0.5 */
void sbk_audio_waveform(struct sbk_audio *a, float *out, int count, float gain);
void sbk_audio_fill_source_list(obs_property_t *list);
