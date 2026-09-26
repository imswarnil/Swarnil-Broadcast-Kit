#!/usr/bin/env node
/*  A scene spec in, an OBS scene collection out.

    OBS will import a scene collection from a JSON file, and that file is the
    only way to hand somebody a finished set of scenes without asking them to
    drag forty things into place. This turns a short spec — scenes, items,
    positions, settings — into one.

    It validates first, against `registry.json`, which is read out of the
    plugin's own C. An unknown source id or an unknown setting key stops the
    build and says which key it is and what the near misses were. That matters
    more than it sounds: OBS silently ignores a setting it does not recognise,
    so a typo produces a scene that looks almost right and nobody can say why.

        node build-collection.mjs show.json -o show.json.collection
        node build-collection.mjs show.json --check

    Then in OBS: Scene Collection → Import, pick the file, switch to it.
*/

import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const REG = JSON.parse(fs.readFileSync(path.join(HERE, 'registry.json'), 'utf8'));
const BY_ID = Object.fromEntries(REG.sources.map((s) => [s.id, s]));

/* OBS's own alignment bits: centre is zero, the rest are flags. */
const ALIGN = {
	'top-left': 5, top: 4, 'top-centre': 4, 'top-center': 4, 'top-right': 6,
	left: 1, centre: 0, center: 0, right: 2,
	'bottom-left': 9, bottom: 8, 'bottom-centre': 8, 'bottom-center': 8, 'bottom-right': 10,
};
/* enum obs_bounds_type — 3 is SCALE_OUTER, which crops rather than squashes */
const BOUNDS_SCALE_OUTER = 3;

const die = (msg) => {
	console.error(`✗ ${msg}`);
	process.exit(1);
};

/* the closest keys by edit distance, for the message when somebody mistypes one */
function nearest(word, options, n = 3) {
	const d = (a, b) => {
		const m = Array.from({ length: a.length + 1 }, (_, i) => [i, ...Array(b.length).fill(0)]);
		for (let j = 0; j <= b.length; j++) m[0][j] = j;
		for (let i = 1; i <= a.length; i++)
			for (let j = 1; j <= b.length; j++)
				m[i][j] = Math.min(m[i - 1][j] + 1, m[i][j - 1] + 1, m[i - 1][j - 1] + (a[i - 1] === b[j - 1] ? 0 : 1));
		return m[a.length][b.length];
	};
	return options
		.map((o) => [o, d(word, o)])
		.sort((a, b) => a[1] - b[1])
		.slice(0, n)
		.filter(([, dist]) => dist <= Math.max(3, word.length / 2))
		.map(([o]) => o);
}

export function validate(spec) {
	const problems = [];
	if (!spec || typeof spec !== 'object') die('the spec is not an object');
	if (!Array.isArray(spec.scenes) || !spec.scenes.length) die('the spec has no scenes');

	const seen = new Set();
	spec.scenes.forEach((scene, si) => {
		const where = `scene ${si + 1}${scene.name ? ` (${scene.name})` : ''}`;
		if (!scene.name) problems.push(`${where}: no name`);
		if (seen.has(scene.name)) problems.push(`${where}: two scenes share this name`);
		seen.add(scene.name);
		if (!Array.isArray(scene.items)) {
			problems.push(`${where}: no items`);
			return;
		}
		scene.items.forEach((item, ii) => {
			const at = `${where}, item ${ii + 1}`;
			if (item.id === 'camera' || item.id === 'screen' || item.id === 'external') return;
			const def = BY_ID[item.id];
			if (!def) {
				const near = nearest(item.id || '', Object.keys(BY_ID));
				problems.push(`${at}: no source called "${item.id}"${near.length ? ` — did you mean ${near.join(', ')}?` : ''}`);
				return;
			}
			if (def.kind !== 'Source')
				problems.push(`${at}: "${item.id}" is a ${def.kind.toLowerCase()}, which cannot be a scene item`);
			if (item.name && seen.has(`${scene.name}\u0000${item.name}`))
				problems.push(`${at}: two items in this scene are both called "${item.name}"`);
			if (item.name) seen.add(`${scene.name}\u0000${item.name}`);
			if (item.name && spec.scenes.some((s) => s.name === item.name))
				problems.push(
					`${at}: "${item.name}" is also a scene name. OBS keeps scenes and sources in one ` +
						`namespace, so the item would resolve to the scene and never appear.`
				);
			if (item.anchor && ALIGN[item.anchor] === undefined)
				problems.push(`${at}: no anchor called "${item.anchor}" — use ${Object.keys(ALIGN).slice(0, 9).join(', ')}`);

			for (const key of Object.keys(item.settings || {})) {
				if (def.settings[key]) continue;
				const near = nearest(key, Object.keys(def.settings));
				problems.push(
					`${at}: "${item.id}" has no setting "${key}"${near.length ? ` — did you mean ${near.join(', ')}?` : ''}`
				);
			}
			for (const [key, value] of Object.entries(item.settings || {})) {
				const def_ = def.settings[key];
				if (!def_ || !def_.values) continue;
				if (typeof value === 'string' && !def_.values.includes(value))
					problems.push(`${at}: "${key}" cannot be "${value}" — it takes ${def_.values.map((v) => `"${v}"`).join(', ')}`);
			}
		});
	});
	return problems;
}

