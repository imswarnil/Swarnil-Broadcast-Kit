/* The overlay registry — every overlay Tally ships, described once. The
   docs site, the URL builder, the scene collection, the pack and the checks
   all read this file; nothing else lists overlays.

   size   — the Browser Source width × height to set in OBS
   params — what the URL accepts; `type` drives the builder's input
   scene  — where the generated scene collection places it (see scenes/) */

export const GLOBAL_PARAMS = [
	{ key: 'accent', label: 'Accent colour', type: 'color', default: '#f5273f', hint: 'Any CSS colour. The tally light, bars, eyebrow.' },
	{ key: 'scale', label: 'Scale', type: 'number', default: 1, min: 0.5, max: 3, step: 0.05, hint: 'Grows everything uniformly. 1 is sized for 1080p; try 2 for 4K.' },
	{ key: 'tone', label: 'Tone', type: 'select', default: '', options: ['', 'solid', 'light'], hint: 'glass (default), solid dark, or light.' },
	{ key: 'font', label: 'Font', type: 'text', default: '', hint: 'A font installed on the streaming machine. Geist otherwise.' },
];

export const OVERLAYS = [
	{
		slug: 'lower-third',
		name: 'Lower third',
		kind: 'component',
		description: 'A name and a line under it, the accent as a bar. Slides in on load.',
		size: { w: 1920, h: 1080 },
		params: [
			{ key: 'name', label: 'Name', type: 'text', default: 'Swarnil Singhai' },
			{ key: 'title', label: 'Title', type: 'text', default: 'Salesforce Architect · imswarnil.com' },
			{ key: 'variant', label: 'Variant', type: 'select', default: '', options: ['', 'pill', 'stacked'] },
			{ key: 'at', label: 'Position', type: 'select', default: 'bottom-left', options: ['bottom-left', 'bottom-right', 'top-left', 'top-right'] },
		],
		example: { name: 'Swarnil Singhai', title: 'Salesforce Architect · imswarnil.com' },
	},
	{
		slug: 'onair',
		name: 'On air',
		kind: 'component',
		description: 'The tally light. Reads OBS: LIVE while streaming, REC while recording, OFF AIR otherwise.',
		size: { w: 1920, h: 1080 },
		params: [
			{ key: 'variant', label: 'Variant', type: 'select', default: '', options: ['', 'solid'] },
			{ key: 'at', label: 'Position', type: 'select', default: 'top-right', options: ['top-right', 'top-left', 'bottom-right', 'bottom-left'] },
			{ key: 'demo', label: 'Demo cycle', type: 'toggle', default: false, hint: 'Cycles the states outside OBS. Ignored inside OBS.' },
		],
		example: { demo: 1 },
	},
	{
		slug: 'visualizer',
		name: 'Visualizer',
		kind: 'visualizer',
		description: 'Audio bars, a waveform, a ring or a dot matrix, from the microphone. Falls back to a demo signal.',
		size: { w: 1920, h: 240 },
		params: [
			{ key: 'style', label: 'Style', type: 'select', default: 'bars', options: ['bars', 'wave', 'ring', 'dots'] },
			{ key: 'bars', label: 'Bars', type: 'number', default: 48, min: 8, max: 128, step: 1 },
			{ key: 'mirror', label: 'Mirror', type: 'toggle', default: false, hint: 'Bars grow from the middle.' },
			{ key: 'cap', label: 'Peak caps', type: 'toggle', default: false },
			{ key: 'mono', label: 'White', type: 'toggle', default: false, hint: 'Ink instead of the accent.' },
			{ key: 'source', label: 'Source', type: 'select', default: 'mic', options: ['mic', 'demo'], hint: 'mic = the default input OBS exposes.' },
			{ key: 'smooth', label: 'Smoothing', type: 'number', default: 0.8, min: 0, max: 0.98, step: 0.02 },
		],
		example: { source: 'demo' },
	},
	{
		slug: 'frame',
		name: 'Webcam frame',
		kind: 'component',
		description: 'A rounded outline with a chip on its edge. Put it over the camera; size it with w and h.',
		size: { w: 640, h: 400 },
		params: [
			{ key: 'w', label: 'Width', type: 'number', default: 640, min: 120, max: 1920, step: 2 },
			{ key: 'h', label: 'Height', type: 'number', default: 360, min: 120, max: 1080, step: 2 },
			{ key: 'label', label: 'Chip', type: 'text', default: '@imswarnil' },
			{ key: 'variant', label: 'Variant', type: 'select', default: '', options: ['', 'accent', 'round'] },
		],
		example: { label: '@imswarnil' },
	},
	{
		slug: 'ticker',
		name: 'Ticker',
		kind: 'component',
		description: 'A strip of text sliding across the foot of the screen. Separate items with a pipe.',
		size: { w: 1920, h: 120 },
		params: [
			{ key: 'tag', label: 'Tag', type: 'text', default: 'Now' },
			{ key: 'text', label: 'Text', type: 'text', default: 'Building a Salesforce app live | Questions in chat | imswarnil.com' },
			{ key: 'speed', label: 'Speed (px/s)', type: 'number', default: 90, min: 20, max: 400, step: 10 },
		],
		example: {},
	},
	{
		slug: 'starting-soon',
		name: 'Starting soon',
		kind: 'scene',
		description: 'A full-screen scene: a card with a countdown, chips, a visualizer along the foot, the clock in a corner.',
		size: { w: 1920, h: 1080 },
		params: [
			{ key: 'title', label: 'Title', type: 'text', default: 'Building a Salesforce app live' },
			{ key: 'body', label: 'Body', type: 'text', default: 'Grab a coffee. We begin at the top of the hour.' },
			{ key: 'eyebrow', label: 'Eyebrow', type: 'text', default: 'Starting soon' },
			{ key: 'at', label: 'Countdown', type: 'text', default: '15m', hint: '"15m", "1h30m", "21:30", or an ISO date.' },
			{ key: 'chips', label: 'Chips', type: 'text', default: '@imswarnil | youtube.com/@imswarnil | imswarnil.com', hint: 'Separate with a pipe.' },
			{ key: 'brand', label: 'Brand', type: 'text', default: 'Swarnil' },
			{ key: 'backdrop', label: 'Backdrop', type: 'select', default: '', options: ['', 'none', 'scrim'], hint: 'Solid (default), transparent, or a bottom scrim over a camera.' },
			{ key: 'style', label: 'Visualizer', type: 'select', default: 'bars', options: ['bars', 'wave', 'dots'] },
			{ key: 'source', label: 'Audio source', type: 'select', default: 'mic', options: ['mic', 'demo'] },
		],
		example: { source: 'demo', at: '15m' },
	},
	{
		slug: 'brb',
		name: 'Be right back',
		kind: 'scene',
		description: 'The same scene as Starting soon, worded for a break, with a clock instead of a countdown.',
		size: { w: 1920, h: 1080 },
		params: [
			{ key: 'title', label: 'Title', type: 'text', default: 'Be right back' },
			{ key: 'body', label: 'Body', type: 'text', default: 'Two minutes. Stretch your legs.' },
			{ key: 'eyebrow', label: 'Eyebrow', type: 'text', default: 'Paused' },
			{ key: 'chips', label: 'Chips', type: 'text', default: '@imswarnil | imswarnil.com', hint: 'Separate with a pipe.' },
			{ key: 'brand', label: 'Brand', type: 'text', default: 'Swarnil' },
			{ key: 'backdrop', label: 'Backdrop', type: 'select', default: '', options: ['', 'none', 'scrim'] },
			{ key: 'style', label: 'Visualizer', type: 'select', default: 'wave', options: ['bars', 'wave', 'dots'] },
			{ key: 'source', label: 'Audio source', type: 'select', default: 'mic', options: ['mic', 'demo'] },
		],
		example: { source: 'demo' },
	},
];

export const bySlug = Object.fromEntries(OVERLAYS.map((o) => [o.slug, o]));

/* Build the URL for an overlay from a params object, dropping defaults. */
export function overlayUrl(base, slug, values = {}) {
	const o = bySlug[slug];
	const all = [...GLOBAL_PARAMS, ...o.params];
	const q = new URLSearchParams();
	for (const p of all) {
		const v = values[p.key];
		if (v === undefined || v === null || v === '' || v === false) continue;
		if (String(v) === String(p.default)) continue;
		q.set(p.key, v === true ? '1' : String(v));
	}
	const s = q.toString();
	return `${base}${slug}/${s ? `?${s}` : ''}`;
}
