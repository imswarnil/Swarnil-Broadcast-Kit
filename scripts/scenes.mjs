/* Scenes — generates the OBS scene collection from scenes/scenes.config.mjs
   and the registry, so a new overlay reaches the collection by editing one
   list. Writes scenes/Tally.json; `npm run check` fails if it is stale. */

import path from 'node:path';
import { ROOT, write, log } from './lib.mjs';
import { collection } from '../scenes/scenes.config.mjs';
import { bySlug, overlayUrl } from '../overlays/registry.mjs';

export const PUBLIC_BASE = 'https://obs.imswarnil.com/';

export function render() {
	const sources = [];
	const seen = new Set();

	function browserSource(item) {
		const o = bySlug[item.overlay];
		if (!o) throw new Error(`scenes.config: unknown overlay "${item.overlay}"`);
		const name = item.name || `Tally · ${o.name}`;
		if (seen.has(name)) return name;
		seen.add(name);
		sources.push({
			id: 'browser_source',
			versioned_id: 'browser_source',
			name,
			enabled: true,
			muted: false,
			volume: 1.0,
			balance: 0.5,
			mixers: 0,
			monitoring_type: 0,
			sync: 0,
			flags: 0,
			deinterlace_mode: 0,
			deinterlace_field_order: 0,
			hotkeys: {},
			private_settings: {},
			'push-to-mute': false,
			'push-to-mute-delay': 0,
			'push-to-talk': false,
			'push-to-talk-delay': 0,
			settings: {
				url: overlayUrl(PUBLIC_BASE, o.slug, item.params || {}),
				width: item.size?.w ?? o.size.w,
				height: item.size?.h ?? o.size.h,
				css: '',
				fps_custom: false,
				reroute_audio: false,
				restart_when_active: false,
				shutdown: false,
				webpage_control_level: 1,
			},
		});
		return name;
	}

	const sceneNames = [];
	for (const scene of collection.scenes) {
		const items = scene.items.map((item, i) => {
			const name = browserSource(item);
			const o = bySlug[item.overlay];
			return {
				id: i + 1,
				name,
				visible: item.visible ?? true,
				locked: false,
				align: 5,
				pos: { x: item.pos?.x ?? 0, y: item.pos?.y ?? 0 },
				rot: 0.0,
				scale: { x: 1.0, y: 1.0 },
				bounds: { x: item.size?.w ?? o.size.w, y: item.size?.h ?? o.size.h },
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
		});
		sceneNames.push(scene.name);
		sources.push({
			id: 'scene',
			versioned_id: 'scene',
			name: scene.name,
			enabled: true,
			muted: false,
			volume: 1.0,
			balance: 0.5,
			mixers: 0,
			monitoring_type: 0,
			sync: 0,
			flags: 0,
			hotkeys: {},
			private_settings: {},
			settings: { custom_size: false, id_counter: items.length, items },
		});
	}

	return {
		name: collection.name,
		current_scene: sceneNames[0],
		current_program_scene: sceneNames[0],
		current_transition: 'Fade',
		transition_duration: 300,
		scene_order: sceneNames.map((name) => ({ name })),
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

export const OUT = path.join(ROOT, 'scenes', `${collection.name}.json`);
export const json = () => JSON.stringify(render(), null, 2) + '\n';

if (process.argv[1] && path.resolve(process.argv[1]) === new URL(import.meta.url).pathname) {
	write(OUT, json());
	log(`scenes/${collection.name}.json`);
}
