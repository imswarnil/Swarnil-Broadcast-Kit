/*  The control panel — a dock inside OBS.

    The phone remote covers being away from the desk. This covers being at it:
    the same actions without hunting through menus, and the two or three numbers
    that tell you whether the broadcast is healthy.

    Stock Qt widgets and lambdas, no Q_OBJECT of our own, so there is no moc
    step and no code generation. Headers come from Homebrew and the frameworks
    from OBS.app at the same version — loading a second Qt would crash on the
    first widget.

    Everything here runs on the UI thread: the timer, the buttons, the rebuild
    on a scene-list change. Nothing touches the graphics or audio threads.  */

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/platform.h>

#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <functional>
#include <string>
#include <vector>

extern "C" {
#include "scenes.h"
#include "sbk-state.h"
}

namespace {

QString timecode(double seconds)
{
	const int s = seconds > 0 ? (int)seconds : 0;
	return QString("%1:%2:%3")
		.arg(s / 3600, 2, 10, QChar('0'))
		.arg((s / 60) % 60, 2, 10, QChar('0'))
		.arg(s % 60, 2, 10, QChar('0'));
}

/* The kit's own hotkeys, found rather than listed, so one added to a source
   later turns up in the panel without this file changing. */
struct HotkeyRow {
	obs_hotkey_id id;
	std::string name, description;
};

bool collect_hotkey(void *data, obs_hotkey_id id, obs_hotkey_t *key)
{
	auto *out = static_cast<std::vector<HotkeyRow> *>(data);
	const char *name = obs_hotkey_get_name(key);
	if (!name || strncmp(name, "SBK.", 4) != 0)
		return true;
	const char *desc = obs_hotkey_get_description(key);
	out->push_back({id, name, desc ? desc : name});
	return true;
}

/* "Broadcast Kit: punch in or out" reads better on a small button as
   "Punch in or out" — the panel is already the kit. */
QString short_label(const std::string &description)
{
	const std::string prefix = "Broadcast Kit: ";
	if (description.rfind(prefix, 0) == 0) {
		std::string rest = description.substr(prefix.size());
		if (!rest.empty())
			rest[0] = (char)toupper((unsigned char)rest[0]);
		return QString::fromStdString(rest);
	}
	return QString::fromStdString(description);
}

class Panel {
public:
	QWidget *root = nullptr;
	QPushButton *stream = nullptr, *record = nullptr, *mic = nullptr;
	QLabel *clock = nullptr, *health = nullptr;
	QVBoxLayout *sceneBox = nullptr, *hotkeyBox = nullptr;
	uint64_t lastBytes = 0, lastNs = 0;
	double kbps = 0.0;

	void rebuildScenes()
	{
		clearLayout(sceneBox);
		struct obs_frontend_source_list list = {};
		obs_frontend_get_scenes(&list);
		obs_source_t *current = obs_frontend_get_current_scene();
		const char *currentName = current ? obs_source_get_name(current) : nullptr;

		for (size_t i = 0; i < list.sources.num; i++) {
			obs_source_t *src = list.sources.array[i];
			const char *name = obs_source_get_name(src);
			if (!name)
				continue;
			auto *b = new QPushButton(QString::fromUtf8(name));
			b->setMinimumHeight(30);
			b->setCheckable(true);
			b->setChecked(currentName && strcmp(name, currentName) == 0);
			const std::string want = name;
			QObject::connect(b, &QPushButton::clicked, [want] {
				obs_source_t *s = obs_get_source_by_name(want.c_str());
				if (s) {
					obs_frontend_set_current_scene(s);
					obs_source_release(s);
				}
			});
			sceneBox->addWidget(b);
		}
		if (current)
			obs_source_release(current);
		obs_frontend_source_list_free(&list);
	}