function sourceEntry(name, id, settings) {
	return {
		prev_ver: 0,
		name,
		id,
		versioned_id: id,
		settings,
		mixers: 0,
		sync: 0,
		flags: 0,
		volume: 1.0,
		balance: 0.5,
		enabled: true,
		muted: false,
		'push-to-mute': false,
		'push-to-mute-delay': 0,
		'push-to-talk': false,
		'push-to-talk-delay': 0,
		hotkeys: {},
		deinterlace_mode: 0,
		deinterlace_field_order: 0,
		monitoring_type: 0,
		private_settings: {},
	};
}

export function build(spec) {
	const look = spec.look || {};
	const sources = [];
	const order = [];
	/* The canvas is not written into the collection — OBS keeps the resolution
	   in the profile, not here. The spec carries it so positions can be checked
	   against it, and so a reader knows what the numbers mean. */
	const canvas = spec.canvas || REG.canvas;
	for (const scene of spec.scenes)
		for (const item of scene.items || [])
			if ((item.x || 0) > canvas.width || (item.y || 0) > canvas.height)
				console.warn(`  ! "${item.name || item.id}" in ${scene.name} is off a ${canvas.width}×${canvas.height} canvas`);

	/* one source per placement, so two lower thirds are two things you can edit
	   apart rather than one thing in two places */
	const used = new Map();

	for (const scene of spec.scenes) {
		const items = [];
		(scene.items || []).forEach((item, i) => {
			let name = item.name;
			let entryId = item.id;
			let settings = { ...(item.settings || {}) };

			if (item.id === 'camera' || item.id === 'screen' || item.id === 'external') {
				/* a real device the spec only refers to: one shared source across
				   every scene that mentions it, exactly as the plugin does it */
				name = item.name || (item.id === 'camera' ? 'Camera' : item.id === 'screen' ? 'Screen' : 'External');
				if (!sources.some((s) => s.name === name)) {
					const def =
						item.id === 'camera'
							? { id: 'macos-avcapture', settings: {} }
							: item.id === 'screen'
								? { id: 'screen_capture', settings: { type: 0, show_cursor: true } }
								: { id: item.source_id || 'color_source', settings: {} };
					sources.push(sourceEntry(name, def.id, { ...def.settings, ...settings }));
				}
			} else {
				const def = BY_ID[item.id];
				if (!name) {
					const base = `SBK · ${def.name.replace(/^SBK /, '')}`;
					const n = (used.get(base) || 0) + 1;
					used.set(base, n);
					name = n === 1 ? base : `${base} ${n}`;
				}
				if (!sources.some((s) => s.name === name))
					sources.push(sourceEntry(name, entryId, { ...settings, ...look }));
			}

			const entry = {
				id: i + 1,
				name,
				visible: item.visible !== false,
				locked: false,
				align: ALIGN[item.anchor || 'top-left'],
				pos: { x: Math.round(item.x || 0), y: Math.round(item.y || 0) },
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
			};
			/* a box crops the source to fit rather than squashing it, which is what
			   you want for a camera whose shape never matches the hole */
			if (item.box) {
				entry.bounds_type = BOUNDS_SCALE_OUTER;
				entry.bounds_align = 0;
				entry.bounds = { x: item.box[0], y: item.box[1] };
			}
			items.push(entry);
		});

		sources.push({
			...sourceEntry(scene.name, 'scene', { custom_size: false, id_counter: items.length, items }),
			versioned_id: 'scene',
		});
		order.push({ name: scene.name });
	}

	const first = spec.scenes[0].name;
	return {
		name: spec.name || 'Swarnil Broadcast Kit',
		current_scene: first,
		current_program_scene: first,
		current_transition: spec.transition || 'Fade',
		transition_duration: spec.transition_duration ?? 300,
		scene_order: order,
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

/* ---- run it ----------------------------------------------------------------

    Only when this file is the thing that was run. Imported — the documentation
    site imports it to generate the ready-made collections it offers — the two
    functions above are all it is.  */

const invokedDirectly = (() => {
	if (!process.argv[1]) return false;
	try {
		return fs.realpathSync(process.argv[1]) === fs.realpathSync(fileURLToPath(import.meta.url));
	} catch {
		return false;
	}
})();
if (invokedDirectly) run();

function run() {
const args = process.argv.slice(2);
const input = args.find((a) => !a.startsWith('-'));
if (!input) die('usage: build-collection.mjs <spec.json> [-o out.json] [--check]');

let spec;
try {
	spec = JSON.parse(fs.readFileSync(input, 'utf8'));
} catch (e) {
	die(`could not read ${input}: ${e.message}`);
}

const problems = validate(spec);
if (problems.length) {
	console.error(`✗ ${problems.length} problem${problems.length === 1 ? '' : 's'}:`);
	for (const p of problems) console.error(`  - ${p}`);
	process.exit(1);
}

const nItems = spec.scenes.reduce((n, s) => n + (s.items || []).length, 0);
if (args.includes('--check')) {
	console.log(`✓ ${spec.scenes.length} scene${spec.scenes.length === 1 ? '' : 's'}, ${nItems} items, every id and key known`);
	process.exit(0);
}

const oi = args.indexOf('-o');
const out = oi >= 0 ? args[oi + 1] : input.replace(/\.json$/, '') + '.collection.json';
fs.writeFileSync(out, JSON.stringify(build(spec), null, '\t') + '\n');
console.log(`✓ ${spec.scenes.length} scene${spec.scenes.length === 1 ? '' : 's'}, ${nItems} items → ${out}`);
console.log('  OBS: Scene Collection → Import, pick that file, then switch to it.');
}
