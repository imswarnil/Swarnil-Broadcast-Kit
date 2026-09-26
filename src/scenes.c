/*  The show — ten complete scenes built from the native sources and placed on
    a 1080p grid (scaled to whatever the canvas actually is), so a fresh install
    has a whole stream to look at rather than a blank canvas.

    Every source is configured through the same obs_data settings the properties
    dialog writes, so a built scene is exactly a hand-built one: open any item's
    properties and everything is there to change.

    The scenes deliberately leave the camera and the screen capture to you.
    The kit draws the treatment — a frame, a lower third, a light — and a frame
    with your own camera placed under it is the whole point; creating a capture
    device on your behalf would switch your webcam on just because you opened
    the menu.  */

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/dstr.h>
#include <util/platform.h>
#include <util/threading.h>
#include <pthread.h>

#include "sbk-common.h"
#include "scenes.h"

#define COLLECTION "Swarnil Broadcast Kit"
#define PROFILE "Swarnil Broadcast Kit"
#define TRANSITION "SBK Wipe"

/* the 1080p grid every position below is written in */
#define EDGE 120.0f
#define MIDX 960.0f

static float K = 1.0f; /* canvas width / 1920 */
static float W = 1920.0f, H = 1080.0f;

static void measure(void)
{
	struct obs_video_info ovi;
	if (obs_get_video_info(&ovi) && ovi.base_width && ovi.base_height) {
		W = (float)ovi.base_width;
		H = (float)ovi.base_height;
	}
	K = W / 1920.0f;
}

/*  Rebuilding a collection safely.

    The first version of this removed each scene and made it again. That is a
    double-free waiting to happen, and it happened: removing a scene releases
    everything in it, so a source used by only that scene drops to no references
    and starts being destroyed — while still answering to its name for a moment.
    The next obs_get_source_by_name hands back a corpse, the caller releases it,
    and OBS aborts inside malloc. The backtrace points at the innocent release.

    Two rules fix it for good:

      · every source this build touches is referenced until the whole build is
        done, so nothing can reach zero part-way through;
      · a scene that already exists is emptied and refilled rather than removed
        and remade, which also keeps the scene order the user arranged.  */

#define MAX_HELD 256
static obs_source_t *g_held[MAX_HELD];
static int g_n_held;

static obs_source_t *hold(obs_source_t *src)
{
	if (!src)
		return NULL;
	if (g_n_held < MAX_HELD)
		g_held[g_n_held++] = src; /* the reference moves into the array */
	else
		obs_source_release(src);  /* absurd, but do not leak */
	return src;
}

static void release_held(void)
{
	for (int i = 0; i < g_n_held; i++)
		obs_source_release(g_held[i]);
	g_n_held = 0;
}

/* Settings as one JSON literal — thirty obs_data trees call-by-call would be
   three times this file. The returned source is BORROWED: release_held() owns
   it until the build finishes. */
/*  A source and a scene share one namespace.

    obs_get_source_by_name finds scenes too, so a source named after a scene
    resolves to the scene, which is then quietly asked to contain itself. OBS
    refuses and the item simply never appears — no warning anywhere. Naming a
    source after its own scene is easy to do by accident, so it is caught here
    rather than left to a screenshot to reveal.  */
static obs_source_t *comp(const char *id, const char *name, const char *json)
{
	obs_source_t *existing = obs_get_source_by_name(name);
	if (existing && obs_scene_from_source(existing)) {
		SBK_LOG(LOG_WARNING, "'%s' is a scene — a source cannot share its name; skipped", name);
		obs_source_release(existing);
		return NULL;
	}
	if (existing) {
		if (json) {
			obs_data_t *st = obs_data_create_from_json(json);
			if (st) {
				obs_source_update(existing, st);
				obs_data_release(st);
			}
		}
		return hold(existing);
	}
	obs_data_t *st = json ? obs_data_create_from_json(json) : NULL;
	obs_source_t *src = obs_source_create(id, name, st, NULL);
	if (st)
		obs_data_release(st);
	if (!src)
		SBK_LOG(LOG_WARNING, "could not create %s (%s)", name, id);
	return hold(src);
}

/* place at (x, y) in 1080p units, anchored by align (OBS_ALIGN_*) */
static void put(obs_scene_t *scene, obs_source_t *src, float x, float y, uint32_t align)
{
	if (!scene || !src)
		return;
	obs_sceneitem_t *it = obs_scene_add(scene, src);
	if (!it)
		return;
	struct vec2 pos;
	vec2_set(&pos, x * K, y * K);
	obs_sceneitem_set_alignment(it, align);
	obs_sceneitem_set_pos(it, &pos);
	if (K != 1.0f) {
		struct vec2 sc;
		vec2_set(&sc, K, K);
		obs_sceneitem_set_scale(it, &sc);
	}
}

/* Empty a scene that is already there, or make it. Either way the caller gets
   a scene it must obs_scene_release(). */
#define MAX_ITEMS 64
struct item_bag {
	obs_sceneitem_t *items[MAX_ITEMS];
	int n;
};

static bool collect_item(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	UNUSED_PARAMETER(scene);
	struct item_bag *bag = param;
	if (bag->n < MAX_ITEMS) {
		obs_sceneitem_addref(item);
		bag->items[bag->n++] = item;
	}
	return true;
}

static obs_scene_t *fresh_scene(const char *name)
{
	obs_source_t *existing = obs_get_source_by_name(name);
	if (existing) {
		obs_scene_t *sc = obs_scene_from_source(existing);
		if (sc) {
			/* collect first, then remove: removing inside the enumeration
			   mutates the list being walked */
			struct item_bag bag = {0};
			obs_scene_enum_items(sc, collect_item, &bag);
			for (int i = 0; i < bag.n; i++) {
				obs_sceneitem_remove(bag.items[i]);
				obs_sceneitem_release(bag.items[i]);
			}
			/* the caller's obs_scene_release matches the reference that
			   obs_get_source_by_name just took */
			return sc;
		}
		obs_source_release(existing);
	}
	return obs_scene_create(name);
}

/* ---- the real devices ------------------------------------------------------

    The kit used to leave the camera and the microphone to you, on the grounds
    that creating a capture switches the webcam light on. That is true, and it
    also meant a freshly built show was thirteen scenes of empty rectangles with
    no way to tell whether any of it worked. Building the collection is an
    explicit menu action, so the devices go in — and every one of them is an
    ordinary source you can swap, disable or delete.  */

#define CAM_NAME "Camera"
#define MIC_NAME "Mic"
#define SCREEN_NAME "Screen"

/* The first real entry in a source type's device list. OBS only fills that list
   when the source exists, so one is made, asked, and thrown away. */
static char *first_device(const char *source_id, const char *prop)
{
	obs_source_t *tmp = obs_source_create_private(source_id, NULL, NULL);
	if (!tmp)
		return NULL;
	obs_properties_t *props = obs_source_properties(tmp);
	char *out = NULL;
	if (props) {
		obs_property_t *p = obs_properties_get(props, prop);
		if (p) {
			size_t n = obs_property_list_item_count(p);
			for (size_t i = 0; i < n; i++) {
				const char *v = obs_property_list_item_string(p, i);
				if (v && *v) {
					out = bstrdup(v);
					break;
				}
			}
		}
		obs_properties_destroy(props);
	}
	obs_source_release(tmp);
	return out;
}

/* The shared camera, with its corners actually rounded rather than covered by
   something rounded. Borrowed like everything else comp() returns. */
