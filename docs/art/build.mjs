/*  The README artwork.

    GitHub strips <svg> out of markdown, so illustrations have to be files
    referenced as images. It does honour <picture> with a prefers-color-scheme
    media query, which is why every drawing here is emitted twice — once for a
    dark reader and once for a light one — rather than trying to find colours
    that survive both.

    No external fonts: an SVG loaded as an image cannot fetch one, so the type
    is set in the system stack. Everything is drawn with plain shapes and
    presentation attributes, because a sanitiser that strips a <style> block
    would otherwise take the whole design with it.

    node docs/art/build.mjs
*/

import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const OUT = path.resolve(path.dirname(fileURLToPath(import.meta.url)));

const SANS = "ui-sans-serif,system-ui,-apple-system,'Segoe UI',Roboto,Helvetica,Arial,sans-serif";
const MONO = "ui-monospace,SFMono-Regular,Menlo,Consolas,monospace";

const themes = {
	dark: {
		bg: '#0b0b0b', panel: '#141414', panel2: '#1c1c1c',
		line: '#2a2a2a', lineStrong: '#3a3a3a',
		ink: '#f6f6f6', dim: '#a8a8a8', faint: '#6f6f6f',
		accent: '#f5273f', ok: '#3fcf6a', shot: '#000',
	},
	light: {
		bg: '#fbfbfa', panel: '#ffffff', panel2: '#f2f2f0',
		line: '#e4e4e1', lineStrong: '#cfcfca',
		ink: '#16171a', dim: '#5b5c61', faint: '#8a8b90',
		accent: '#d81734', ok: '#1f9d51', shot: '#16171a',
	},
};

const esc = (s) => String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
const text = (x, y, s, { size = 16, weight = 400, fill, anchor = 'start', font = SANS, track = 0, op = 1 } = {}) =>
	`<text x="${x}" y="${y}" font-family="${font}" font-size="${size}" font-weight="${weight}" fill="${fill}" ` +
	`text-anchor="${anchor}" letter-spacing="${track}" opacity="${op}" dominant-baseline="middle">${esc(s)}</text>`;
const rect = (x, y, w, h, { r = 0, fill = 'none', stroke = 'none', sw = 1, op = 1, dash } = {}) =>
	`<rect x="${x}" y="${y}" width="${w}" height="${h}" rx="${r}" fill="${fill}" stroke="${stroke}" ` +
	`stroke-width="${sw}" opacity="${op}"${dash ? ` stroke-dasharray="${dash}"` : ''}/>`;
const circle = (cx, cy, r, { fill = 'none', stroke = 'none', sw = 1, op = 1 } = {}) =>
	`<circle cx="${cx}" cy="${cy}" r="${r}" fill="${fill}" stroke="${stroke}" stroke-width="${sw}" opacity="${op}"/>`;
const line = (x1, y1, x2, y2, { stroke, sw = 1, op = 1, dash } = {}) =>
	`<line x1="${x1}" y1="${y1}" x2="${x2}" y2="${y2}" stroke="${stroke}" stroke-width="${sw}" opacity="${op}"` +
	`${dash ? ` stroke-dasharray="${dash}"` : ''} stroke-linecap="round"/>`;

const svg = (w, h, body) =>
	`<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 ${w} ${h}" width="${w}" height="${h}" role="img">\n${body}\n</svg>\n`;

/* ---- the wordmark, reused by every drawing -------------------------------- */

function wordmark(t, x, y, scale = 1) {
	const s = (n) => n * scale;
	return [
		circle(x + s(22), y + s(22), s(21), { stroke: t.lineStrong, sw: s(2) }),
		circle(x + s(22), y + s(22), s(10), { fill: t.accent }),
		text(x + s(58), y + s(16), 'SBK', { size: s(30), weight: 700, fill: t.ink, track: s(-1) }),
		text(x + s(58), y + s(37), 'SWARNIL BROADCAST KIT', { size: s(10), weight: 600, fill: t.faint, track: s(2.2) }),
	].join('\n');
}

/* ---- 1. the banner --------------------------------------------------------- */

