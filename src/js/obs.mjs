/* OBS bridge — the real state of the program, from the obs-browser plugin.
   Inside a Browser Source OBS injects `window.obsstudio` and fires DOM
   events on `window` when streaming or recording changes. Outside OBS
   (in a browser tab, on the docs site) there is no plugin, so `?demo=1`
   walks through the states instead. */

export const status = {
	inObs: typeof window.obsstudio !== 'undefined',
	streaming: false,
	recording: false,
	recordingPaused: false,
	replayBuffer: false,
	virtualCam: false,
	scene: null,
	visible: true,
	active: true,
};

const listeners = new Set();

export function onStatus(fn) {
	listeners.add(fn);
	fn(status);
	return () => listeners.delete(fn);
}

function emit() {
	const root = document.documentElement;
	root.dataset.tallyStreaming = String(status.streaming);
	root.dataset.tallyRecording = String(status.recording);
	root.dataset.tallyState = state();
	for (const fn of listeners) fn(status);
	window.dispatchEvent(new CustomEvent('tally:status', { detail: { ...status } }));
}

/* One word for the light: both | live | rec | off. */
export function state() {
	if (status.streaming && status.recording) return 'both';
	if (status.streaming) return 'live';
	if (status.recording) return 'rec';
	return 'off';
}

const EVENTS = {
	obsStreamingStarted: () => (status.streaming = true),
	obsStreamingStopped: () => (status.streaming = false),
	obsRecordingStarted: () => { status.recording = true; status.recordingPaused = false; },
	obsRecordingPaused: () => (status.recordingPaused = true),
	obsRecordingUnpaused: () => (status.recordingPaused = false),
	obsRecordingStopped: () => { status.recording = false; status.recordingPaused = false; },
	obsReplaybufferStarted: () => (status.replayBuffer = true),
	obsReplaybufferStopped: () => (status.replayBuffer = false),
	obsVirtualcamStarted: () => (status.virtualCam = true),
	obsVirtualcamStopped: () => (status.virtualCam = false),
	obsSceneChanged: (e) => (status.scene = e.detail?.name ?? null),
	obsSourceVisibleChanged: (e) => (status.visible = !!e.detail?.visible),
	obsSourceActiveChanged: (e) => (status.active = !!e.detail?.active),
};

export function connect({ demo = false } = {}) {
	for (const [name, apply] of Object.entries(EVENTS)) {
		window.addEventListener(name, (e) => { apply(e); emit(); });
	}

	if (status.inObs && window.obsstudio.getStatus) {
		window.obsstudio.getStatus((s) => {
			status.streaming = !!s.streaming;
			status.recording = !!s.recording;
			status.replayBuffer = !!s.replaybuffer;
			status.virtualCam = !!s.virtualcam;
			emit();
		});
		window.obsstudio.getCurrentScene?.((s) => { status.scene = s?.name ?? null; emit(); });
	}

	if (demo && !status.inObs) {
		const steps = [
			{ streaming: false, recording: false },
			{ streaming: false, recording: true },
			{ streaming: true, recording: true },
			{ streaming: true, recording: false },
		];
		let i = 0;
		setInterval(() => {
			Object.assign(status, steps[i = (i + 1) % steps.length]);
			emit();
		}, 3000);
	}

	emit();
	return status;
}
