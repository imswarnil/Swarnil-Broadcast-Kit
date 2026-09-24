/* Small behaviours the components need: a clock, a countdown, a ticker loop,
   the on-air label. Each is `mount(el, options)` and idempotent. */

import { onStatus, state } from './obs.mjs';

export function clock(el, { hour12 = true, seconds = false } = {}) {
	function tick() {
		const d = new Date();
		let h = d.getHours();
		const m = String(d.getMinutes()).padStart(2, '0');
		const s = String(d.getSeconds()).padStart(2, '0');
		let meridiem = '';
		if (hour12) {
			meridiem = h >= 12 ? 'PM' : 'AM';
			h = h % 12 || 12;
		}
		el.innerHTML = `${String(h).padStart(2, '0')}:${m}${seconds ? `:${s}` : ''}` +
			(meridiem ? `<span class="tally-clock__meridiem">${meridiem}</span>` : '');
		el.dateTime = d.toISOString();
	}
	tick();
	setInterval(tick, 1000);
}

/* `at` is an absolute time ("21:30" today, or an ISO date) or a duration
   ("15m", "1h30m", "90s"). When it reaches zero the element gets
   `data-done` and fires `tally:done`. */
export function countdown(el, at, { done = 'Now' } = {}) {
	const target = parseTarget(at);
	const parts = {
		hh: el.querySelector('[data-part="hh"]'),
		mm: el.querySelector('[data-part="mm"]'),
		ss: el.querySelector('[data-part="ss"]'),
	};
	function tick() {
		let left = Math.max(0, Math.round((target - Date.now()) / 1000));
		const h = Math.floor(left / 3600);
		const m = Math.floor((left % 3600) / 60);
		const s = left % 60;
		if (parts.hh) {
			parts.hh.textContent = String(h).padStart(2, '0');
			parts.hh.parentElement.querySelector('[data-part-sep="hh"]')?.toggleAttribute('hidden', h === 0);
			parts.hh.toggleAttribute('hidden', h === 0);
		}
		if (parts.mm) parts.mm.textContent = String(m).padStart(2, '0');
		if (parts.ss) parts.ss.textContent = String(s).padStart(2, '0');
		if (left === 0 && !el.hasAttribute('data-done')) {
			el.setAttribute('data-done', '');
			if (done) el.textContent = done;
			el.dispatchEvent(new CustomEvent('tally:done', { bubbles: true }));
		}
	}
	tick();
	setInterval(tick, 250);
}

function parseTarget(at) {
	if (!at) return Date.now() + 15 * 60 * 1000;
	const dur = /^(?:(\d+)h)?(?:(\d+)m)?(?:(\d+)s)?$/i.exec(at.trim());
	if (dur && (dur[1] || dur[2] || dur[3])) {
		return Date.now() + ((+dur[1] || 0) * 3600 + (+dur[2] || 0) * 60 + (+dur[3] || 0)) * 1000;
	}
	const hm = /^(\d{1,2}):(\d{2})(?::(\d{2}))?$/.exec(at.trim());
	if (hm) {
		const d = new Date();
		d.setHours(+hm[1], +hm[2], +(hm[3] || 0), 0);
		if (d.getTime() < Date.now()) d.setDate(d.getDate() + 1);
		return d.getTime();
	}
	const t = Date.parse(at);
	return Number.isFinite(t) ? t : Date.now() + 15 * 60 * 1000;
}

/* The ticker needs its text twice for a seamless loop, and a speed that
   depends on how long the text is, so every ticker moves at the same pace. */
export function ticker(el, { pxPerSecond = 90 } = {}) {
	const track = el.querySelector('.tally-ticker__track');
	const text = el.querySelector('.tally-ticker__text');
	if (!track || !text) return;
	track.querySelectorAll('.tally-ticker__text:not(:first-child)').forEach((n) => n.remove());
	const w = text.getBoundingClientRect().width;
	const copies = Math.max(2, Math.ceil(track.clientWidth / Math.max(1, w)) + 1);
	for (let i = 1; i < copies; i++) track.appendChild(text.cloneNode(true));
	el.style.setProperty('--tally-ticker-speed', `${(w / pxPerSecond).toFixed(2)}s`);
}

export function onair(el, labels = {}) {
	const words = { off: 'Off air', live: 'Live', rec: 'Rec', both: 'Live · Rec', ...labels };
	const label = el.querySelector('.tally-onair__label');
	onStatus(() => {
		const s = state();
		el.dataset.state = s;
		if (label) label.textContent = words[s];
	});
}