function banner(t) {
	const W = 1280, H = 420;
	const p = [];
	p.push(rect(0, 0, W, H, { fill: t.bg }));

	/* the kit's own grid backdrop, faded out before it reaches the words */
	p.push(`<defs><linearGradient id="fade" x1="0" y1="0" x2="1" y2="0">
		<stop offset="0" stop-color="#000" stop-opacity="0"/><stop offset="1" stop-color="#000" stop-opacity="1"/>
	</linearGradient><mask id="gridmask"><rect x="0" y="0" width="${W}" height="${H}" fill="url(#fade)"/></mask></defs>`);
	const grid = [];
	for (let x = 0; x <= W; x += 40) grid.push(line(x, 0, x, H, { stroke: t.lineStrong, sw: 1 }));
	for (let y = 0; y <= H; y += 40) grid.push(line(0, y, W, y, { stroke: t.lineStrong, sw: 1 }));
	p.push(`<g mask="url(#gridmask)">${grid.join('')}</g>`);

	p.push(wordmark(t, 72, 64, 1.25));

	p.push(text(72, 196, 'Overlays OBS draws itself.', { size: 46, weight: 700, fill: t.ink, track: -1.4 }));
	p.push(text(72, 240, 'A native plugin, not a browser source: seventeen sources, six filters,', { size: 17, fill: t.dim }));
	p.push(text(72, 266, 'two transitions, a control dock and a show of twenty scenes.', { size: 17, fill: t.dim }));

	const pills = ['Tally light', 'Level meter in dB', 'Live counters', 'QR', 'CRT filter', 'Voice chain'];
	let px = 72;
	for (const label of pills) {
		const w = label.length * 7.6 + 30;
		p.push(rect(px, 306, w, 30, { r: 15, fill: t.panel, stroke: t.line }));
		p.push(circle(px + 15, 321, 3.5, { fill: t.accent }));
		p.push(text(px + 26, 322, label, { size: 12.5, fill: t.dim, weight: 500 }));
		px += w + 8;
	}

	/* a miniature scene on the right, the pieces in the places they go */
	const bx = 812, by = 64, bw = 396, bh = 223;
	p.push(rect(bx - 1, by - 1, bw + 2, bh + 2, { r: 11, fill: t.shot, stroke: t.lineStrong }));
	p.push(rect(bx + 232, by + 96, 148, 84, { r: 8, stroke: t.dim, sw: 2, op: 0.65 }));
	p.push(rect(bx + 16, by + 132, 176, 44, { r: 7, fill: t.panel, stroke: t.line }));
	p.push(rect(bx + 24, by + 141, 4, 26, { r: 2, fill: t.accent }));
	p.push(text(bx + 36, by + 151, 'Swarnil Singhai', { size: 12, weight: 600, fill: '#f6f6f6' }));
	p.push(text(bx + 36, by + 166, 'imswarnil.com', { size: 9, fill: '#a8a8a8' }));
	p.push(rect(bx + 286, by + 14, 94, 24, { r: 12, fill: t.panel, stroke: t.line }));
	p.push(circle(bx + 300, by + 26, 4, { fill: t.accent }));
	p.push(text(bx + 311, by + 27, 'OFF AIR', { size: 9.5, weight: 600, fill: '#f6f6f6', track: 0.8 }));
	p.push(rect(bx + 10, by + 190, bw - 20, 22, { r: 6, fill: t.panel, stroke: t.line }));
	p.push(rect(bx + 18, by + 196, 34, 10, { r: 5, fill: t.accent }));
	for (let i = 0; i < 3; i++)
		p.push(text(bx + 62 + i * 108, by + 202, 'Building live', { size: 9, fill: '#a8a8a8' }));
	for (let i = 0; i < 40; i++) {
		const h = 4 + ((i * 7919) % 17);
		p.push(rect(bx + 12 + i * 9.4, by + 178 - h, 5, h, { r: 1.5, fill: t.accent, op: 0.5 }));
	}
	p.push(text(bx, by + bh + 26, 'A scene the plugin builds, drawn by OBS itself', { size: 12, fill: t.faint }));

	return svg(W, H, p.join('\n'));
}

/* ---- 2. what a page cannot reach ------------------------------------------- */