static obs_source_t *camera(void)
{
	obs_source_t *existing = obs_get_source_by_name(CAM_NAME);
	if (existing)
		return hold(existing);

	char *dev = first_device("macos-avcapture", "device");
	if (!dev) {
		SBK_LOG(LOG_INFO, "no camera found — the frames are left empty for you to fill");
		return NULL;
	}
	obs_data_t *st = obs_data_create();
	obs_data_set_string(st, "device", dev);
	obs_source_t *src = obs_source_create("macos-avcapture", CAM_NAME, st, NULL);
	obs_data_release(st);
	bfree(dev);
	if (!src) {
		SBK_LOG(LOG_WARNING, "could not open the camera");
		return NULL;
	}

	/* the filter, not an overlay: the corners are gone from the picture, so
	   anything can sit behind it */
	obs_source_t *f = obs_source_create_private("sbk_round", "Round corners", NULL);
	if (f) {
		obs_source_filter_add(src, f);
		obs_source_release(f);
	}
	SBK_LOG(LOG_INFO, "camera added with rounded corners");
	return hold(src);
}

/*  The display capture.

    ScreenCaptureKit, the same source the Sources menu offers, created here so a
    two-up or three-up layout arrives with something in the big hole rather than
    a rectangle and an instruction. macOS asks for Screen Recording permission
    the first time; until it is given the capture is black, which is the system
    telling you, not a fault here.  */
static obs_source_t *screen_capture(void)
{
	obs_source_t *existing = obs_get_source_by_name(SCREEN_NAME);
	if (existing)
		return hold(existing);

	obs_data_t *st = obs_data_create();
	obs_data_set_int(st, "type", 0);       /* 0 = a whole display */
	obs_data_set_bool(st, "show_cursor", true);
	obs_source_t *src = obs_source_create("screen_capture", SCREEN_NAME, st, NULL);
	obs_data_release(st);
	if (!src) {
		SBK_LOG(LOG_INFO, "no display capture available — the big frames are left for you to fill");
		return NULL;
	}
	SBK_LOG(LOG_INFO, "display capture added (grant Screen Recording if it is black)");
	return hold(src);
}

/* The microphone goes on OBS's own Mic/Aux channel rather than into one scene,
   because that is where the mixer expects it and it is what "@mic" means to the
   meter and the visualizer. An input the user has already chosen is left alone. */
static void ensure_mic(void)
{
	obs_source_t *existing = obs_get_output_source(3);
	if (existing) {
		obs_source_release(existing);
		return;
	}
	char *dev = first_device("coreaudio_input_capture", "device_id");
	if (!dev) {
		SBK_LOG(LOG_INFO, "no audio input found — the meters will paint the demo signal");
		return;
	}
	obs_data_t *st = obs_data_create();
	obs_data_set_string(st, "device_id", dev);
	obs_source_t *mic = obs_source_create("coreaudio_input_capture", MIC_NAME, st, NULL);
	obs_data_release(st);
	bfree(dev);
	if (!mic) {
		SBK_LOG(LOG_WARNING, "could not open the audio input");
		return;
	}
	obs_set_output_source(3, mic);
	obs_source_release(mic);
	SBK_LOG(LOG_INFO, "microphone set on the Mic/Aux channel");
}

/* Put a source in a box of a given size, cropping rather than squashing — which
   is what you want for a camera whose shape never matches the hole. */
static void put_box(obs_scene_t *scene, obs_source_t *src, float x, float y, float w, float h, uint32_t align)
{
	if (!scene || !src)
		return;
	obs_sceneitem_t *it = obs_scene_add(scene, src);
	if (!it)
		return;
	struct vec2 pos, bounds;
	vec2_set(&pos, x * K, y * K);
	vec2_set(&bounds, w * K, h * K);
	obs_sceneitem_set_alignment(it, align);
	obs_sceneitem_set_pos(it, &pos);
	obs_sceneitem_set_bounds_type(it, OBS_BOUNDS_SCALE_OUTER);
	obs_sceneitem_set_bounds_alignment(it, OBS_ALIGN_CENTER);
	obs_sceneitem_set_bounds(it, &bounds);
}

/* ---- the pieces every scene is assembled from --------------------------- */

static const uint32_t TL = OBS_ALIGN_TOP | OBS_ALIGN_LEFT;
static const uint32_t TR = OBS_ALIGN_TOP | OBS_ALIGN_RIGHT;
static const uint32_t BL = OBS_ALIGN_BOTTOM | OBS_ALIGN_LEFT;
static const uint32_t BR = OBS_ALIGN_BOTTOM | OBS_ALIGN_RIGHT;
static const uint32_t TC = OBS_ALIGN_TOP | OBS_ALIGN_CENTER;

/* A printf that hands back a string with no freeing at the call site, so a
   scene reads as one line. It cycles a small ring of buffers because two calls
   land in the same expression — light() builds a name and its settings — and
   one shared buffer would have the second overwrite the first. */
static struct dstr g_fmt_ring[6];

static const char *fmt(const char *f, ...)
{
	struct dstr *ring = g_fmt_ring;
	static int next;
	struct dstr *out = &ring[next];
	next = (next + 1) % (int)(sizeof(g_fmt_ring) / sizeof(g_fmt_ring[0]));
	va_list args;
	va_start(args, f);
	dstr_vprintf(out, f, args);
	va_end(args);
	return out->array;
}

/*  A plate goes behind a box, so it has to line up with that box exactly.

    The plate reports itself bigger than its shape — the shadow needs room to
    fall outside it — so anchoring one by a corner puts the shape a shadow's
    width away from whatever it is meant to sit under. The padding is symmetric,
    so the shape's centre is the source's centre: place the plate centred on the
    box's centre and the two register whatever the shadow is set to.  */
static void put_plate(obs_scene_t *scene, obs_source_t *src, float x, float y, float w, float h, uint32_t align)
{
	float cx = (align & OBS_ALIGN_LEFT) ? x + w * 0.5f : (align & OBS_ALIGN_RIGHT) ? x - w * 0.5f : x;
	float cy = (align & OBS_ALIGN_TOP) ? y + h * 0.5f : (align & OBS_ALIGN_BOTTOM) ? y - h * 0.5f : y;
	put(scene, src, cx, cy, OBS_ALIGN_CENTER);
}

static const char *card_json(const char *eyebrow, const char *title, const char *body, const char *chips,
			     const char *variant, const char *align, int width)
{
	return fmt("{\"eyebrow\":\"%s\",\"title\":\"%s\",\"body\":\"%s\",\"chips\":\"%s\","
		   "\"variant\":\"%s\",\"align\":\"%s\",\"width\":%d}",
		   eyebrow, title, body, chips, variant, align, width);
}

/* the light, shared by every scene so there is one of it in the collection */
static obs_source_t *light(const char *shape)
{
	return comp("sbk_onair", fmt("SBK · Light%s%s", *shape ? " " : "", shape),
		    fmt("{\"shape\":\"%s\"}", *shape ? shape : "pill"));
}

static obs_source_t *backdrop(const char *name, const char *mode, const char *extra)
{
	if (!extra)
		extra = "";
	return comp("sbk_backdrop", name,
		    fmt("{\"mode\":\"%s\",\"color\":%u,\"width\":%d,\"height\":%d%s%s}", mode, (unsigned)SBK_SOLID,
			(int)W, (int)H, *extra ? "," : "", extra));
}

/* ---- the show ------------------------------------------------------------ */

/*  The show.

    Ordered deliberately. OBS lists scenes with the most recently added at the
    top, so these are built in reverse of how you would run them — the result is
    a Scenes panel that reads from "Starting soon" at the top down to the private
    desk at the bottom, in the order of an actual broadcast.

    A scene that already exists is emptied and refilled rather than removed, so
    anything you dragged somewhere else stays where you put it.

    Grouped: the opening, the teaching scenes, the talking scenes, the breaks,
    the closing, and one that is not for the stream at all.  */

struct scene_spec {
	const char *name;
	void (*build)(obs_scene_t *sc);
};

