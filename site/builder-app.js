/*  The Broadcast Builder.

    Place sources on a 1920 × 1080 canvas, set the few properties that change
    how a thing reads, and export a scene collection OBS can import. What you
    get out is the real thing: the same source ids and setting keys the plugin
    registers, so the import builds native sources, not a picture of them.

    The canvas is drawn in CSS at 1920 × 1080 and scaled with a transform, so
    every position in the interface is already in canvas units and nothing has
    to be converted on the way out. That is the whole reason the maths here is
    boring.  */

import { PALETTE, START, byId, toCollection, CANVAS } from './builder.mjs';

const $ = (sel, root = document) => root.querySelector(sel);
const el = (tag, cls, text) => {
	const n = document.createElement(tag);
	if (cls) n.className = cls;
	if (text != null) n.textContent = text;
	return n;
};

const stage = $('#stage');
const canvas = $('#canvas');
const propsBox = $('#props');
const paletteBox = $('#palette');
const startBox = $('#start');

let items = [];
let selected = -1;
let snap = true;
let look = { accent: '#f5273f', scale: 1, tone: 'glass' };

/* ---- persistence ----------------------------------------------------------
   A builder that loses your layout on a refresh is a toy. */
const SAVE = 'sbk-builder-v1';
const save = () => {
	try {
		localStorage.setItem(SAVE, JSON.stringify({ items, look, name: $('#scene-name').value }));
	} catch {}
};
const load = () => {
	try {
		const raw = localStorage.getItem(SAVE);
		if (!raw) return false;
		const data = JSON.parse(raw);
		items = data.items || [];
		look = { ...look, ...(data.look || {}) };
		if (data.name) $('#scene-name').value = data.name;
		return items.length > 0;
	} catch {
		return false;
	}
};

/* ---- the canvas ----------------------------------------------------------- */

function fit() {
	const pad = 24;
	const scale = Math.min(
		(stage.clientWidth - pad) / CANVAS.w,
		(stage.clientHeight - pad) / CANVAS.h
	);
	canvas.style.transform = `scale(${scale})`;
	stage.style.setProperty('--canvas-scale', String(scale));
}

/* A schematic of each source: the right box in the right place, with enough of
   its character to be recognisable. Deliberately not a copy of the plugin's
   drawing — that would drift the first time a shader changed. */