	void rebuildHotkeys()
	{
		clearLayout(hotkeyBox);
		std::vector<HotkeyRow> rows;
		obs_enum_hotkeys(collect_hotkey, &rows);
		for (const auto &row : rows) {
			auto *b = new QPushButton(short_label(row.description));
			b->setMinimumHeight(30);
			b->setToolTip(QString::fromStdString(row.name));
			const obs_hotkey_id id = row.id;
			QObject::connect(b, &QPushButton::clicked, [id] {
				/* press and release, or a hotkey that only acts on the
				   press half never fires from a button */
				obs_hotkey_trigger_routed_callback(id, true);
				obs_hotkey_trigger_routed_callback(id, false);
			});
			hotkeyBox->addWidget(b);
		}
		if (rows.empty()) {
			auto *l = new QLabel("Add a Broadcast Kit source and its hotkeys appear here.");
			l->setWordWrap(true);
			l->setStyleSheet("color:#8a8f98;");
			hotkeyBox->addWidget(l);
		}
	}

	void tick()
	{
		const bool live = obs_frontend_streaming_active();
		const bool rec = obs_frontend_recording_active();
		stream->setText(live ? "Stop streaming" : "Go live");
		stream->setChecked(live);
		record->setText(rec ? "Stop recording" : "Record");
		record->setChecked(rec);

		double up = sbk_state_uptime(false);
		if (up <= 0.0)
			up = sbk_state_uptime(true);
		clock->setText(timecode(up));

		obs_output_t *out = obs_frontend_get_streaming_output();
		QString text = "idle";
		QString colour = "#8a8f98";
		if (out) {
			const int dropped = obs_output_get_frames_dropped(out);
			const int total = obs_output_get_total_frames(out);
			const float congestion = obs_output_get_congestion(out);
			const uint64_t bytes = obs_output_get_total_bytes(out);
			const uint64_t now = os_gettime_ns();
			if (lastNs && now > lastNs && bytes >= lastBytes) {
				const double dt = (double)(now - lastNs) / 1e9;
				if (dt >= 0.9) {
					kbps = (double)(bytes - lastBytes) * 8.0 / 1000.0 / dt;
					lastBytes = bytes;
					lastNs = now;
				}
			} else {
				lastBytes = bytes;
				lastNs = now;
			}
			if (obs_output_active(out)) {
				const double drop = total > 0 ? (double)dropped * 100.0 / (double)total : 0.0;
				text = QString("%1 kb/s · %2% dropped").arg((int)kbps).arg(drop, 0, 'f', 1);
				colour = (congestion > 0.3f || drop > 1.0) ? "#ff5c33" : "#3fcf6a";
			}
			obs_output_release(out);
		} else {
			lastNs = 0;
		}
		health->setText(text);
		health->setStyleSheet(QString("color:%1;").arg(colour));

		obs_source_t *micSrc = obs_get_output_source(3);
		if (micSrc) {
			const bool muted = obs_source_muted(micSrc);
			mic->setEnabled(true);
			mic->setText(muted ? "Mic muted" : "Mic live");
			mic->setChecked(muted);
			obs_source_release(micSrc);
		} else {
			mic->setEnabled(false);
			mic->setText("No mic");
		}
	}

private:
	static void clearLayout(QLayout *layout)
	{
		while (QLayoutItem *item = layout->takeAt(0)) {
			if (QWidget *w = item->widget())
				w->deleteLater();
			delete item;
		}
	}
};

Panel *g_panel = nullptr;

void on_frontend_event(enum obs_frontend_event event, void *)
{
	if (!g_panel)
		return;
	switch (event) {
	case OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED:
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		g_panel->rebuildScenes();
		g_panel->rebuildHotkeys();
		break;
	case OBS_FRONTEND_EVENT_SCENE_CHANGED:
		g_panel->rebuildScenes();
		break;
	default:
		break;
	}
}

} // namespace