/* ---- the opening ----------------------------------------------------------- */

static void scene_starting(obs_scene_t *sc)
{
	put(sc, backdrop("SBK · Backdrop grid", "grid", "\"drift\":6.0,\"pitch\":72.0"), 0, 0, TL);
	put(sc, comp("sbk_visualizer", "SBK · Viz bars", "{\"style\":\"bars\",\"height\":240}"), 0, 1080, BL);
	put(sc, light(""), EDGE, EDGE, TL);
	put(sc, comp("sbk_clock", "SBK · Clock", NULL), 1920 - EDGE, EDGE, TR);
	put(sc, comp("sbk_card", "SBK · Starting soon card",
		     card_json("Starting soon", "Building a Salesforce app live",
			       "Grab a coffee. We begin at the top of the hour.",
			       "@imswarnil | youtube.com/@imswarnil | imswarnil.com", "card", "left", 1100)),
	    EDGE, 300, TL);
	put(sc, comp("sbk_social", "SBK · Social stack",
		     "{\"mode\":\"stack\",\"variant\":\"card\",\"brand\":true}"),
	    EDGE, 820, BL);
	put(sc, comp("sbk_countdown", "SBK · Countdown",
		     "{\"mode\":\"duration\",\"minutes\":15,\"style\":\"ring\",\"ring_size\":300,\"label\":\"We begin in\"}"),
	    1920 - EDGE, 300, TR);
}

static void scene_welcome(obs_scene_t *sc)
{
	put(sc, backdrop("SBK · Backdrop aurora", "aurora", "\"color2\":4283584542,\"drift\":4.0,\"reach\":0.55"), 0, 0, TL);
	put(sc, comp("sbk_visualizer", "SBK · Viz line",
		     "{\"style\":\"line\",\"height\":300,\"bars\":72,\"fill_alpha\":0.22}"),
	    0, 1080, BL);
	put(sc, comp("sbk_card", "SBK · Welcome card",
		     card_json("Live now", "Welcome in", "Say hello in the chat while everyone arrives.", "",
			       "none", "centre", 1400)),
	    MIDX, 360, TC);
	put(sc, comp("sbk_chip", "SBK · Handle chip",
		     "{\"label\":\"@imswarnil\",\"dot\":\"live\",\"variant\":\"pill\",\"enter\":\"pop\"}"),
	    MIDX, 640, TC);
	put(sc, light("bar"), 0, EDGE, TL);
}

static void scene_lesson(obs_scene_t *sc)
{
	put(sc, backdrop("SBK · Backdrop gradient", "gradient", "\"color2\":4281545523,\"angle\":\"diagonal\""), 0, 0, TL);
	put(sc, comp("sbk_card", "SBK · Lesson card",
		     card_json("Module 3", "Auth and sessions", "What a session really is, and the four ways to lose one.",
			       "", "none", "centre", 1500)),
	    MIDX, 340, TC);
	put(sc, comp("sbk_progress", "SBK · Module progress",
		     "{\"label\":\"Modules\",\"value\":3,\"target\":8,\"style\":\"segments\",\"segments\":8,"
		     "\"width\":720,\"step\":1,\"variant\":\"none\"}"),
	    MIDX, 700, TC);
	put(sc, light("dot"), 1920 - EDGE, EDGE, TR);
}

/* ---- teaching -------------------------------------------------------------- */

static void scene_screen_share(obs_scene_t *sc)
{
	put_plate(sc, comp("sbk_plate", "SBK · Cam plate small",
		     "{\"aspect\":\"16x9\",\"size\":0.66,\"radius\":16.0,\"shadow_y\":18.0,\"shadow_blur\":48.0,\"fill_glass\":true}"),
	    1920 - EDGE, 1080 - EDGE, 422, 238, BR);
	put_box(sc, camera(), 1920 - EDGE, 1080 - EDGE, 420, 236, BR);
	put(sc, comp("sbk_frame", "SBK · Cam frame small",
		     "{\"aspect\":\"16x9\",\"size\":0.66,\"style\":\"ring\",\"radius\":16.0,"
		     "\"label\":\"@imswarnil\",\"chip_at\":\"bottom-left\"}"),
	    1920 - EDGE, 1080 - EDGE, BR);
	put(sc, comp("sbk_chip", "SBK · Topic chip",
		     "{\"label\":\"Chapter 1 — setting up\",\"variant\":\"card\",\"dot\":\"accent\"}"),
	    EDGE, EDGE, TL);
	put(sc, comp("sbk_progress", "SBK · Chapter progress",
		     "{\"label\":\"Chapters\",\"value\":1,\"target\":8,\"style\":\"segments\",\"segments\":8,"
		     "\"width\":420,\"step\":1}"),
	    EDGE, 1080 - EDGE, BL);
	put(sc, comp("sbk_meter", "SBK · Mic meter",
		     "{\"source\":\"@mic\",\"label\":\"Mic\",\"width\":300,\"style\":\"segments\","
		     "\"segment_count\":20,\"show_db\":false}"),
	    EDGE, 1080 - EDGE - 130, BL);
	put(sc, comp("sbk_prompt", "SBK · Prompt left",
		     "{\"edge\":\"left\",\"every\":240.0,\"hold\":7.0,\"width\":460,\"variant\":\"card\"}"),
	    EDGE, 300, TL);
	put(sc, light("badge"), 1920 - EDGE, EDGE, TR);
}

/* Two people over a shared screen: the pair that a course review or a
   walkthrough actually needs. Both cameras sit on the same edge so the screen
   keeps the whole middle of the frame. */
static void scene_pair_share(obs_scene_t *sc)
{
	put_plate(sc, comp("sbk_plate", "SBK · Pair plate host",
		     "{\"aspect\":\"16x9\",\"size\":0.52,\"radius\":14.0,\"shadow_y\":14.0,\"shadow_blur\":40.0,\"fill_glass\":true}"),
	    1920 - EDGE, EDGE, 333, 187, TR);
	put_box(sc, camera(), 1920 - EDGE, EDGE, 333, 187, TR);
	put(sc, comp("sbk_frame", "SBK · Pair frame host",
		     "{\"aspect\":\"16x9\",\"size\":0.52,\"style\":\"ring\",\"radius\":14.0,\"label\":\"Host\"}"),
	    1920 - EDGE, EDGE, TR);
	put_plate(sc, comp("sbk_plate", "SBK · Pair plate guest",
		     "{\"aspect\":\"16x9\",\"size\":0.52,\"radius\":14.0,\"shadow_y\":14.0,\"shadow_blur\":40.0,\"fill_glass\":true}"),
	    1920 - EDGE, EDGE + 230, 333, 187, TR);
	put(sc, comp("sbk_frame", "SBK · Pair frame guest",
		     "{\"aspect\":\"16x9\",\"size\":0.52,\"style\":\"ring\",\"radius\":14.0,\"label\":\"Guest\","
		     "\"line\":\"accent\"}"),
	    1920 - EDGE, EDGE + 230, TR);
	put(sc, comp("sbk_chip", "SBK · Topic chip",
		     "{\"label\":\"Chapter 1 — setting up\",\"variant\":\"card\",\"dot\":\"accent\"}"),
	    EDGE, EDGE, TL);
	put(sc, comp("sbk_meter", "SBK · Host meter",
		     "{\"source\":\"@mic\",\"label\":\"Host\",\"width\":333,\"style\":\"segments\","
		     "\"segment_count\":18,\"show_db\":false}"),
	    1920 - EDGE, EDGE + 460, TR);
	put(sc, comp("sbk_meter", "SBK · Guest meter",
		     "{\"source\":\"@mic2\",\"label\":\"Guest\",\"width\":333,\"style\":\"segments\","
		     "\"segment_count\":18,\"show_db\":false}"),
	    1920 - EDGE, EDGE + 570, TR);
	put(sc, comp("sbk_ticker", "SBK · Ticker", "{\"width\":1920}"), 0, 1080, BL);
}