function preview(def, item) {
	const s = { ...def.settings, ...(item.settings || {}) };
	const box = el('div', 'node__body');
	box.dataset.kind = def.id;

	switch (def.id) {
		case 'sbk_onair': {
			box.classList.add('pv-pill');
			box.append(el('i', 'pv-dot'), el('span', null, (s.word_off || 'Off air').toUpperCase()));
			if (s.shape === 'edge') box.classList.add('pv-edge');
			break;
		}
		case 'sbk_chip': {
			box.classList.add('pv-pill');
			box.append(el('i', 'pv-dot'), el('span', null, s.label || 'Chip'));
			if (s.value) box.append(el('b', 'pv-value', s.value));
			break;
		}
		case 'sbk_lower_third': {
			box.classList.add('pv-card', 'pv-lower');
			box.append(el('i', 'pv-bar'));
			const t = el('div');
			t.append(el('strong', null, s.name || 'Name'), el('span', null, s.title || ''));
			box.append(t);
			break;
		}
		case 'sbk_ticker': {
			box.classList.add('pv-card', 'pv-ticker');
			box.append(el('b', 'pv-tag', (s.tag || '').toUpperCase()), el('span', null, (s.text || '').replace(/\|/g, ' · ')));
			break;
		}
		case 'sbk_frame': {
			box.classList.add('pv-frame');
			if (s.label) box.append(el('b', 'pv-chip', s.label));
			break;
		}
		case 'sbk_card': {
			box.classList.add('pv-card', 'pv-bigcard');
			if (s.align === 'centre') box.classList.add('is-centre');
			if (s.eyebrow) box.append(el('b', 'pv-eyebrow', s.eyebrow));
			box.append(el('strong', null, s.title || 'Title'));
			if (s.body) box.append(el('span', null, s.body));
			break;
		}
		case 'sbk_backdrop':
			box.classList.add('pv-ground');
			box.dataset.mode = s.mode || 'solid';
			break;
		case 'sbk_visualizer':
			box.classList.add('pv-viz');
			box.dataset.style = s.style || 'bars';
			for (let i = 0; i < 28; i++) box.append(el('i'));
			break;
		case 'sbk_meter': {
			box.classList.add('pv-card', 'pv-meter');
			box.append(el('b', null, s.label || 'Mic'));
			const track = el('div', 'pv-track');
			for (let i = 0; i < 18; i++) track.append(el('i'));
			box.append(track);
			break;
		}
		case 'sbk_progress': {
			box.classList.add('pv-card', 'pv-progress');
			box.append(el('b', null, s.label || 'Goal'));
			const track = el('div', 'pv-track');
			const fillPct = Math.max(0, Math.min(100, ((s.value || 0) / (s.target || 1)) * 100));
			const fill = el('i');
			fill.style.width = fillPct + '%';
			track.append(fill);
			box.append(track);
			break;
		}
		case 'sbk_countdown': {
			box.classList.add('pv-card', 'pv-timer');
			if (s.style === 'ring' || s.style === 'ring-only') box.classList.add('is-ring');
			box.append(el('strong', null, s.style === 'ring-only' ? '' : '15:00'));
			break;
		}
		case 'sbk_clock':
			box.classList.add('pv-card', 'pv-timer');
			box.append(el('strong', null, '21:30'));
			break;
		case 'sbk_qr': {
			box.classList.add('pv-card', 'pv-qr');
			box.append(el('div', 'pv-code'), el('b', null, s.caption || ''));
			break;
		}
		case 'sbk_counter':
			box.classList.add('pv-card', 'pv-counter');
			box.append(el('b', null, s.label || 'Count'), el('strong', null, '—'));
			break;
		case 'sbk_stats':
			box.classList.add('pv-card', 'pv-stats');
			for (const row of ['Live for', 'Bitrate', 'Dropped']) {
				const r = el('div');
				r.append(el('b', null, row), el('span', null, '—'));
				box.append(r);
			}
			break;
		case 'sbk_plate':
			box.classList.add('pv-plate');
			break;
		case 'sbk_logo': {
			box.classList.add('pv-logo');
			const m = el('div', 'pv-mark');
			m.dataset.loop = s.loop || 'orbit';
			box.append(m);
			if (s.caption) box.append(el('b', null, s.caption));
			break;
		}
		case 'sbk_social': {
			box.classList.add('pv-card', 'pv-social');
			const mode = s.mode || 'rotate';
			box.classList.add('is-' + mode);
			const lines = String(s.accounts || '')
				.split('\n')
				.map((l) => l.trim())
				.filter(Boolean)
				.map((l) => {
					const i = l.indexOf(':');
					return i < 0 ? l : l.slice(i + 1).trim();
				});
			/* rotate shows one at a time, so the preview shows one — it should
			   read as the thing it will be, not as a list of everything */
			for (const handle of mode === 'rotate' ? lines.slice(0, 1) : lines.slice(0, 6)) {
				const row = el('div', 'pv-row');
				row.append(el('i', 'pv-glyph'), el('span', null, handle));
				box.append(row);
			}
			break;
		}
		case 'sbk_prompt': {
			box.classList.add('pv-card', 'pv-prompt');
			const first = String(s.lines || '').split('\n').map((l) => l.trim()).filter(Boolean)[0] || '';
			const afterMark = first.includes(':') ? first.slice(first.indexOf(':') + 1).trim() : first;
			const [title, body] = afterMark.split('|').map((x) => (x || '').trim());
			box.append(el('i', 'pv-glyph'));
			const t = el('div');
			t.append(el('strong', null, title || 'Enjoying this?'), el('span', null, body || ''));
			box.append(t);
			break;
		}
		case 'sbk_comments': {
			box.classList.add('pv-card', 'pv-comments');
			box.append(el('b', null, s.title || 'Questions'));
			const qs = String(s.manual || '').split('\n').map((l) => l.trim()).filter(Boolean);
			for (const q of qs.slice(0, Math.max(1, Number(s.show_count) || 3))) {
				const i = q.indexOf(':');
				const row = el('div', 'pv-q');
				const t = el('div');
				t.append(el('b', null, i < 0 ? '' : q.slice(0, i)), el('span', null, i < 0 ? q : q.slice(i + 1).trim()));
				row.append(el('i', 'pv-av'), t);
				box.append(row);
			}
			break;
		}
		default:
			box.classList.add('pv-card');
			box.append(el('span', null, def.name));
	}
	return box;
}

function sizeOf(def, item) {
	const s = { ...def.settings, ...(item.settings || {}) };
	let [w, h] = def.size;
	if (s.width) w = Number(s.width);
	if (s.height) h = Number(s.height);
	/* frame and plate share one aspect table, and the plugin's own numbers —
	   21:9 is 756 × 324, not a round figure, and guessing put a plate a shadow's
	   width away from the frame it belonged to */
	if (def.id === 'sbk_frame' || def.id === 'sbk_plate') {
		const ratios = { '16x9': [640, 360], '9x16': [360, 640], '1x1': [480, 480], '4x5': [432, 540], '4x3': [560, 420], '21x9': [756, 324] };
		const r = ratios[s.aspect] || [640, 360];
		w = r[0] * (s.size || 1);
		h = r[1] * (s.size || 1);
	}
	return [w, h];
}

