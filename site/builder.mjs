/*  The Broadcast Builder — the palette, the defaults and the exporter.

    Two halves worth keeping straight:

      · the PREVIEW is a schematic. It is CSS approximating what each source
        looks like so you can judge placement and reading order. It is not the
        plugin and does not try to be: the plugin draws with shaders and real
        hinted type, and anything that looked pixel-perfect here would only ever
        drift away from it.
      · the EXPORT is exact. It writes the same scene-collection JSON that OBS
        writes itself, with the real source ids and the real setting keys, so
        what you place here is literally what OBS builds on import.

    The setting keys below have to match the C. When a source gains a property,
    add it here too or the builder will quietly export a scene missing it.  */

export const CANVAS = { w: 1920, h: 1080 };

/* `size` is the natural size at scale 1; sources that measure themselves get an
   estimate, which is only used to draw the box in the preview. */
export const PALETTE = [
	{
		id: 'sbk_onair', name: 'Light', group: 'Indicator', size: [168, 48], fixed: true,
		settings: { shape: 'pill', word_live: 'Live', word_off: 'Off air' },
		fields: [
			['shape', 'Shape', ['pill', 'badge', 'dot', 'bar', 'edge']],
			['word_live', 'While live', 'text'],
			['word_off', 'Otherwise', 'text'],
		],
	},
	{
		id: 'sbk_lower_third', name: 'Lower third', group: 'Titles', size: [520, 120], fixed: true,
		settings: { name: 'Swarnil Singhai', title: 'Salesforce Architect · imswarnil.com', variant: 'card', bar: true },
		fields: [
			['name', 'Name', 'text'],
			['title', 'Line under it', 'text'],
			['variant', 'Variant', ['card', 'pill', 'split', 'minimal', 'underline']],
		],
	},
	{
		id: 'sbk_logo', name: 'Logo', group: 'Brand', size: [140, 140],
		settings: { mark: 'ring', loop: 'orbit', mark_size: 96, speed: 3.2, variant: 'none' },
		fields: [
			['mark', 'Mark', ['ring', 'play', 'camera', 'at', 'chat', 'heart', 'bell', 'star', 'share', 'globe', 'code', 'person', 'bookmark', 'plus', 'arrow']],
			['loop', 'Loop', ['orbit', 'breathe', 'pulse', 'spin', 'draw', 'bob', 'none']],
			['mark_size', 'Mark size', 'number'],
			['speed', 'One cycle takes (seconds)', 'number'],
			['caption', 'Caption', 'text'],
		],
	},
	{
		id: 'sbk_social', name: 'Social', group: 'Titles', size: [420, 96],
		settings: { mode: 'rotate', rotate: 8.0, brand: true, show_name: true, variant: 'card',
			accounts: 'youtube: @imswarnil\nx: @imswarnil\ninstagram: @imswarnil\nweb: imswarnil.com' },
		fields: [
			['accounts', 'Accounts, one per line as platform: handle', 'text'],
			['mode', 'Shape', ['rotate', 'bar', 'stack']],
			['rotate', 'Change every (seconds)', 'number'],
			['brand', 'Each platform\u2019s own colour', 'bool'],
			['variant', 'Variant', ['card', 'glass', 'none', 'accent', 'outline']],
		],
	},
	{
		id: 'sbk_prompt', name: 'Prompt', group: 'Titles', size: [520, 108],
		settings: { edge: 'right', every: 180.0, hold: 8.0, width: 520, variant: 'card',
			lines: 'heart: Enjoying this? | A like costs you nothing and helps a lot.\nbell: Subscribe | There is a new build every Thursday.' },
		fields: [
			['lines', 'What it asks, one per line as mark: Title | Body', 'text'],
			['edge', 'Comes in from', ['right', 'left', 'top', 'bottom']],
			['every', 'Every (seconds)', 'number'],
			['hold', 'Stays for (seconds)', 'number'],
			['only_live', 'Only when on air', 'bool'],
		],
	},
	{
		id: 'sbk_ticker', name: 'Ticker', group: 'Titles', size: [1920, 56],
		settings: { width: 1920, tag: 'Now', text: 'Building a Salesforce app live | Questions in chat', variant: 'strip' },
		fields: [
			['tag', 'Tag', 'text'],
			['text', 'Items, split with |', 'text'],
			['variant', 'Variant', ['strip', 'bare', 'chips']],
		],
	},
	{
		id: 'sbk_plate', name: 'Plate', group: 'Camera', size: [640, 360],
		settings: { aspect: '16x9', size: 1.0, radius: 20.0, shadow_y: 18.0, shadow_blur: 52.0, fill: false },
		fields: [
			['aspect', 'Aspect', ['16x9', '9x16', '1x1', '4x5', '4x3', '21x9', 'custom']],
			['size', 'Size', 'number'],
			['radius', 'Corner radius', 'number'],
			['shadow_y', 'Shadow drop', 'number'],
			['shadow_blur', 'Shadow blur', 'number'],
			['fill', 'Fill the shape', 'bool'],
		],
	},
	{
		id: 'sbk_comments', name: 'Comments', group: 'Chat', size: [560, 420],
		settings: { width: 560, title: 'Questions', show_count: 3, rotate: 12.0, variant: 'card',
			manual: 'Priya: does this work with a capture card?\nMarco: which OBS version is this?' },
		fields: [
			['title', 'Heading', 'text'],
			['manual', 'Questions, one per line as Name: question', 'text'],
			['show_count', 'Show at once', 'number'],
			['rotate', 'Rotate every (seconds)', 'number'],
			['variant', 'Variant', ['card', 'glass', 'none', 'accent', 'outline']],
		],
	},
	{
		id: 'sbk_frame', name: 'Cam frame', group: 'Camera', size: [640, 360],
		settings: { aspect: '16x9', size: 1.0, style: 'ring', label: '@imswarnil' },
		fields: [
			['aspect', 'Shape', ['16x9', '9x16', '1x1', '4x5', '4x3', '21x9']],
			['style', 'Treatment', ['ring', 'inset', 'corner', 'corner-out', 'edge', 'glow', 'double']],
			['label', 'Chip', 'text'],
		],
	},
	{
		id: 'sbk_visualizer', name: 'Visualizer', group: 'Audio', size: [1920, 240],
		settings: { width: 1920, height: 240, style: 'bars', source: '@program' },
		fields: [
			['style', 'Style', ['bars', 'mirror', 'wave', 'dots', 'ring', 'blocks', 'line']],
			['source', 'Listen to', ['@program', '@desktop', '@mic', '']],
		],
	},
	{
		id: 'sbk_meter', name: 'Meter', group: 'Audio', size: [420, 96],
		settings: { width: 420, source: '@mic', label: 'Mic', style: 'segments' },
		fields: [
			['label', 'Label', 'text'],
			['source', 'Listen to', ['@mic', '@program', '@desktop']],
			['style', 'Style', ['segments', 'solid']],
		],
	},
	{
		id: 'sbk_stats', name: 'Stats', group: 'Indicator', size: [420, 150],
		settings: { width: 420, show_fps: false },
		fields: [['width', 'Width', 'number']],
	},
	{
		id: 'sbk_counter', name: 'Counter', group: 'Live data', size: [420, 150],
		settings: { width: 420, provider: 'youtube', label: 'Subscribers', compact: true },
		fields: [
			['provider', 'From', ['youtube', 'ghost', 'json']],
			['label', 'Label', 'text'],
		],
	},
	{
		id: 'sbk_qr', name: 'QR', group: 'Live data', size: [348, 420],
		settings: { text: 'https://imswarnil.com', caption: 'Scan to visit', code_size: 300 },
		fields: [
			['text', 'Link', 'text'],
			['caption', 'Caption', 'text'],
			['code_size', 'Code size', 'number'],
		],
	},
	{
		id: 'sbk_card', name: 'Card', group: 'Scenes', size: [1100, 300],
		settings: { width: 1100, eyebrow: 'Starting soon', title: 'Building a Salesforce app live', body: 'Grab a coffee.', chips: '@imswarnil | imswarnil.com', variant: 'card', align: 'left' },
		fields: [
			['eyebrow', 'Eyebrow', 'text'],
			['title', 'Title', 'text'],
			['body', 'Body', 'text'],
			['variant', 'Variant', ['card', 'split', 'outline', 'accent', 'none']],
			['align', 'Align', ['left', 'centre']],
		],
	},
	{
		id: 'sbk_backdrop', name: 'Backdrop', group: 'Scenes', size: [1920, 1080], ground: true,
		settings: { width: 1920, height: 1080, mode: 'grid', drift: 6 },
		fields: [
			['mode', 'Kind', ['solid', 'scrim', 'vignette', 'gradient', 'grid', 'dots', 'stripes', 'waves', 'rings', 'hex', 'grain', 'aurora', 'plasma', 'stars', 'checkers']],
		],
	},
	{
		id: 'sbk_chip', name: 'Chip', group: 'Indicator', size: [200, 44], fixed: true,
		settings: { label: '@imswarnil', variant: 'pill', dot: 'accent' },
		fields: [
			['label', 'Label', 'text'],
			['value', 'Value', 'text'],
			['variant', 'Variant', ['pill', 'card', 'outline', 'accent', 'none']],
		],
	},
	{
		id: 'sbk_progress', name: 'Progress', group: 'Indicator', size: [520, 90],
		settings: { width: 520, label: 'Subscriber goal', value: 640, target: 1000, style: 'solid' },
		fields: [
			['label', 'Label', 'text'],
			['value', 'Value', 'number'],
			['target', 'Target', 'number'],
			['style', 'Bar', ['solid', 'segments', 'line']],
		],
	},
	{
		id: 'sbk_countdown', name: 'Timer', group: 'Scenes', size: [280, 120], fixed: true,
		settings: { mode: 'duration', minutes: 15, style: 'digits', draw_card: true },
		fields: [
			['mode', 'Counts', ['duration', 'time', 'up', 'uptime']],
			['style', 'Style', ['digits', 'ring', 'ring-only', 'bar']],
			['minutes', 'Minutes', 'number'],
			['label', 'Label', 'text'],
		],
	},
	{
		id: 'sbk_clock', name: 'Clock', group: 'Scenes', size: [190, 56], fixed: true,
		settings: { h24: false, seconds: false },
		fields: [['h24', '24-hour', 'bool'], ['seconds', 'Seconds', 'bool']],
	},
];