/*  Two up: the screen and you, in two different shapes.

    The screen keeps a wide 21:9 crop of the canvas so code has room for long
    lines, and the camera is a 9:16 column beside it — the shape a phone shoots
    and the shape a face actually fills. Two aspect ratios in one frame is the
    point: matching them wastes half the picture on a desk.  */
static void scene_two_up(obs_scene_t *sc)
{
	put(sc, backdrop("SBK · Backdrop grid", "grid", "\"drift\":3.0,\"pitch\":72.0"), 0, 0, TL);

	put_plate(sc, comp("sbk_plate", "SBK · Two up screen plate",
			   "{\"aspect\":\"21x9\",\"size\":1.40,\"radius\":18.0,\"shadow_y\":18.0,"
			   "\"shadow_blur\":56.0,\"fill_glass\":true}"),
		  EDGE, 260, 1058, 454, TL);
	put_box(sc, screen_capture(), EDGE, 260, 1058, 454, TL);
	put(sc, comp("sbk_frame", "SBK · Two up screen frame",
		     "{\"aspect\":\"21x9\",\"size\":1.40,\"style\":\"ring\",\"radius\":18.0,"
		     "\"label\":\"Screen\",\"chip_at\":\"top-left\"}"),
	    EDGE, 260, TL);

	put_plate(sc, comp("sbk_plate", "SBK · Two up cam plate",
			   "{\"aspect\":\"9x16\",\"size\":0.79,\"radius\":18.0,\"shadow_y\":18.0,"
			   "\"shadow_blur\":56.0,\"fill_glass\":true}"),
		  1920 - EDGE, 260, 284, 506, TR);
	put_box(sc, camera(), 1920 - EDGE, 260, 284, 506, TR);
	put(sc, comp("sbk_frame", "SBK · Two up cam frame",
		     "{\"aspect\":\"9x16\",\"size\":0.79,\"style\":\"ring\",\"radius\":18.0,"
		     "\"label\":\"@imswarnil\",\"chip_at\":\"bottom-left\"}"),
	    1920 - EDGE, 260, TR);

	put(sc, comp("sbk_logo", "SBK · Bug",
		     "{\"mark\":\"ring\",\"loop\":\"orbit\",\"mark_size\":52,\"speed\":3.6,\"variant\":\"none\"}"),
	    EDGE, EDGE - 20, TL);
	put(sc, comp("sbk_chip", "SBK · Topic chip",
		     "{\"label\":\"Chapter 1 — setting up\",\"variant\":\"card\",\"dot\":\"accent\"}"),
	    EDGE + 130, EDGE, TL);
	put(sc, comp("sbk_social", "SBK · Social bar",
		     "{\"mode\":\"bar\",\"variant\":\"card\",\"brand\":true}"),
	    MIDX, 1080 - EDGE, OBS_ALIGN_BOTTOM | OBS_ALIGN_CENTER);
	put(sc, light("badge"), 1920 - EDGE, EDGE, TR);
}

/*  Three up: the screen and two people.

    A 16:9 screen across the top, and under it a 1:1 and a 4:5 — square for the
    host because a square crops a talking head without cutting the shoulders,
    and 4:5 for the guest because it is the shape every remote call hands you.
    Each has a meter under it, so a silent guest is visible at a glance.  */
static void scene_three_up(obs_scene_t *sc)
{
	put(sc, backdrop("SBK · Backdrop dots", "dots", "\"drift\":2.0,\"pitch\":48.0"), 0, 0, TL);

	put_plate(sc, comp("sbk_plate", "SBK · Three up screen plate",
			   "{\"aspect\":\"16x9\",\"size\":1.80,\"radius\":18.0,\"shadow_y\":18.0,"
			   "\"shadow_blur\":56.0,\"fill_glass\":true}"),
		  MIDX, 150, 1152, 648, TC);
	put_box(sc, screen_capture(), MIDX, 150, 1152, 648, TC);
	put(sc, comp("sbk_frame", "SBK · Three up screen frame",
		     "{\"aspect\":\"16x9\",\"size\":1.80,\"style\":\"ring\",\"radius\":18.0,\"label\":\"\"}"),
	    MIDX, 150, TC);

	put_plate(sc, comp("sbk_plate", "SBK · Three up host plate",
			   "{\"aspect\":\"1x1\",\"size\":0.42,\"radius\":16.0,\"shadow_y\":14.0,"
			   "\"shadow_blur\":44.0,\"fill_glass\":true}"),
		  EDGE, 280, 202, 202, TL);
	put_box(sc, camera(), EDGE, 280, 202, 202, TL);
	put(sc, comp("sbk_frame", "SBK · Three up host frame",
		     "{\"aspect\":\"1x1\",\"size\":0.42,\"style\":\"ring\",\"radius\":16.0,\"label\":\"Host\"}"),
	    EDGE, 280, TL);
	put(sc, comp("sbk_meter", "SBK · Host meter",
		     "{\"source\":\"@mic\",\"label\":\"Host\",\"width\":162,\"style\":\"segments\","
		     "\"segment_count\":14,\"show_db\":false}"),
	    EDGE, 512, TL);

	put_plate(sc, comp("sbk_plate", "SBK · Three up guest plate",
			   "{\"aspect\":\"4x5\",\"size\":0.44,\"radius\":16.0,\"shadow_y\":14.0,"
			   "\"shadow_blur\":44.0,\"fill_glass\":true}"),
		  1920 - EDGE, 280, 190, 238, TR);
	put(sc, comp("sbk_frame", "SBK · Three up guest frame",
		     "{\"aspect\":\"4x5\",\"size\":0.44,\"style\":\"ring\",\"radius\":16.0,"
		     "\"label\":\"Guest\",\"line\":\"accent\"}"),
	    1920 - EDGE, 280, TR);
	put(sc, comp("sbk_meter", "SBK · Guest meter",
		     "{\"source\":\"@mic2\",\"label\":\"Guest\",\"width\":150,\"style\":\"segments\","
		     "\"segment_count\":14,\"show_db\":false}"),
	    1920 - EDGE, 548, TR);

	put(sc, comp("sbk_lower_third", "SBK · Three up title",
		     "{\"variant\":\"minimal\",\"bar\":true,\"name\":\"Reviewing the build\","
		     "\"title\":\"Live, with questions at the end\"}"),
	    EDGE, 880, TL);
	put(sc, light("dot"), 1920 - EDGE, EDGE, TR);
}

/* The screen keeps the left; the questions stack down the right. This is the
   one to cut to when the chat has got ahead of you. */
static void scene_comments(obs_scene_t *sc)
{
	put(sc, comp("sbk_comments", "SBK · Questions",
		     "{\"width\":560,\"title\":\"Questions\",\"show_count\":3,\"rotate\":12.0,"
		     "\"variant\":\"card\",\"enter\":\"left\"}"),
	    1920 - EDGE, EDGE + 120, TR);
	put(sc, comp("sbk_chip", "SBK · QA chip",
		     "{\"label\":\"Questions\",\"variant\":\"accent\",\"dot\":\"none\"}"),
	    1920 - EDGE, EDGE, TR);
	put_plate(sc, comp("sbk_plate", "SBK · Cam plate corner",
		     "{\"aspect\":\"16x9\",\"size\":0.52,\"radius\":14.0,\"shadow_y\":14.0,\"shadow_blur\":40.0,\"fill_glass\":true}"),
	    EDGE, 1080 - EDGE, 333, 187, BL);
	put_box(sc, camera(), EDGE, 1080 - EDGE, 333, 187, BL);
	put(sc, comp("sbk_frame", "SBK · Cam frame corner",
		     "{\"aspect\":\"16x9\",\"size\":0.52,\"style\":\"ring\",\"radius\":14.0,\"label\":\"\"}"),
	    EDGE, 1080 - EDGE, BL);
	put(sc, light("badge"), EDGE, EDGE, TL);
}