function render() {
	canvas.replaceChildren();
	canvas.style.setProperty('--accent', look.accent);
	/* the plugin grows type, padding and radius from one number; so does this,
	   or the scale slider is a control that changes the export and nothing you
	   can see */
	canvas.style.setProperty('--look-scale', String(look.scale));
	canvas.dataset.tone = look.tone;

	items.forEach((item, i) => {
		const def = byId[item.id];
		if (!def) return;
		const [w, h] = sizeOf(def, item);
		const node = el('div', 'node');
		node.style.cssText = `left:${item.x}px;top:${item.y}px;width:${w}px;height:${h}px`;
		node.dataset.index = String(i);
		if (i === selected) node.classList.add('is-selected');
		if (def.ground) node.classList.add('is-ground');
		node.append(preview(def, item));
		node.append(el('span', 'node__tag', def.name));
		canvas.append(node);
	});
	renderProps();
	save();
}

/* ---- properties ------------------------------------------------------------ */

function renderProps() {
	propsBox.replaceChildren();
	if (selected < 0 || !items[selected]) {
		propsBox.append(el('p', 'muted', 'Pick something on the canvas, or add a piece from the left.'));
		return;
	}
	const item = items[selected];
	const def = byId[item.id];
	const s = { ...def.settings, ...(item.settings || {}) };

	const head = el('div', 'props__head');
	head.append(el('strong', null, def.name));
	const del = el('button', 'btn btn--ghost', 'Remove');
	del.onclick = () => {
		items.splice(selected, 1);
		selected = -1;
		render();
	};
	head.append(del);
	propsBox.append(head);

	for (const [key, label, type] of def.fields || []) {
		const row = el('label', 'field');
		row.append(el('span', null, label));
		let input;
		if (Array.isArray(type)) {
			input = el('select');
			for (const opt of type) {
				const o = el('option', null, opt === '' ? 'demo signal' : opt);
				o.value = opt;
				if (String(s[key] ?? '') === opt) o.selected = true;
				input.append(o);
			}
		} else if (type === 'bool') {
			input = el('input');
			input.type = 'checkbox';
			input.checked = !!s[key];
		} else {
			input = el('input');
			input.type = type === 'number' ? 'number' : 'text';
			input.value = s[key] ?? '';
		}
		input.oninput = () => {
			item.settings = item.settings || {};
			item.settings[key] =
				type === 'number' ? Number(input.value) : type === 'bool' ? input.checked : input.value;
			render();
		};
		row.append(input);
		propsBox.append(row);
	}

	const pos = el('div', 'field field--pair');
	for (const axis of ['x', 'y']) {
		const wrap = el('label');
		wrap.append(el('span', null, axis.toUpperCase()));
		const n = el('input');
		n.type = 'number';
		n.value = Math.round(item[axis]);
		n.oninput = () => {
			item[axis] = Number(n.value);
			render();
		};
		wrap.append(n);
		pos.append(wrap);
	}
	propsBox.append(pos);

	const order = el('div', 'props__order');
	const back = el('button', 'btn btn--ghost', 'Send back');
	back.onclick = () => {
		if (selected <= 0) return;
		[items[selected - 1], items[selected]] = [items[selected], items[selected - 1]];
		selected--;
		render();
	};
	const front = el('button', 'btn btn--ghost', 'Bring forward');
	front.onclick = () => {
		if (selected < 0 || selected >= items.length - 1) return;
		[items[selected + 1], items[selected]] = [items[selected], items[selected + 1]];
		selected++;
		render();
	};
	order.append(back, front);
	propsBox.append(order);
}

/* ---- palette --------------------------------------------------------------- */

function buildPalette() {
	const groups = new Map();
	for (const def of PALETTE) {
		if (!groups.has(def.group)) groups.set(def.group, []);
		groups.get(def.group).push(def);
	}
	for (const [group, defs] of groups) {
		paletteBox.append(el('h3', null, group));
		const grid = el('div', 'palette__grid');
		for (const def of defs) {
			const b = el('button', 'chip-btn', def.name);
			b.onclick = () => {
				const [w, h] = sizeOf(def, {});
				items.push({
					id: def.id,
					x: def.ground ? 0 : Math.round((CANVAS.w - w) / 2),
					y: def.ground ? 0 : Math.round((CANVAS.h - h) / 2),
				});
				/* a ground belongs at the back, or it hides everything placed before it */
				if (def.ground) {
					const added = items.pop();
					items.unshift(added);
					selected = 0;
				} else {
					selected = items.length - 1;
				}
				render();
			};
			grid.append(b);
		}
		paletteBox.append(grid);
	}

	for (const preset of START) {
		const b = el('button', 'chip-btn', preset.name);
		b.onclick = () => {
			items = preset.items.map((i) => ({ ...i, settings: { ...(i.settings || {}) } }));
			selected = -1;
			render();
		};
		startBox.append(b);
	}
}