function native(t) {
	const W = 1280, H = 470;
	const rows = [
		['Know you are live', 'OBS streaming and recording state'],
		['Hear the program mix', 'Every source and filter, not just a mic'],
		['See dropped frames', 'Congestion and the render rate'],
		['Be a transition', 'Composited between two scene textures'],
		['Filter the picture', 'Rounded corners, a grade, a CRT'],
		['Filter the voice', 'Compressor, gate, limiter'],
	];
	const p = [rect(0, 0, W, H, { fill: t.bg })];
	p.push(text(64, 52, 'What a page in a Browser Source cannot reach', { size: 26, weight: 700, fill: t.ink, track: -0.6 }));
	p.push(text(64, 84, 'These are not stylistic preferences. They are the reason the kit is a plugin.', { size: 15, fill: t.dim }));

	const colW = 556, gap = 40, top = 118;
	const cols = [
		{ x: 64, title: 'Browser source', sub: 'a page in a hidden Chromium', ok: false },
		{ x: 64 + colW + gap, title: 'Native plugin', sub: 'drawn by libobs', ok: true },
	];

	for (const col of cols) {
		p.push(rect(col.x, top, colW, 300, { r: 14, fill: t.panel, stroke: col.ok ? t.accent : t.line, sw: col.ok ? 1.5 : 1 }));
		p.push(text(col.x + 24, top + 34, col.title, { size: 17, weight: 600, fill: t.ink }));
		p.push(text(col.x + 24, top + 56, col.sub, { size: 12.5, fill: t.faint }));
		p.push(line(col.x + 24, top + 76, col.x + colW - 24, top + 76, { stroke: t.line }));

		rows.forEach((row, i) => {
			const y = top + 104 + i * 33;
			if (col.ok) {
				p.push(circle(col.x + 32, y, 8, { fill: t.ok, op: 0.16 }));
				p.push(`<path d="M${col.x + 28} ${y} l3 3 l5.5 -6" fill="none" stroke="${t.ok}" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>`);
			} else {
				p.push(circle(col.x + 32, y, 8, { fill: t.faint, op: 0.14 }));
				p.push(`<path d="M${col.x + 28.5} ${y - 3.5} l7 7 M${col.x + 35.5} ${y - 3.5} l-7 7" fill="none" stroke="${t.faint}" stroke-width="2" stroke-linecap="round"/>`);
			}
			p.push(text(col.x + 52, y, row[0], { size: 14, weight: col.ok ? 500 : 400, fill: col.ok ? t.ink : t.faint }));
			p.push(text(col.x + colW - 24, y, row[1], { size: 11.5, fill: t.faint, anchor: 'end', op: col.ok ? 1 : 0.7 }));
		});
	}

	p.push(text(64, 448, 'And a Chromium process per overlay, against one or two draw calls per source.', { size: 13, fill: t.faint }));
	return svg(W, H, p.join('\n'));
}

/* ---- 3. the anatomy of a scene --------------------------------------------- */