static void scene_talking_head(obs_scene_t *sc)
{
	put_box(sc, camera(), MIDX, EDGE, 1664, 936, TC);
	put(sc, comp("sbk_frame", "SBK · Cam frame full",
		     "{\"aspect\":\"16x9\",\"size\":2.6,\"style\":\"corner\",\"bracket\":96.0,\"label\":\"\","
		     "\"line\":\"accent\",\"weight\":4.0}"),
	    MIDX, EDGE, TC);
	put(sc, comp("sbk_logo", "SBK · Bug",
		     "{\"mark\":\"ring\",\"loop\":\"orbit\",\"mark_size\":52,\"speed\":3.6,\"variant\":\"none\"}"),
	    EDGE, EDGE - 20, TL);
	put(sc, comp("sbk_lower_third", "SBK · Lower third minimal",
		     "{\"variant\":\"minimal\",\"bar\":true}"),
	    EDGE + 40, 980, BL);
	put(sc, comp("sbk_chip", "SBK · Topic chip",
		     "{\"label\":\"Chapter 1 — setting up\",\"variant\":\"card\",\"dot\":\"accent\"}"),
	    1920 - EDGE - 40, 980, BR);
	put(sc, light("dot"), 1920 - EDGE, EDGE, TR);
}

static void scene_interview(obs_scene_t *sc)
{
	put_plate(sc, comp("sbk_plate", "SBK · Plate left",
		     "{\"aspect\":\"16x9\",\"size\":1.31,\"radius\":18.0,\"shadow_y\":20.0,\"shadow_blur\":56.0,\"fill_glass\":true}"),
	    EDGE, 300, 838, 472, TL);
	put_box(sc, camera(), EDGE, 300, 838, 471, TL);
	put(sc, comp("sbk_frame", "SBK · Cam frame left",
		     "{\"aspect\":\"16x9\",\"size\":1.31,\"style\":\"ring\",\"label\":\"\"}"),
	    EDGE, 300, TL);
	put_plate(sc, comp("sbk_plate", "SBK · Plate right",
		     "{\"aspect\":\"16x9\",\"size\":1.31,\"radius\":18.0,\"shadow_y\":20.0,\"shadow_blur\":56.0,\"fill_glass\":true}"),
	    1920 - EDGE, 300, 838, 472, TR);
	put(sc, comp("sbk_frame", "SBK · Cam frame right",
		     "{\"aspect\":\"16x9\",\"size\":1.31,\"style\":\"ring\",\"label\":\"\"}"),
	    1920 - EDGE, 300, TR);
	put(sc, comp("sbk_lower_third", "SBK · Name left",
		     "{\"variant\":\"split\",\"name\":\"Swarnil Singhai\",\"title\":\"Host\"}"),
	    EDGE, 840, TL);
	put(sc, comp("sbk_lower_third", "SBK · Name right",
		     "{\"variant\":\"split\",\"name\":\"Your guest\",\"title\":\"Guest\"}"),
	    1920 - EDGE - 840, 840, TL);
	put(sc, comp("sbk_ticker", "SBK · Ticker", "{\"width\":1920}"), 0, 1080, BL);
	put(sc, light(""), 1920 - EDGE, EDGE, TR);
}

static void scene_live(obs_scene_t *sc)
{
	put_plate(sc, comp("sbk_plate", "SBK · Cam plate",
		     "{\"aspect\":\"16x9\",\"size\":1.0,\"radius\":20.0,\"shadow_y\":18.0,\"shadow_blur\":52.0,\"fill_glass\":true}"),
	    1920 - EDGE, 560, 640, 360, TR);
	put_box(sc, camera(), 1920 - EDGE, 560, 640, 360, TR);
	put(sc, comp("sbk_frame", "SBK · Cam frame",
		     "{\"aspect\":\"16x9\",\"size\":1.0,\"style\":\"ring\",\"label\":\"@imswarnil\"}"),
	    1920 - EDGE, 560, TR);
	put(sc, comp("sbk_ticker", "SBK · Ticker", "{\"width\":1920}"), 0, 1080, BL);
	put(sc, comp("sbk_lower_third", "SBK · Lower third", NULL), EDGE, 900, BL);
	put(sc, comp("sbk_prompt", "SBK · Prompt",
		     "{\"edge\":\"right\",\"every\":180.0,\"hold\":8.0,\"width\":520}"),
	    1920 - EDGE, EDGE + 90, TR);
	put(sc, light(""), 1920 - EDGE, EDGE, TR);
}

static void scene_gameplay(obs_scene_t *sc)
{
	put_plate(sc, comp("sbk_plate", "SBK · Cam plate corner tiny",
		     "{\"aspect\":\"16x9\",\"size\":0.56,\"radius\":12.0,\"shadow_y\":12.0,\"shadow_blur\":34.0,\"fill_glass\":true}"),
	    1920 - EDGE, EDGE, 358, 202, TR);
	put_box(sc, camera(), 1920 - EDGE, EDGE, 360, 203, TR);
	put(sc, comp("sbk_frame", "SBK · Cam frame corner tiny",
		     "{\"aspect\":\"16x9\",\"size\":0.56,\"style\":\"corner-out\",\"bracket\":40.0,"
		     "\"line\":\"accent\",\"label\":\"\"}"),
	    1920 - EDGE, EDGE, TR);
	put(sc, comp("sbk_chip", "SBK · Now playing chip",
		     "{\"label\":\"Now playing\",\"value\":\"Act 2\",\"variant\":\"card\",\"dot\":\"pulse\"}"),
	    EDGE, EDGE, TL);
	put(sc, comp("sbk_progress", "SBK · Run progress",
		     "{\"label\":\"Run\",\"value\":3,\"target\":10,\"style\":\"segments\","
		     "\"segments\":10,\"width\":420,\"step\":1}"),
	    EDGE, 1080 - EDGE, BL);
	put(sc, comp("sbk_ticker", "SBK · Ticker", "{\"width\":1920}"), 0, 1080, BL);
}

static void scene_music(obs_scene_t *sc)
{
	put(sc, backdrop("SBK · Backdrop checkers", "checkers", "\"color2\":4280229663,\"pitch\":160.0,\"drift\":5.0"), 0, 0, TL);
	put(sc, comp("sbk_visualizer", "SBK · Viz ring",
		     "{\"style\":\"ring\",\"width\":720,\"height\":720,\"bars\":72,\"inner\":0.52}"),
	    MIDX, 120, TC);
	put(sc, comp("sbk_chip", "SBK · Track chip",
		     "{\"label\":\"Now playing\",\"value\":\"—\",\"variant\":\"pill\",\"dot\":\"pulse\"}"),
	    MIDX, 880, TC);
	put(sc, comp("sbk_clock", "SBK · Clock", NULL), 1920 - EDGE, EDGE, TR);
	put(sc, light(""), EDGE, EDGE, TL);
}

static void scene_highlight(obs_scene_t *sc)
{
	put(sc, backdrop("SBK · Backdrop stars", "stars", "\"pitch\":90.0,\"weight\":2.5"), 0, 0, TL);
	put(sc, comp("sbk_card", "SBK · Highlight card",
		     card_json("", "The best overlay is the one nobody notices.", "", "", "none", "centre", 1500)),
	    MIDX, 380, TC);
	put(sc, comp("sbk_chip", "SBK · Highlight chip",
		     "{\"label\":\"@imswarnil\",\"variant\":\"outline\",\"dot\":\"none\",\"enter\":\"pop\"}"),
	    MIDX, 700, TC);
	put(sc, light("dot"), 1920 - EDGE, EDGE, TR);
}