/* ---- dragging -------------------------------------------------------------- */

let drag = null;

canvas.addEventListener('pointerdown', (e) => {
	const node = e.target.closest('.node');
	if (!node) return;
	const i = Number(node.dataset.index);
	selected = i;
	const scale = Number(getComputedStyle(stage).getPropertyValue('--canvas-scale')) || 1;
	drag = { i, startX: e.clientX, startY: e.clientY, ox: items[i].x, oy: items[i].y, scale };
	node.setPointerCapture(e.pointerId);
	render();
	e.preventDefault();
});

addEventListener('pointermove', (e) => {
	if (!drag) return;
	/* the canvas is transform-scaled, so a pixel of pointer movement is more
	   than a pixel of canvas; divide or everything lags behind the cursor */
	let x = drag.ox + (e.clientX - drag.startX) / drag.scale;
	let y = drag.oy + (e.clientY - drag.startY) / drag.scale;
	if (snap) {
		x = Math.round(x / 20) * 20;
		y = Math.round(y / 20) * 20;
	}
	items[drag.i].x = Math.round(x);
	items[drag.i].y = Math.round(y);
	render();
});

addEventListener('pointerup', () => (drag = null));

addEventListener('keydown', (e) => {
	if (selected < 0 || /^(INPUT|SELECT|TEXTAREA)$/.test(document.activeElement.tagName)) return;
	const step = e.shiftKey ? 20 : 2;
	const moves = { ArrowLeft: [-step, 0], ArrowRight: [step, 0], ArrowUp: [0, -step], ArrowDown: [0, step] };
	if (moves[e.key]) {
		items[selected].x += moves[e.key][0];
		items[selected].y += moves[e.key][1];
		render();
		e.preventDefault();
	} else if (e.key === 'Backspace' || e.key === 'Delete') {
		items.splice(selected, 1);
		selected = -1;
		render();
		e.preventDefault();
	}
});

/* ---- the toolbar ----------------------------------------------------------- */

/* OBS stores colours as 0xAABBGGRR, which is not what a colour input gives you */
function rgbToObs(hex) {
	const n = parseInt(hex.slice(1), 16);
	const r = (n >> 16) & 255, g = (n >> 8) & 255, b = n & 255;
	return (255 << 24 >>> 0) + (b << 16) + (g << 8) + r;
}

$('#accent').oninput = (e) => {
	look.accent = e.target.value;
	render();
};
$('#scale').oninput = (e) => {
	look.scale = Number(e.target.value);
	$('#scale-out').textContent = look.scale.toFixed(2) + '×';
	render();
};
$('#tone').onchange = (e) => {
	look.tone = e.target.value;
	render();
};
$('#snap').onchange = (e) => (snap = e.target.checked);
$('#clear').onclick = () => {
	if (!items.length || confirm('Clear the canvas?')) {
		items = [];
		selected = -1;
		render();
	}
};

$('#export').onclick = () => {
	const name = $('#scene-name').value.trim() || 'SBK Scene';
	const collection = toCollection(name, items, {
		accent: rgbToObs(look.accent),
		scale: look.scale,
		tone: look.tone,
	});
	const blob = new Blob([JSON.stringify(collection, null, 2)], { type: 'application/json' });
	const a = document.createElement('a');
	a.href = URL.createObjectURL(blob);
	a.download = name.replace(/[^\w -]+/g, '').replace(/\s+/g, '-') + '.json';
	a.click();
	setTimeout(() => URL.revokeObjectURL(a.href), 1000);
	const note = $('#export-note');
	note.textContent = `Saved ${a.download}. In OBS: Scene Collection → Import → pick that file.`;
	note.hidden = false;
};

/* ---- go -------------------------------------------------------------------- */

buildPalette();
if (!load()) items = START[1].items.map((i) => ({ ...i, settings: { ...(i.settings || {}) } }));
$('#accent').value = look.accent;
$('#scale').value = String(look.scale);
$('#scale-out').textContent = look.scale.toFixed(2) + '×';
$('#tone').value = look.tone;
render();
fit();
addEventListener('resize', fit);
