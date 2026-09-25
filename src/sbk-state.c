#include <obs-module.h>
#include <util/platform.h>
#include <obs-frontend-api.h>

#include "sbk-state.h"

struct sbk_status sbk_status;

static void on_event(enum obs_frontend_event e, void *data)
{
	UNUSED_PARAMETER(data);
	switch (e) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		sbk_status.streaming = obs_frontend_streaming_active();
		sbk_status.recording = obs_frontend_recording_active();
		sbk_status.paused = obs_frontend_recording_paused();
		/* OBS may already have been live when the plugin loaded */
		if (sbk_status.streaming && !sbk_status.stream_started_ns)
			sbk_status.stream_started_ns = os_gettime_ns();
		if (sbk_status.recording && !sbk_status.record_started_ns)
			sbk_status.record_started_ns = os_gettime_ns();
		sbk_status.ready = true;
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STARTED:
		sbk_status.streaming = true;
		sbk_status.stream_started_ns = os_gettime_ns();
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STOPPED: sbk_status.streaming = false; break;
	case OBS_FRONTEND_EVENT_RECORDING_STARTED:
		sbk_status.recording = true;
		sbk_status.paused = false;
		sbk_status.record_started_ns = os_gettime_ns();
		break;
	case OBS_FRONTEND_EVENT_RECORDING_PAUSED: sbk_status.paused = true; break;
	case OBS_FRONTEND_EVENT_RECORDING_UNPAUSED: sbk_status.paused = false; break;
	case OBS_FRONTEND_EVENT_RECORDING_STOPPED:
		sbk_status.recording = false;
		sbk_status.paused = false;
		break;
	default: break;
	}
}

void sbk_state_init(void)
{
	obs_frontend_add_event_callback(on_event, NULL);
}

const char *sbk_state_word(void)
{
	if (sbk_status.streaming && sbk_status.recording)
		return "both";
	if (sbk_status.streaming)
		return "live";
	if (sbk_status.recording)
		return "rec";
	return "off";
}

double sbk_state_uptime(bool recording)
{
	uint64_t start = recording ? sbk_status.record_started_ns : sbk_status.stream_started_ns;
	bool on = recording ? sbk_status.recording : sbk_status.streaming;
	if (!on || !start)
		return 0.0;
	return (double)(os_gettime_ns() - start) / 1000000000.0;
}