export const START = [
	{
		id: 'blank', name: 'Blank',
		items: [],
	},
	{
		id: 'live', name: 'Live',
		items: [
			{ id: 'sbk_ticker', x: 0, y: 1024, settings: { width: 1920 } },
			{ id: 'sbk_lower_third', x: 120, y: 840 },
			{ id: 'sbk_frame', x: 1160, y: 560 },
			{ id: 'sbk_onair', x: 1632, y: 120 },
		],
	},
	{
		id: 'starting', name: 'Starting soon',
		items: [
			{ id: 'sbk_backdrop', x: 0, y: 0, settings: { mode: 'grid' } },
			{ id: 'sbk_visualizer', x: 0, y: 840 },
			{ id: 'sbk_onair', x: 120, y: 120 },
			{ id: 'sbk_clock', x: 1610, y: 120 },
			{ id: 'sbk_card', x: 120, y: 300 },
			{ id: 'sbk_countdown', x: 1520, y: 300, settings: { style: 'ring' } },
		],
	},
	{
		id: 'support', name: 'Support',
		items: [
			{ id: 'sbk_backdrop', x: 0, y: 0, settings: { mode: 'rings' } },
			{ id: 'sbk_card', x: 120, y: 240, settings: { eyebrow: 'Support the channel', title: 'Become a member', body: '', variant: 'none' } },
			{ id: 'sbk_qr', x: 1452, y: 240 },
			{ id: 'sbk_counter', x: 120, y: 620 },
			{ id: 'sbk_ticker', x: 0, y: 1024 },
		],
	},
	{
		id: 'teaching', name: 'Teaching',
		items: [
			{ id: 'sbk_plate', x: 1380, y: 724, settings: { aspect: '16x9', size: 0.66, radius: 16, shadow_y: 18, shadow_blur: 48 } },
			{ id: 'sbk_frame', x: 1380, y: 724, settings: { aspect: '16x9', size: 0.66, style: 'ring', radius: 16, label: '@imswarnil' } },
			{ id: 'sbk_comments', x: 120, y: 200, settings: { width: 560, title: 'Questions', show_count: 3 } },
			{ id: 'sbk_chip', x: 120, y: 120, settings: { label: 'Chapter 1 — setting up', variant: 'card', dot: 'accent' } },
			{ id: 'sbk_onair', x: 1632, y: 120, settings: { shape: 'badge' } },
		],
	},
];

