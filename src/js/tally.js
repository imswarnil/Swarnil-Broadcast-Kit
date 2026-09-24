/* Tally runtime — loaded by every overlay page. It applies the URL knobs,
   fills text from params, connects the OBS bridge, and mounts whatever the
   page declares with `data-tally-*` attributes. A page needs no script of
   its own. The `Tally` global is exposed for anyone who wants more. */

import * as P from './params.mjs';
import * as OBS from './obs.mjs';
import { createAudio } from './audio.mjs';
import { mount as mountViz } from './viz.mjs';
import * as W from './widgets.mjs';

const Tally = {
	version: '__TALLY_VERSION__',
	params: P,
	obs: OBS,
	audio: null,
	show(sel = '[data-tally-visible]') {
		document.querySelectorAll(sel).forEach((el) => (el.dataset.tallyVisible = 'true'));
	},
	hide(sel = '[data-tally-visible]') {
		document.querySelectorAll(sel).forEach((el) => (el.dataset.tallyVisible = 'false'));
	},
};

async function boot() {
	P.applyGlobals();
	P.fill();
	OBS.connect({ demo: P.bool('demo') });

	document.querySelectorAll('[data-tally-clock]').forEach((el) =>
		W.clock(el, { hour12: !P.bool('h24'), seconds: P.bool('seconds') }));

	document.querySelectorAll('[data-tally-count]').forEach((el) =>
		W.countdown(el, P.get('at', el.dataset.tallyCount), { done: P.get('done', 'Now') }));

	document.querySelectorAll('[data-tally-onair]').forEach((el) => W.onair(el));

	document.querySelectorAll('.tally-ticker').forEach((el) => {
		W.ticker(el, { pxPerSecond: P.num('speed', 90) });
		new ResizeObserver(() => W.ticker(el, { pxPerSecond: P.num('speed', 90) })).observe(el);
	});

	const canvases = document.querySelectorAll('[data-tally-viz]');
	if (canvases.length) {
		Tally.audio = await createAudio({
			source: P.get('source', P.bool('demo') ? 'demo' : 'mic'),
			fft: P.num('fft', 2048),
			smoothing: Math.min(0.98, Math.max(0, P.num('smooth', 0.8))),
		});
		document.documentElement.dataset.tallyAudio = Tally.audio.mode;
		if (P.get("source") === "mic" && Tally.audio.mode === "demo") console.info("Tally: no microphone granted, painting the demo signal");
		canvases.forEach((c) =>
			mountViz(c, Tally.audio, { style: P.get('style', c.dataset.tallyViz || 'bars'), bars: P.num('bars', 48) }));
	}

	if (P.bool('hidden')) Tally.hide();
	document.documentElement.dataset.tallyReady = 'true';
	window.dispatchEvent(new CustomEvent('tally:ready'));
}

window.Tally = Tally;
if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', boot);
else boot();

export default Tally;