/* ---- breaks ---------------------------------------------------------------- */

static void scene_intermission(obs_scene_t *sc)
{
	put(sc, backdrop("SBK · Backdrop aurora", "aurora", ""), 0, 0, TL);
	put(sc, comp("sbk_countdown", "SBK · Intermission ring",
		     "{\"mode\":\"duration\",\"minutes\":4,\"style\":\"ring\",\"ring_size\":420,"
		     "\"label\":\"Back in\",\"draw_card\":false,\"enter\":\"grow\"}"),
	    MIDX, 300, TC);
	put(sc, comp("sbk_chip", "SBK · Intermission chip",
		     "{\"label\":\"Stay there\",\"variant\":\"outline\",\"dot\":\"pulse\"}"),
	    MIDX, 820, TC);
	put(sc, light(""), 1920 - EDGE, EDGE, TR);
}

static void scene_brb(obs_scene_t *sc)
{
	put(sc, backdrop("SBK · Backdrop dots", "dots", "\"drift\":-4.0,\"pitch\":48.0"), 0, 0, TL);
	put(sc, comp("sbk_visualizer", "SBK · Viz wave", "{\"style\":\"wave\",\"height\":240}"), 0, 1080, BL);
	put(sc, light(""), EDGE, EDGE, TL);
	put(sc, comp("sbk_clock", "SBK · Clock", NULL), 1920 - EDGE, EDGE, TR);
	put(sc, comp("sbk_card", "SBK · Break card",
		     card_json("Paused", "Be right back", "Two minutes. Stretch your legs.",
			       "@imswarnil | imswarnil.com", "card", "left", 1000)),
	    EDGE, 340, TL);
	put(sc, comp("sbk_countdown", "SBK · Break countdown",
		     "{\"mode\":\"duration\",\"minutes\":5,\"style\":\"bar\",\"draw_card\":true,\"label\":\"Back in\"}"),
	    1920 - EDGE, 340, TR);
}

static void scene_podcast(obs_scene_t *sc)
{
	put(sc, backdrop("SBK · Backdrop plasma", "plasma", "\"color2\":4281545523,\"drift\":3.0,\"pitch\":140.0"), 0, 0, TL);
	put(sc, comp("sbk_card", "SBK · Podcast card",
		     card_json("Episode", "Building things in public", "", "", "none", "centre", 1300)),
	    MIDX, 220, TC);
	put(sc, comp("sbk_meter", "SBK · Host meter",
		     "{\"source\":\"@mic\",\"label\":\"Host\",\"width\":700,\"style\":\"segments\","
		     "\"segment_count\":28}"),
	    EDGE, 560, TL);
	put(sc, comp("sbk_meter", "SBK · Guest meter",
		     "{\"source\":\"@mic2\",\"label\":\"Guest\",\"width\":700,\"style\":\"segments\","
		     "\"segment_count\":28}"),
	    1920 - EDGE, 560, TR);
	put(sc, comp("sbk_countdown", "SBK · Episode clock",
		     "{\"mode\":\"up\",\"minutes\":45,\"style\":\"bar\",\"label\":\"Running time\","
		     "\"draw_card\":true}"),
	    MIDX, 800, TC);
	put(sc, light(""), 1920 - EDGE, EDGE, TR);
}

static void scene_trouble(obs_scene_t *sc)
{
	put(sc, backdrop("SBK · Backdrop vignette", "vignette", "\"reach\":0.75"), 0, 0, TL);
	put(sc, comp("sbk_card", "SBK · Trouble card",
		     card_json("Stand by", "Technical difficulties", "Back in a moment. Do not adjust your set.", "",
			       "accent", "centre", 1200)),
	    MIDX, 420, TC);
	put(sc, light("edge"), 0, 0, TL);
}

/* ---- closing --------------------------------------------------------------- */

static void scene_support(obs_scene_t *sc)
{
	put(sc, backdrop("SBK · Backdrop rings", "rings", "\"drift\":2.0,\"pitch\":120.0"), 0, 0, TL);
	put(sc, comp("sbk_card", "SBK · Support card",
		     card_json("Support the channel", "Become a member",
			       "Members get the source for everything built on this stream.", "",
			       "none", "left", 900)),
	    EDGE, 240, TL);
	put(sc, comp("sbk_qr", "SBK · Membership QR",
		     "{\"text\":\"https://imswarnil.com/#/portal/signup\",\"caption\":\"Scan to join\","
		     "\"sub\":\"imswarnil.com\",\"code_size\":300,\"variant\":\"card\",\"enter\":\"pop\"}"),
	    1920 - EDGE, 240, TR);
	put(sc, comp("sbk_counter", "SBK · Members",
		     "{\"provider\":\"ghost\",\"label\":\"Members\",\"site\":\"https://imswarnil.com\","
		     "\"show_goal\":true,\"goal\":500,\"width\":420}"),
	    EDGE, 620, TL);
	put(sc, comp("sbk_counter", "SBK · Subscribers",
		     "{\"provider\":\"youtube\",\"label\":\"Subscribers\",\"width\":420}"),
	    EDGE + 460, 620, TL);
	put(sc, comp("sbk_ticker", "SBK · Ticker", "{\"width\":1920}"), 0, 1080, BL);
	put(sc, light(""), 1920 - EDGE, EDGE, TR);
}

static void scene_ending(obs_scene_t *sc)
{
	put(sc, backdrop("SBK · Backdrop stripes", "stripes", "\"drift\":10.0,\"pitch\":96.0"), 0, 0, TL);
	put(sc, comp("sbk_visualizer", "SBK · Viz dots", "{\"style\":\"dots\",\"height\":240}"), 0, 1080, BL);
	put(sc, comp("sbk_card", "SBK · Ending card",
		     card_json("That is a wrap", "Thanks for watching", "Subscribe for the next one.", "", "none",
			       "centre", 1400)),
	    MIDX, 320, TC);
	put(sc, comp("sbk_progress", "SBK · Subscriber goal",
		     "{\"label\":\"Subscriber goal\",\"value\":640,\"target\":1000,\"width\":720}"),
	    MIDX, 620, TC);
	put(sc, comp("sbk_chip", "SBK · Subscribe chip",
		     "{\"label\":\"youtube.com/@imswarnil\",\"variant\":\"accent\",\"dot\":\"none\"}"),
	    MIDX, 760, TC);
	put(sc, comp("sbk_social", "SBK · Social bar",
		     "{\"mode\":\"bar\",\"variant\":\"card\",\"brand\":true}"),
	    MIDX, 900, TC);
	put(sc, comp("sbk_logo", "SBK · Sign off mark",
		     "{\"mark\":\"ring\",\"loop\":\"draw\",\"mark_size\":120,\"speed\":4.0,"
		     "\"variant\":\"none\",\"caption\":\"See you Thursday\"}"),
	    EDGE, 1080 - EDGE, BL);
	put(sc, comp("sbk_qr", "SBK · Channel QR",
		     "{\"text\":\"https://youtube.com/@imswarnil\",\"caption\":\"Scan to subscribe\","
		     "\"sub\":\"\",\"code_size\":240,\"variant\":\"none\",\"invert\":true}"),
	    1920 - EDGE, 1080 - EDGE, BR);
	put(sc, light(""), EDGE, EDGE, TL);
}