export const byId = Object.fromEntries(PALETTE.map((p) => [p.id, p]));

/* ---- the export ------------------------------------------------------------

   An OBS scene collection is a flat list of sources where a scene is a source
   whose settings hold its items. The shape below is what OBS itself writes; the
   fields it does not strictly need are still included because OBS fills them in
   on load anyway and a diff against a hand-made collection should be boring. */

const sourceEntry = (name, id, settings) => ({
	id,
	versioned_id: id,
	name,
	enabled: true,
	muted: false,
	volume: 1.0,
	balance: 0.5,
	mixers: 0,
	sync: 0,
	flags: 0,
	deinterlace_mode: 0,
	deinterlace_field_order: 0,
	monitoring_type: 0,
	hotkeys: {},
	private_settings: {},
	'push-to-mute': false,
	'push-to-mute-delay': 0,
	'push-to-talk': false,
	'push-to-talk-delay': 0,
	settings,
});

export function toCollection(sceneName, items, look) {
	const sources = [];
	const used = new Map();
	const sceneItems = [];

	items.forEach((item, i) => {
		const def = byId[item.id];
		if (!def) return;
		/* one source per placement, named so two lower thirds in a scene are two
		   sources you can edit apart rather than one in two places */
		const base = `SBK · ${def.name}`;
		const n = (used.get(base) || 0) + 1;
		used.set(base, n);
		const name = n === 1 ? base : `${base} ${n}`;

		sources.push(sourceEntry(name, def.id, { ...def.settings, ...(item.settings || {}), ...look }));
		sceneItems.push({
			id: i + 1,
			name,
			visible: true,
			locked: false,
			align: 5, /* top-left, which is what x and y mean here */
			pos: { x: Math.round(item.x), y: Math.round(item.y) },
			rot: 0.0,
			scale: { x: 1.0, y: 1.0 },
			bounds: { x: 0.0, y: 0.0 },
			bounds_type: 0,
			bounds_align: 0,
			crop_left: 0, crop_top: 0, crop_right: 0, crop_bottom: 0,
			scale_filter: 'disable',
			blend_method: 'default',
			blend_type: 'normal',
			private_settings: {},
			show_transition: {},
			hide_transition: {},
		});
	});

	sources.push({
		...sourceEntry(sceneName, 'scene', { custom_size: false, id_counter: sceneItems.length, items: sceneItems }),
		versioned_id: 'scene',
	});

	return {
		name: sceneName,
		current_scene: sceneName,
		current_program_scene: sceneName,
		current_transition: 'Fade',
		transition_duration: 300,
		scene_order: [{ name: sceneName }],
		transitions: [],
		quick_transitions: [
			{ name: 'Cut', duration: 300, hotkeys: [], id: 1, fade_to_black: false },
			{ name: 'Fade', duration: 300, hotkeys: [], id: 2, fade_to_black: false },
		],
		groups: [],
		saved_projectors: [],
		modules: {},
		sources,
	};
}