function anatomy(t) {
	const W = 1280, H = 700;
	const p = [rect(0, 0, W, H, { fill: t.bg })];
	p.push(text(64, 50, 'Anatomy of a scene', { size: 26, weight: 700, fill: t.ink, track: -0.6 }));
	p.push(text(64, 80, 'Every piece is an ordinary OBS source. Select it, open Properties, change anything.', { size: 15, fill: t.dim }));

	const cx = 232, cy = 128, cw = 816, ch = 459; /* 16:9 */
	p.push(rect(cx - 1, cy - 1, cw + 2, ch + 2, { r: 13, fill: t.shot, stroke: t.lineStrong }));

	/* backdrop grid */
	const g = [];
	for (let x = cx; x < cx + cw; x += 34) g.push(line(x, cy, x, cy + ch, { stroke: '#ffffff', sw: 1, op: 0.05 }));
	for (let y = cy; y < cy + ch; y += 34) g.push(line(cx, y, cx + cw, y, { stroke: '#ffffff', sw: 1, op: 0.05 }));
	p.push(g.join(''));

	const marks = [];
	const mark = (x, y, label, side) => marks.push({ x, y, label, side });

	/* the light */
	p.push(rect(cx + 28, cy + 26, 116, 30, { r: 15, fill: '#0c0c0cc7', stroke: '#ffffff24' }));
	p.push(circle(cx + 46, cy + 41, 5, { fill: t.accent }));
	p.push(text(cx + 60, cy + 42, 'OFF AIR', { size: 11, weight: 600, fill: '#f6f6f6', track: 0.9 }));
	mark(cx + 28, cy + 41, 'Light', 'left');

	/* the card */
	p.push(rect(cx + 28, cy + 96, 430, 150, { r: 12, fill: '#0c0c0cc7', stroke: '#ffffff24' }));
	p.push(circle(cx + 50, cy + 124, 4, { fill: t.accent }));
	p.push(text(cx + 62, cy + 125, 'STARTING SOON', { size: 10, weight: 600, fill: t.accent, track: 1.2 }));
	p.push(text(cx + 50, cy + 160, 'Building a Salesforce app live', { size: 24, weight: 700, fill: '#f6f6f6', track: -0.6 }));
	p.push(text(cx + 50, cy + 190, 'Grab a coffee. We begin at the top of the hour.', { size: 12, fill: '#a8a8a8' }));
	for (let i = 0; i < 3; i++) {
		const w = [78, 132, 96][i];
		const ox = [50, 136, 276][i];
		p.push(rect(cx + ox, cy + 208, w, 22, { r: 11, fill: '#ffffff1a', stroke: '#ffffff24' }));
	}
	mark(cx + 28, cy + 170, 'Card', 'left');

	/* the timer ring */
	p.push(circle(cx + 654, cy + 172, 62, { stroke: '#ffffff1f', sw: 9 }));
	p.push(`<path d="M ${cx + 654} ${cy + 110} A 62 62 0 1 1 ${cx + 594} ${cy + 189}" fill="none" stroke="${t.accent}" stroke-width="9" stroke-linecap="round"/>`);
	p.push(text(cx + 654, cy + 173, '14:59', { size: 24, weight: 700, fill: '#f6f6f6', font: MONO, anchor: 'middle' }));
	mark(cx + 716, cy + 172, 'Timer', 'right');

	/* the cam frame */
	p.push(rect(cx + 556, cy + 250, 216, 114, { r: 9, stroke: '#ffffffa0', sw: 2.5 }));
	p.push(rect(cx + 566, cy + 338, 74, 18, { r: 9, fill: '#0c0c0cc7', stroke: '#ffffff24' }));
	p.push(text(cx + 576, cy + 348, '@imswarnil', { size: 8.5, fill: '#f6f6f6' }));
	mark(cx + 772, cy + 284, 'Cam frame', 'right');

	/* the visualiser */
	for (let i = 0; i < 58; i++) {
		const h = 6 + ((i * 6151) % 24);
		p.push(rect(cx + 24 + i * 13.3, cy + 402 - h, 7, h, { r: 2, fill: t.accent, op: 0.55 }));
	}
	mark(cx + 24, cy + 392, 'Visualizer', 'left');

	/* the ticker */
	p.push(rect(cx + 14, cy + 414, cw - 28, 30, { r: 8, fill: '#0c0c0cc7', stroke: '#ffffff24' }));
	p.push(rect(cx + 26, cy + 422, 44, 14, { r: 7, fill: t.accent }));
	p.push(text(cx + 34, cy + 430, 'NOW', { size: 8, weight: 700, fill: '#fff', track: 0.6 }));
	for (let i = 0; i < 4; i++) {
		p.push(text(cx + 84 + i * 176, cy + 430, 'Questions in chat', { size: 10, fill: '#a8a8a8' }));
		if (i < 3) p.push(circle(cx + 196 + i * 176, cy + 430, 2.5, { fill: t.accent }));
	}
	mark(cx + 14, cy + 429, 'Ticker', 'left');

	/* the callouts */
	for (const m of marks) {
		const toX = m.side === 'left' ? 200 : cx + cw + 32;
		p.push(line(m.x, m.y, toX + (m.side === 'left' ? 8 : -8), m.y, { stroke: t.lineStrong, sw: 1, dash: '3 4' }));
		p.push(circle(m.x, m.y, 3, { fill: t.accent }));
		p.push(text(toX, m.y, m.label, { size: 13, weight: 500, fill: t.ink, anchor: m.side === 'left' ? 'end' : 'start' }));
	}

	p.push(text(64, 632, 'The camera goes under the frame; the kit draws the treatment, never the capture.', { size: 13, fill: t.faint }));
	p.push(text(64, 656, 'A backdrop, a light, a card, a timer, a frame, a visualizer and a ticker — seven sources.', { size: 13, fill: t.faint }));
	return svg(W, H, p.join('\n'));
}

/* ---- write ----------------------------------------------------------------- */

const drawings = { banner, native, anatomy };
let n = 0;
for (const [name, fn] of Object.entries(drawings)) {
	for (const [mode, t] of Object.entries(themes)) {
		const file = path.join(OUT, `${name}-${mode}.svg`);
		fs.writeFileSync(file, fn(t));
		n++;
	}
}
console.log(`${n} drawings → docs/art/`);