static void scene_vertical(obs_scene_t *sc)
{
	put(sc, backdrop("SBK · Backdrop hex", "hex", "\"drift\":3.0,\"pitch\":60.0"), 0, 0, TL);
	put_plate(sc, comp("sbk_plate", "SBK · Cam plate vertical",
		     "{\"aspect\":\"9x16\",\"size\":1.22,\"radius\":24.0,\"shadow_y\":20.0,\"shadow_blur\":60.0,\"fill_glass\":true}"),
	    MIDX, 1080 - 30, 439, 781, OBS_ALIGN_BOTTOM | OBS_ALIGN_CENTER);
	put_box(sc, camera(), MIDX, 1080 - 30, 439, 781, OBS_ALIGN_BOTTOM | OBS_ALIGN_CENTER);
	put(sc, comp("sbk_frame", "SBK · Cam frame vertical",
		     "{\"aspect\":\"9x16\",\"size\":1.22,\"style\":\"ring\",\"radius\":24.0,"
		     "\"label\":\"@imswarnil\",\"chip_at\":\"bottom-left\"}"),
	    MIDX, 1080 - 30, OBS_ALIGN_BOTTOM | OBS_ALIGN_CENTER);
	put(sc, comp("sbk_card", "SBK · Vertical card",
		     card_json("Short", "One idea, sixty seconds", "", "", "none", "centre", 760)),
	    MIDX, 56, TC);
	put(sc, light("dot"), 1920 - EDGE, EDGE, TR);
}

/* ---- not for the stream ----------------------------------------------------- */

static void scene_desk(obs_scene_t *sc)
{
	put(sc, backdrop("SBK · Backdrop desk", "solid", ""), 0, 0, TL);
	put(sc, comp("sbk_stats", "SBK · Stats", "{\"width\":760,\"show_fps\":true,\"variant\":\"card\"}"),
	    EDGE, 200, TL);
	put(sc, comp("sbk_meter", "SBK · Program meter",
		     "{\"source\":\"@program\",\"label\":\"Program\",\"width\":760}"),
	    EDGE, 460, TL);
	put(sc, comp("sbk_meter", "SBK · Mic meter desk",
		     "{\"source\":\"@mic\",\"label\":\"Mic\",\"width\":760}"),
	    EDGE, 600, TL);
	put(sc, comp("sbk_comments", "SBK · Desk comments",
		     "{\"width\":700,\"title\":\"Questions\",\"show_count\":4,\"variant\":\"card\"}"),
	    1920 - EDGE, 460, TR);
	put(sc, comp("sbk_countdown", "SBK · Uptime",
		     "{\"mode\":\"uptime\",\"style\":\"digits\",\"label\":\"On air\",\"draw_card\":true}"),
	    1920 - EDGE, 200, TR);
	put(sc, light(""), 1920 - EDGE, EDGE, TR);
}

/* The running order, top of the show to the bottom. */
static const struct scene_spec SHOW[] = {
	{"SBK · Starting soon", scene_starting},
	{"SBK · Welcome", scene_welcome},
	{"SBK · Lesson", scene_lesson},
	{"SBK · Screen share", scene_screen_share},
	{"SBK · Screen share + two", scene_pair_share},
	{"SBK · Two up", scene_two_up},
	{"SBK · Three up", scene_three_up},
	{"SBK · Comments", scene_comments},
	{"SBK · Talking head", scene_talking_head},
	{"SBK · Interview", scene_interview},
	{"SBK · Live", scene_live},
	{"SBK · Gameplay", scene_gameplay},
	{"SBK · Music", scene_music},
	{"SBK · Highlight", scene_highlight},
	{"SBK · Intermission", scene_intermission},
	{"SBK · Be right back", scene_brb},
	{"SBK · Podcast", scene_podcast},
	{"SBK · Technical difficulties", scene_trouble},
	{"SBK · Support", scene_support},
	{"SBK · Ending", scene_ending},
	{"SBK · Vertical", scene_vertical},
	{"SBK · Desk (private)", scene_desk},
};
#define N_SHOW (sizeof(SHOW) / sizeof(SHOW[0]))

/*  A scene the show no longer has.

    Scenes are reused rather than recreated, which is what keeps anything you
    dragged where you like it. The cost is that renaming or dropping a scene
    would leave the old one behind, empty, for ever. This removes any SBK scene
    that is not in the running order — and only those, so a scene you made
    yourself is never touched.  */
static void prune_retired_scenes(void)
{
	struct obs_frontend_source_list list = {0};
	obs_frontend_get_scenes(&list);
	for (size_t i = 0; i < list.sources.num; i++) {
		obs_source_t *src = list.sources.array[i];
		const char *name = obs_source_get_name(src);
		if (!name || strncmp(name, "SBK · ", strlen("SBK · ")) != 0)
			continue;
		bool keep = false;
		for (size_t j = 0; j < N_SHOW && !keep; j++)
			keep = strcmp(name, SHOW[j].name) == 0;
		if (keep)
			continue;
		SBK_LOG(LOG_INFO, "retired scene removed: %s", name);
		obs_source_remove(src);
	}
	obs_frontend_source_list_free(&list);
}

int sbk_build_scenes(void)
{
	measure();
	ensure_mic();

	/* built back to front: OBS puts the most recently added scene at the top
	   of the Scenes panel, so this is what makes the panel read in show order */
	for (size_t i = N_SHOW; i-- > 0;) {
		obs_scene_t *sc = fresh_scene(SHOW[i].name);
		if (!sc)
			continue;
		SHOW[i].build(sc);
		obs_scene_release(sc);
	}

	prune_retired_scenes();
	obs_frontend_set_transition_duration(400);
	obs_source_t *first = obs_get_source_by_name(SHOW[0].name);
	if (first) {
		obs_frontend_set_current_scene(first);
		obs_source_release(first);
	}
	release_held();
	SBK_LOG(LOG_INFO, "%zu scenes built", N_SHOW);
	return (int)N_SHOW;
}

/* The frontend API can list transitions and choose one, but it has no call to
   add one — only the Scene Transitions panel's "+" can do that, which is the
   same for every transition plugin there has ever been. So: if the user has
   already added SBK Wipe, select it; otherwise say once how to. */
static void use_wipe(void)
{
	struct obs_frontend_source_list list = {0};
	obs_frontend_get_transitions(&list);
	obs_source_t *found = NULL;
	for (size_t i = 0; i < list.sources.num; i++) {
		const char *id = obs_source_get_id(list.sources.array[i]);
		if (id && strcmp(id, "sbk_wipe") == 0) {
			found = list.sources.array[i];
			break;
		}
	}
	if (found) {
		obs_frontend_set_current_transition(found);
		SBK_LOG(LOG_INFO, "using the %s transition", obs_source_get_name(found));
	} else {
		SBK_LOG(LOG_INFO, "for the accent sweep between these scenes: Scene Transitions → + → %s", TRANSITION);
	}
	obs_frontend_source_list_free(&list);
}

void sbk_create_collection(void)
{
	char *cur = obs_frontend_get_current_scene_collection();
	bool already = cur && strcmp(cur, COLLECTION) == 0;
	bfree(cur);
	if (!already) {
		/* add creates AND switches; false means it exists, so switch */
		if (!obs_frontend_add_scene_collection(COLLECTION))
			obs_frontend_set_current_scene_collection(COLLECTION);
	}
	sbk_build_scenes();
	use_wipe();

	/* the empty scene a fresh collection is born with */
	obs_source_t *def = obs_get_source_by_name("Scene");
	if (def) {
		obs_source_remove(def);
		obs_source_release(def);
	}
	obs_frontend_save();
}

