/*  Swarnil Broadcast Kit — native overlays for OBS Studio: a tally light that reads the real
    program state, a lower third, a ticker, a camera frame, an audio
    visualizer, a clock, a countdown, an announcement card and a backdrop.
    Sources in OBS's own "+" menu; four scenes built from them under Tools.  */

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/platform.h>

#include "sbk-common.h"
#include "sbk-state.h"
#include "scenes.h"

extern struct obs_source_info sbk_onair_info;
extern struct obs_source_info sbk_lower_third_info;
extern struct obs_source_info sbk_ticker_info;
extern struct obs_source_info sbk_frame_info;
extern struct obs_source_info sbk_visualizer_info;
extern struct obs_source_info sbk_clock_info;
extern struct obs_source_info sbk_countdown_info;
extern struct obs_source_info sbk_card_info;
extern struct obs_source_info sbk_backdrop_info;
extern struct obs_source_info sbk_chip_info;
extern struct obs_source_info sbk_progress_info;
extern struct obs_source_info sbk_counter_info;
extern struct obs_source_info sbk_qr_info;
extern struct obs_source_info sbk_meter_info;
extern struct obs_source_info sbk_stats_info;
extern struct obs_source_info sbk_wipe_info;

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("sbk", "en-US")

MODULE_EXPORT const char *obs_module_name(void)
{
	return "Swarnil Broadcast Kit";
}
MODULE_EXPORT const char *obs_module_description(void)
{
	return "Swarnil Broadcast Kit — native overlays for OBS: a tally light, lower third, ticker, frame, visualizer, clock, countdown, card, backdrop, chip, progress bar and a wipe transition";
}

static void on_build(void *d) { UNUSED_PARAMETER(d); sbk_build_scenes(); }
static void on_collection(void *d) { UNUSED_PARAMETER(d); sbk_create_collection(); }
static void on_pack(void *d) { UNUSED_PARAMETER(d); sbk_add_live_pack(); }
static void on_profile(void *d) { UNUSED_PARAMETER(d); sbk_use_profile(); }

/* Drop a file called .sbk-selftest in the OBS config folder and the next
   start builds the collection and screenshots every scene. Consumed, so it
   fires once. This is how the build is checked without clicking anything. */
static void on_event(enum obs_frontend_event e, void *d)
{
	UNUSED_PARAMETER(d);
	if (e != OBS_FRONTEND_EVENT_FINISHED_LOADING)
		return;
	char *trigger = os_get_config_path_ptr("obs-studio/.sbk-selftest");
	if (trigger && os_file_exists(trigger)) {
		os_unlink(trigger);
		SBK_LOG(LOG_INFO, ".sbk-selftest found — building the collection");
		sbk_selftest();
	}
	bfree(trigger);
}

bool obs_module_load(void)
{
	const uint32_t built = LIBOBS_API_VER, running = obs_get_version();
	SBK_LOG(LOG_INFO, "v%s loaded — built for libobs %u.%u.%u, running %u.%u.%u", SBK_VERSION, built >> 24,
		  (built >> 16) & 0xFF, built & 0xFFFF, running >> 24, (running >> 16) & 0xFF, running & 0xFFFF);

	obs_register_source(&sbk_onair_info);
	obs_register_source(&sbk_lower_third_info);
	obs_register_source(&sbk_ticker_info);
	obs_register_source(&sbk_frame_info);
	obs_register_source(&sbk_visualizer_info);
	obs_register_source(&sbk_clock_info);
	obs_register_source(&sbk_countdown_info);
	obs_register_source(&sbk_card_info);
	obs_register_source(&sbk_backdrop_info);
	obs_register_source(&sbk_chip_info);
	obs_register_source(&sbk_progress_info);
	obs_register_source(&sbk_counter_info);
	obs_register_source(&sbk_qr_info);
	obs_register_source(&sbk_meter_info);
	obs_register_source(&sbk_stats_info);

	/* a real transition type — it appears under "+" in the Scene Transitions panel */
	obs_register_source(&sbk_wipe_info);

	sbk_state_init();
	obs_frontend_add_tools_menu_item("Broadcast Kit: build the show here", on_build, NULL);
	obs_frontend_add_tools_menu_item("Broadcast Kit: create the scene collection", on_collection, NULL);
	obs_frontend_add_tools_menu_item("Broadcast Kit: add the live pack to this scene", on_pack, NULL);
	obs_frontend_add_tools_menu_item("Broadcast Kit: use the Broadcast Kit profile", on_profile, NULL);
	obs_frontend_add_event_callback(on_event, NULL);

	/* create one of each and throw it away — the effects compile in create(),
	   so this proves the shaders build on this machine's renderer */
	static const char *ids[] = {"sbk_onair", "sbk_lower_third", "sbk_ticker", "sbk_frame", "sbk_visualizer",
				    "sbk_clock", "sbk_countdown", "sbk_card", "sbk_backdrop",
				    "sbk_chip", "sbk_progress", "sbk_meter", "sbk_stats", "sbk_counter", "sbk_qr", "sbk_wipe"};
	for (size_t i = 0; i < sizeof(ids) / sizeof(ids[0]); i++) {
		obs_source_t *t = obs_source_create_private(ids[i], NULL, NULL);
		if (t)
			obs_source_release(t);
		else
			SBK_LOG(LOG_WARNING, "self-test: %s would not create", ids[i]);
	}
	return true;
}

void obs_module_unload(void)
{
	sbk_scenes_free();
	SBK_LOG(LOG_INFO, "unloaded");
}