extern "C" void sbk_dock_register(void)
{
	auto *panel = new Panel();
	g_panel = panel;

	auto *root = new QWidget;
	panel->root = root;
	auto *outer = new QVBoxLayout(root);
	outer->setContentsMargins(10, 10, 10, 10);
	outer->setSpacing(10);

	/* the strip you look at rather than press */
	auto *strip = new QWidget;
	auto *striprow = new QHBoxLayout(strip);
	striprow->setContentsMargins(0, 0, 0, 0);
	panel->clock = new QLabel("00:00:00");
	panel->clock->setStyleSheet("font-family:'Geist Mono',ui-monospace,monospace;font-weight:600;");
	panel->health = new QLabel("idle");
	panel->health->setStyleSheet("color:#8a8f98;");
	striprow->addWidget(panel->clock);
	striprow->addStretch();
	striprow->addWidget(panel->health);
	outer->addWidget(strip);

	auto *controls = new QWidget;
	auto *crow = new QHBoxLayout(controls);
	crow->setContentsMargins(0, 0, 0, 0);
	panel->stream = new QPushButton("Go live");
	panel->record = new QPushButton("Record");
	panel->mic = new QPushButton("Mic");
	for (auto *b : {panel->stream, panel->record, panel->mic}) {
		b->setMinimumHeight(34);
		b->setCheckable(true);
	}
	crow->addWidget(panel->stream, 2);
	crow->addWidget(panel->record, 1);
	crow->addWidget(panel->mic, 1);
	outer->addWidget(controls);

	QObject::connect(panel->stream, &QPushButton::clicked, [] {
		if (obs_frontend_streaming_active())
			obs_frontend_streaming_stop();
		else
			obs_frontend_streaming_start();
	});
	QObject::connect(panel->record, &QPushButton::clicked, [] {
		if (obs_frontend_recording_active())
			obs_frontend_recording_stop();
		else
			obs_frontend_recording_start();
	});
	QObject::connect(panel->mic, &QPushButton::clicked, [] {
		obs_source_t *src = obs_get_output_source(3);
		if (!src)
			return;
		obs_source_set_muted(src, !obs_source_muted(src));
		obs_source_release(src);
	});

	auto *scenes = new QGroupBox("Scenes");
	auto *sceneLayout = new QVBoxLayout(scenes);
	sceneLayout->setSpacing(4);
	panel->sceneBox = sceneLayout;
	outer->addWidget(scenes);

	auto *actions = new QGroupBox("Kit actions");
	auto *actionLayout = new QVBoxLayout(actions);
	actionLayout->setSpacing(4);
	panel->hotkeyBox = actionLayout;
	outer->addWidget(actions);

	auto *build = new QGroupBox("Build");
	auto *buildLayout = new QVBoxLayout(build);
	buildLayout->setSpacing(4);
	struct {
		const char *label;
		void (*fn)(void);
	} buttons[] = {
		{"Build the show here", [] { sbk_build_scenes(); }},
		{"Create the scene collection", [] { sbk_create_collection(); }},
		{"Add the live pack to this scene", [] { sbk_add_live_pack(); }},
		{"Add my camera and microphone", [] { sbk_add_devices(); }},
	};
	for (const auto &b : buttons) {
		auto *btn = new QPushButton(b.label);
		btn->setMinimumHeight(30);
		auto fn = b.fn;
		QObject::connect(btn, &QPushButton::clicked, [fn] { fn(); });
		buildLayout->addWidget(btn);
	}
	outer->addWidget(build);
	outer->addStretch();

	/* a dock can be narrow and tall or short and wide; scrolling keeps the
	   build buttons reachable either way */
	auto *scroll = new QScrollArea;
	scroll->setWidget(root);
	scroll->setWidgetResizable(true);
	scroll->setFrameShape(QFrame::NoFrame);

	panel->rebuildScenes();
	panel->rebuildHotkeys();

	auto *timer = new QTimer(scroll);
	QObject::connect(timer, &QTimer::timeout, [panel] { panel->tick(); });
	timer->start(500);

	obs_frontend_add_event_callback(on_frontend_event, nullptr);
	obs_frontend_add_dock_by_id("sbk_control", "Broadcast Kit", scroll);
}

extern "C" void sbk_dock_unregister(void)
{
	obs_frontend_remove_event_callback(on_frontend_event, nullptr);
	g_panel = nullptr;
}
