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

#define MAX_HELD 128
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
static obs_source_t *comp(const char *id, const char *name, const char *json)
{
	obs_source_t *existing = obs_get_source_by_name(name);
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
	return comp("sbk_backdrop", name,
		    fmt("{\"mode\":\"%s\",\"color\":%u,\"width\":%d,\"height\":%d%s%s}", mode, (unsigned)SBK_SOLID,
			(int)W, (int)H, *extra ? "," : "", extra));
}

/* ---- the show ------------------------------------------------------------ */

int sbk_build_scenes(void)
{
	measure();
	int n = 0;

	/* 1. Starting soon — the countdown screen people sit on */
	{
		obs_scene_t *sc = fresh_scene("SBK · Starting soon");
		put(sc, backdrop("SBK · Backdrop grid", "grid", "\"drift\":6.0,\"pitch\":72.0"), 0, 0, TL);
		put(sc, comp("sbk_visualizer", "SBK · Viz bars", "{\"style\":\"bars\",\"height\":240}"), 0, 1080, BL);
		put(sc, light(""), EDGE, EDGE, TL);
		put(sc, comp("sbk_clock", "SBK · Clock", NULL), 1920 - EDGE, EDGE, TR);
		put(sc, comp("sbk_card", "SBK · Starting soon card",
			     card_json("Starting soon", "Building a Salesforce app live",
				       "Grab a coffee. We begin at the top of the hour.",
				       "@imswarnil | youtube.com/@imswarnil | imswarnil.com", "card", "left", 1100)),
		    EDGE, 300, TL);
		put(sc, comp("sbk_countdown", "SBK · Countdown",
			     "{\"mode\":\"duration\",\"minutes\":15,\"draw_card\":true}"),
		    1920 - EDGE, 300, TR);
		obs_scene_release(sc);
		n++;
	}
	/* 2. Welcome — the first seconds of the stream, nothing to read */
	{
		obs_scene_t *sc = fresh_scene("SBK · Welcome");
		put(sc, backdrop("SBK · Backdrop gradient", "gradient", "\"color2\":4281545523,\"angle\":\"diagonal\""), 0, 0,
		    TL);
		put(sc, comp("sbk_visualizer", "SBK · Viz line",
			     "{\"style\":\"line\",\"height\":300,\"bars\":72,\"fill_alpha\":0.22}"),
		    0, 1080, BL);
		put(sc, comp("sbk_card", "SBK · Welcome card",
			     card_json("Live now", "Welcome in", "Say hello in the chat while everyone arrives.", "",
				       "none", "centre", 1400)),
		    MIDX, 360, TC);
		put(sc, comp("sbk_chip", "SBK · Handle chip",
			     "{\"label\":\"@imswarnil\",\"dot\":\"live\",\"variant\":\"pill\"}"),
		    MIDX, 640, TC);
		put(sc, light("bar"), 0, EDGE, TL);
		obs_scene_release(sc);
		n++;
	}
	/* 3. Live — the everyday scene: camera under the frame, lower third, ticker */
	{
		obs_scene_t *sc = fresh_scene("SBK · Live");
		put(sc, comp("sbk_ticker", "SBK · Ticker", "{\"width\":1920}"), 0, 1080, BL);
		put(sc, comp("sbk_lower_third", "SBK · Lower third", NULL), EDGE, 900, BL);
		put(sc, comp("sbk_frame", "SBK · Cam frame",
			     "{\"aspect\":\"16x9\",\"size\":1.0,\"style\":\"ring\",\"label\":\"@imswarnil\"}"),
		    1920 - EDGE, 560, TR);
		put(sc, light(""), 1920 - EDGE, EDGE, TR);
		obs_scene_release(sc);
		n++;
	}
	/* 4. Talking head — the camera is the whole picture, so the chrome shrinks */
	{
		obs_scene_t *sc = fresh_scene("SBK · Talking head");
		put(sc, comp("sbk_frame", "SBK · Cam frame full",
			     "{\"aspect\":\"16x9\",\"size\":2.6,\"style\":\"corner\",\"bracket\":96.0,\"label\":\"\","
			     "\"line\":\"accent\",\"weight\":4.0}"),
		    MIDX, EDGE, TC);
		put(sc, comp("sbk_lower_third", "SBK · Lower third minimal",
			     "{\"variant\":\"minimal\",\"bar\":true}"),
		    EDGE + 40, 980, BL);
		put(sc, comp("sbk_chip", "SBK · Topic chip",
			     "{\"label\":\"Chapter 1 — setting up\",\"variant\":\"card\",\"dot\":\"accent\"}"),
		    1920 - EDGE - 40, 980, BR);
		put(sc, light("dot"), 1920 - EDGE, EDGE, TR);
		obs_scene_release(sc);
		n++;
	}
	/* 5. Screen share — the code has the frame, so everything hugs the edges */
	{
		obs_scene_t *sc = fresh_scene("SBK · Screen share");
		put(sc, comp("sbk_frame", "SBK · Cam frame small",
			     "{\"aspect\":\"16x9\",\"size\":0.66,\"style\":\"ring\",\"radius\":16.0,\"label\":\"@imswarnil\","
			     "\"chip_at\":\"bottom-left\"}"),
		    1920 - EDGE, 1080 - EDGE, BR);
		put(sc, comp("sbk_chip", "SBK · Topic chip",
			     "{\"label\":\"Chapter 1 — setting up\",\"variant\":\"card\",\"dot\":\"accent\"}"),
		    EDGE, EDGE, TL);
		put(sc, comp("sbk_progress", "SBK · Chapter progress",
			     "{\"label\":\"Chapters\",\"value\":1,\"target\":8,\"style\":\"segments\",\"segments\":8,"
			     "\"as_percent\":false,\"width\":420,\"step\":1}"),
		    EDGE, 1080 - EDGE, BL);
		put(sc, comp("sbk_meter", "SBK · Mic meter",
			     "{\"source\":\"@mic\",\"label\":\"Mic\",\"width\":300,\"style\":\"segments\","
			     "\"segment_count\":20,\"show_db\":false}"),
		    EDGE, 1080 - EDGE - 130, BL);
		put(sc, light("badge"), 1920 - EDGE, EDGE, TR);
		obs_scene_release(sc);
		n++;
	}
	/* 6. Interview — two cameras, two names, nothing else competing */
	{
		obs_scene_t *sc = fresh_scene("SBK · Interview");
		put(sc, comp("sbk_frame", "SBK · Cam frame left",
			     "{\"aspect\":\"16x9\",\"size\":1.31,\"style\":\"ring\",\"label\":\"\"}"),
		    EDGE, 300, TL);
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
		obs_scene_release(sc);
		n++;
	}
	/* 7. Q&A — the question gets the left half and the camera the right */
	{
		obs_scene_t *sc = fresh_scene("SBK · Q&A");
		put(sc, backdrop("SBK · Backdrop scrim", "scrim", "\"reach\":0.85"), 0, 0, TL);
		put(sc, comp("sbk_chip", "SBK · QA chip",
			     "{\"label\":\"Question\",\"variant\":\"accent\",\"dot\":\"none\"}"),
		    EDGE, 260, TL);
		put(sc, comp("sbk_card", "SBK · Question card",
			     card_json("", "How do you structure a Salesforce org for scale?",
				       "Asked by a viewer in the chat.", "", "split", "left", 860)),
		    EDGE, 360, TL);
		put(sc, comp("sbk_frame", "SBK · Cam frame qa",
			     "{\"aspect\":\"16x9\",\"size\":1.19,\"style\":\"inset\",\"label\":\"@imswarnil\"}"),
		    1920 - EDGE, 360, TR);
		put(sc, comp("sbk_ticker", "SBK · Ticker", "{\"width\":1920}"), 0, 1080, BL);
		put(sc, light(""), 1920 - EDGE, EDGE, TR);
		obs_scene_release(sc);
		n++;
	}
	/* 8. Be right back */
	{
		obs_scene_t *sc = fresh_scene("SBK · Be right back");
		put(sc, backdrop("SBK · Backdrop dots", "dots", "\"drift\":-4.0,\"pitch\":48.0"), 0, 0, TL);
		put(sc, comp("sbk_visualizer", "SBK · Viz wave", "{\"style\":\"wave\",\"height\":240}"), 0, 1080, BL);
		put(sc, light(""), EDGE, EDGE, TL);
		put(sc, comp("sbk_clock", "SBK · Clock", NULL), 1920 - EDGE, EDGE, TR);
		put(sc, comp("sbk_card", "SBK · Break card",
			     card_json("Paused", "Be right back", "Two minutes. Stretch your legs.",
				       "@imswarnil | imswarnil.com", "card", "left", 1000)),
		    EDGE, 340, TL);
		put(sc, comp("sbk_countdown", "SBK · Break countdown",
			     "{\"mode\":\"duration\",\"minutes\":5,\"draw_card\":true}"),
		    1920 - EDGE, 340, TR);
		obs_scene_release(sc);
		n++;
	}
	/* 9. Technical difficulties — the one scene that should look wrong on
	   purpose, so nobody mistakes it for the show */
	{
		obs_scene_t *sc = fresh_scene("SBK · Technical difficulties");
		put(sc, backdrop("SBK · Backdrop vignette", "vignette", "\"reach\":0.75"), 0, 0, TL);
		put(sc, comp("sbk_card", "SBK · Trouble card",
			     card_json("Stand by", "Technical difficulties", "Back in a moment. Do not adjust your set.", "",
				       "accent", "centre", 1200)),
		    MIDX, 420, TC);
		put(sc, light("edge"), 0, 0, TL);
		obs_scene_release(sc);
		n++;
	}
	/* 10. Ending — the ask goes here, with the goal it is asking for */
	{
		obs_scene_t *sc = fresh_scene("SBK · Ending");
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
		put(sc, comp("sbk_qr", "SBK · Channel QR",
			     "{\"text\":\"https://youtube.com/@imswarnil\",\"caption\":\"Scan to subscribe\","
			     "\"sub\":\"\",\"code_size\":240,\"variant\":\"none\",\"invert\":true}"),
		    1920 - EDGE, 1080 - EDGE, BR);
		put(sc, light(""), EDGE, EDGE, TL);
		obs_scene_release(sc);
		n++;
	}

	/* 11. Support — the ask, with something to scan. The counters are wired
	   to nothing until you put your own key in: they show a dash and a grey
	   lamp until then, which is the honest thing for them to do. */
	{
		obs_scene_t *sc = fresh_scene("SBK · Support");
		put(sc, backdrop("SBK · Backdrop rings", "rings", "\"drift\":2.0,\"pitch\":120.0"), 0, 0, TL);
		put(sc, comp("sbk_card", "SBK · Support card",
			     card_json("Support the channel", "Become a member",
				       "Members get the source for everything built on this stream.", "",
				       "none", "left", 900)),
		    EDGE, 240, TL);
		put(sc, comp("sbk_qr", "SBK · Membership QR",
			     "{\"text\":\"https://imswarnil.com/#/portal/signup\",\"caption\":\"Scan to join\","
			     "\"sub\":\"imswarnil.com\",\"code_size\":300,\"variant\":\"card\"}"),
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
		obs_scene_release(sc);
		n++;
	}
	/* 12. Vertical — a 9:16 crop inside the landscape canvas, for the clip
	   that becomes a Short. Frame yourself inside the box and the same take
	   cuts both ways. */
	{
		obs_scene_t *sc = fresh_scene("SBK · Vertical");
		put(sc, backdrop("SBK · Backdrop hex", "hex", "\"drift\":3.0,\"pitch\":60.0"), 0, 0, TL);
		put(sc, comp("sbk_frame", "SBK · Cam frame vertical",
			     "{\"aspect\":\"9x16\",\"size\":1.22,\"style\":\"ring\",\"radius\":24.0,"
			     "\"label\":\"@imswarnil\",\"chip_at\":\"bottom-left\"}"),
		    MIDX, 1080 - 30, OBS_ALIGN_BOTTOM | OBS_ALIGN_CENTER);
		put(sc, comp("sbk_card", "SBK · Vertical card",
			     card_json("Short", "One idea, sixty seconds", "", "", "none", "centre", 760)),
		    MIDX, 56, TC);
		put(sc, light("dot"), 1920 - EDGE, EDGE, TR);
		obs_scene_release(sc);
		n++;
	}
	/* 13. Desk — not for the stream. Open it as a windowed projector on a
	   second monitor and it is a read-out of how the broadcast is going. */
	{
		obs_scene_t *sc = fresh_scene("SBK · Desk (private)");
		put(sc, backdrop("SBK · Backdrop desk", "solid", ""), 0, 0, TL);
		put(sc, comp("sbk_stats", "SBK · Stats",
			     "{\"width\":760,\"show_fps\":true,\"variant\":\"card\"}"),
		    EDGE, 200, TL);
		put(sc, comp("sbk_meter", "SBK · Program meter",
			     "{\"source\":\"@program\",\"label\":\"Program\",\"width\":760}"),
		    EDGE, 460, TL);
		put(sc, comp("sbk_meter", "SBK · Mic meter desk",
			     "{\"source\":\"@mic\",\"label\":\"Mic\",\"width\":760}"),
		    EDGE, 600, TL);
		put(sc, comp("sbk_clock", "SBK · Clock seconds", "{\"seconds\":true}"), 1920 - EDGE, 200, TR);
		put(sc, light(""), 1920 - EDGE, EDGE, TR);
		obs_scene_release(sc);
		n++;
	}

	/* the show's own transition, if it has been created */
	obs_frontend_set_transition_duration(400);
	obs_source_t *first = obs_get_source_by_name("SBK · Starting soon");
	if (first) {
		obs_frontend_set_current_scene(first);
		obs_source_release(first);
	}
	release_held();
	SBK_LOG(LOG_INFO, "%d scenes built", n);
	return n;
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

static const char *const WALK[] = {
	"SBK · Starting soon",          "SBK · Welcome",   "SBK · Live",
	"SBK · Talking head",           "SBK · Screen share", "SBK · Interview",
	"SBK · Q&A",                    "SBK · Support",   "SBK · Vertical",
	"SBK · Be right back",          "SBK · Technical difficulties", "SBK · Ending",
	"SBK · Desk (private)",
};
#define N_WALK (sizeof(WALK) / sizeof(WALK[0]))

static void step_switch(void *param)
{
	obs_source_t *s = obs_get_source_by_name((const char *)param);
	if (!s) {
		SBK_LOG(LOG_WARNING, "self-test: no scene called '%s'", (const char *)param);
		return;
	}
	obs_frontend_set_current_scene(s);
	obs_source_release(s);
}

static void step_shoot(void *param)
{
	UNUSED_PARAMETER(param);
	obs_frontend_take_screenshot();
}

static void *walk_thread(void *arg)
{
	UNUSED_PARAMETER(arg);
	os_set_thread_name("sbk-selftest");
	for (size_t i = 0; i < N_WALK; i++) {
		obs_queue_task(OBS_TASK_UI, step_switch, (void *)WALK[i], true);
		/* the transition has to land before the shot, or the frame is a
		   blend of two scenes */
		os_sleep_ms(1500);
		obs_queue_task(OBS_TASK_UI, step_shoot, NULL, true);
		os_sleep_ms(600);
	}
	SBK_LOG(LOG_INFO, "self-test: walked %zu scenes, screenshots are in the recording folder", N_WALK);
	return NULL;
}

void sbk_selftest(void)
{
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