void sbk_add_live_pack(void)
{
	measure();
	obs_source_t *scene_src = obs_frontend_get_current_scene();
	obs_scene_t *sc = obs_scene_from_source(scene_src);
	if (!sc) {
		obs_source_release(scene_src);
		return;
	}
	const char *scene_name = obs_source_get_name(scene_src);
	struct dstr name = {0};
#define NAMED(what) (dstr_printf(&name, "SBK · %s (%s)", what, scene_name), name.array)
	put(sc, comp("sbk_ticker", NAMED("Ticker"), "{\"width\":1920}"), 0, 1080, BL);
	put(sc, comp("sbk_lower_third", NAMED("Lower third"), NULL), EDGE, 900, BL);
	put(sc, comp("sbk_frame", NAMED("Cam frame"), NULL), 1920 - EDGE, 560, TR);
	put(sc, comp("sbk_onair", NAMED("Light"), NULL), 1920 - EDGE, EDGE, TR);
#undef NAMED
	dstr_free(&name);
	release_held();
	obs_source_release(scene_src);
	SBK_LOG(LOG_INFO, "live pack added to '%s'", scene_name);
}

bool sbk_use_profile(void)
{
	char **profiles = obs_frontend_get_profiles();
	bool found = false;
	for (char **p = profiles; p && *p; p++)
		if (strcmp(*p, PROFILE) == 0)
			found = true;
	bfree(profiles);
	if (!found) {
		SBK_LOG(LOG_WARNING, "no profile called '%s' — run profile/install.command", PROFILE);
		return false;
	}
	char *cur = obs_frontend_get_current_profile();
	bool already = cur && strcmp(cur, PROFILE) == 0;
	bfree(cur);
	if (!already)
		obs_frontend_set_current_profile(PROFILE);
	return true;
}

/*  Builds the collection, then walks the scenes taking a program screenshot of
    each.

    The walk runs on its own thread and hands each step back to the UI thread
    with obs_queue_task. Sleeping on the UI thread instead — which is the
    obvious way to write this — looks like it works and is not: OBS renders and
    writes a queued screenshot on that same thread, so a blocked UI thread
    collapses twelve requests into the one that fires after the block ends.  */

struct walk_step {
	const char *scene;
	bool shoot;
};


static void step_switch(void *param)
{
	obs_source_t *s = obs_get_source_by_name((const char *)param);
	if (!s) {
		SBK_LOG(LOG_WARNING, "self-test: no scene called '%s'", (const char *)param);
		return;
	}
	obs_frontend_set_current_scene(s);
	obs_source_release(s);
	SBK_LOG(LOG_INFO, "self-test: on '%s'", (const char *)param);
}

static void step_shoot(void *param)
{
	UNUSED_PARAMETER(param);
	obs_frontend_take_screenshot();
}

/*  The walk cuts rather than fades.

    A 400 ms fade plus the sources' own arrivals meant a shot could still carry a
    ghost of the scene before it, and a published picture of one scene with
    another bleeding through is worse than no picture. Zero duration makes every
    switch a cut, so the frame is only ever one scene.  */
static int g_saved_duration;

static void step_cut(void *param)
{
	UNUSED_PARAMETER(param);
	g_saved_duration = obs_frontend_get_transition_duration();
	obs_frontend_set_transition_duration(0);
}

static void step_uncut(void *param)
{
	UNUSED_PARAMETER(param);
	obs_frontend_set_transition_duration(g_saved_duration > 0 ? g_saved_duration : 400);
}

/*  Hiding the camera for a screenshot run.

    The shots that end up on the website are taken by this walk, and the
    collection now puts a real camera in five of the scenes — so a run captures
    whatever the webcam is pointed at, and those images get published. The clean
    run hides every item whose source is the camera, walks, and puts them back.  */

#define MAX_HIDDEN 64
static obs_sceneitem_t *g_hidden[MAX_HIDDEN];
static int g_n_hidden;

static bool hide_camera_item(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	UNUSED_PARAMETER(scene);
	UNUSED_PARAMETER(param);
	obs_source_t *src = obs_sceneitem_get_source(item);
	const char *name = src ? obs_source_get_name(src) : NULL;
	/* the display capture is every bit as private as the camera — a published
	   screenshot must not carry whatever happens to be on the desktop */
	if (!name || (strcmp(name, CAM_NAME) != 0 && strcmp(name, SCREEN_NAME) != 0))
		return true;
	if (!obs_sceneitem_visible(item) || g_n_hidden >= MAX_HIDDEN)
		return true;
	obs_sceneitem_addref(item);
	g_hidden[g_n_hidden++] = item;
	obs_sceneitem_set_visible(item, false);
	return true;
}

static void hide_cameras(void)
{
	struct obs_frontend_source_list list = {0};
	obs_frontend_get_scenes(&list);
	for (size_t i = 0; i < list.sources.num; i++) {
		obs_scene_t *sc = obs_scene_from_source(list.sources.array[i]);
		if (sc)
			obs_scene_enum_items(sc, hide_camera_item, NULL);
	}
	obs_frontend_source_list_free(&list);
	if (g_n_hidden)
		SBK_LOG(LOG_INFO, "self-test: %d camera and screen item(s) hidden for the walk", g_n_hidden);
}

static void restore_cameras(void)
{
	for (int i = 0; i < g_n_hidden; i++) {
		obs_sceneitem_set_visible(g_hidden[i], true);
		obs_sceneitem_release(g_hidden[i]);
		g_hidden[i] = NULL;
	}
	g_n_hidden = 0;
}

static void step_hide(void *param)
{
	UNUSED_PARAMETER(param);
	hide_cameras();
}
static void step_restore(void *param)
{
	UNUSED_PARAMETER(param);
	restore_cameras();
}

static bool g_walk_clean;

static void *walk_thread(void *arg)
{
	UNUSED_PARAMETER(arg);
	os_set_thread_name("sbk-selftest");
	if (g_walk_clean)
		obs_queue_task(OBS_TASK_UI, step_hide, NULL, true);
	obs_queue_task(OBS_TASK_UI, step_cut, NULL, true);
	for (size_t i = 0; i < N_SHOW; i++) {
		obs_queue_task(OBS_TASK_UI, step_switch, (void *)SHOW[i].name, true);
		/* the cut is instant, but every source plays an arrival and the
		   pollers want a frame or two, and a prompt comes in at two and a
		   half — shoot once the scene has actually settled */
		os_sleep_ms(3200);
		obs_queue_task(OBS_TASK_UI, step_shoot, NULL, true);
		os_sleep_ms(600);
	}
	obs_queue_task(OBS_TASK_UI, step_uncut, NULL, true);
	if (g_walk_clean)
		obs_queue_task(OBS_TASK_UI, step_restore, NULL, true);
	SBK_LOG(LOG_INFO, "self-test: walked %zu scenes%s, screenshots are in the recording folder", N_SHOW,
		g_walk_clean ? " with the camera hidden" : "");
	return NULL;
}

void sbk_selftest(bool clean)
{
	g_walk_clean = clean;
	sbk_create_collection();
	pthread_t th;
	if (pthread_create(&th, NULL, walk_thread, NULL) == 0)
		pthread_detach(th);
	else
		SBK_LOG(LOG_WARNING, "self-test: could not start the walk");
}

/* The format ring's buffers live for the life of the module; OBS counts them as
   leaks at shutdown if they are not handed back. */
void sbk_scenes_free(void)
{
	for (size_t i = 0; i < sizeof(g_fmt_ring) / sizeof(g_fmt_ring[0]); i++)
		dstr_free(&g_fmt_ring[i]);
	release_held();
}

/* For a scene someone built by hand: the same camera, with the same rounded
   corners, and the microphone on the Mic/Aux channel. */
void sbk_add_devices(void)
{
	measure();
	ensure_mic();
	obs_source_t *scene_src = obs_frontend_get_current_scene();
	obs_scene_t *sc = obs_scene_from_source(scene_src);
	if (sc) {
		obs_source_t *cam = camera();
		if (cam)
			put_box(sc, cam, 1920 - EDGE, 1080 - EDGE, 480, 270, BR);
	}
	release_held();
	obs_source_release(scene_src);
}
