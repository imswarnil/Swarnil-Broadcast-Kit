/* The site — obs.imswarnil.com. A catalogue, a page per overlay with a
   live preview and a URL builder, an install guide, a scenes page. Plain
   template functions; the registry supplies every fact. */

import path from 'node:path';
import fs from 'node:fs';
import { OVERLAYS, GLOBAL_PARAMS, overlayUrl } from '../overlays/registry.mjs';
import { collection } from '../scenes/scenes.config.mjs';

const esc = (s) => String(s ?? '').replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));
const KINDS = { component: 'Component', visualizer: 'Visualizer', scene: 'Scene' };

export function buildSite({ dist, base, version }) {
	const here = path.dirname(new URL(import.meta.url).pathname);
	const out = (p, html) => { fs.mkdirSync(path.dirname(path.join(dist, p)), { recursive: true }); fs.writeFileSync(path.join(dist, p), html); };
	fs.copyFileSync(path.join(here, 'site.css'), path.join(dist, 'site.css'));
	fs.copyFileSync(path.join(here, 'site.js'), path.join(dist, 'site.js'));
	fs.writeFileSync(path.join(dist, 'registry.json'), JSON.stringify({ base, version, globals: GLOBAL_PARAMS, overlays: OVERLAYS }, null, 2));

	const ctx = { base, version };
	const pages = [
		['index.html', home(ctx)],
		['install/index.html', install(ctx)],
		['scenes/index.html', scenes(ctx)],
		['404.html', notFound(ctx)],
		...OVERLAYS.map((o) => [`docs/${o.slug}/index.html`, overlay(ctx, o)]),
	];
	for (const [p, html] of pages) out(p, html);
	out('sitemap.xml', `<?xml version="1.0" encoding="UTF-8"?>\n<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">\n${pages.filter(([p]) => p !== '404.html').map(([p]) => `  <url><loc>${base}${p.replace(/index\.html$/, '')}</loc></url>`).join('\n')}\n</urlset>\n`);
	return pages.length;
}

function shell({ base, version }, { title, description, body, current = '', head = '' }) {
	const nav = [['Overlays', '/'], ['Install', '/install/'], ['Scenes', '/scenes/']];
	return `<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>${esc(title)} · Tally</title>
<meta name="description" content="${esc(description)}">
<meta property="og:title" content="${esc(title)} · Tally">
<meta property="og:description" content="${esc(description)}">
<meta name="color-scheme" content="light dark">
<link rel="icon" href="/assets/mark.svg" type="image/svg+xml">
<link rel="stylesheet" href="/assets/tally.css">
<link rel="stylesheet" href="/site.css">
<script type="module" src="/site.js"></script>
${head}
</head>
<body class="site">
<header class="site-bar">
	<a class="site-brand" href="/"><span class="tally-brand"><b>Tally</b><i class="tally-dot" data-pulse></i></span><span class="site-brand__by">for OBS Studio</span></a>
	<nav class="site-nav" aria-label="Site">
		${nav.map(([l, h]) => `<a href="${h}"${current === h ? ' aria-current="page"' : ''}>${l}</a>`).join('')}
		<a href="https://github.com/imswarnil/Tally" rel="noopener">GitHub</a>
	</nav>
</header>
<main class="site-main">
${body}
</main>
<footer class="site-foot">
	<p>Tally ${esc(version)} · MIT · Built by <a href="https://imswarnil.com">Swarnil Singhai</a>, in the language of the <a href="https://design.imswarnil.com">Im Design System</a>.</p>
	<p><a href="https://github.com/imswarnil/Tally/releases">Releases</a> · <a href="https://github.com/imswarnil/Tally/issues">Issues</a> · <a href="/registry.json">registry.json</a></p>
</footer>
</body>
</html>
`;
}

const preview = (o, values, extra = '') => `<div class="preview" style="--pw:${o.size.w};--ph:${o.size.h}" data-preview ${extra}><iframe src="${overlayUrl('/', o.slug, values)}" width="${o.size.w}" height="${o.size.h}" title="${esc(o.name)} preview" loading="lazy"></iframe></div>`;

function home(ctx) {
	const groups = ['component', 'visualizer', 'scene'];
	return shell(ctx, {
		title: 'Overlays for OBS Studio',
		description: 'Tally — stream overlays, audio visualizers and full scenes for OBS Studio. Add a URL as a Browser Source; nothing to install.',
		current: '/',
		body: `
<section class="hero">
	<p class="eyebrow"><i class="tally-dot"></i> Open source · MIT</p>
	<h1>Overlays for OBS Studio that look like they belong together.</h1>
	<p class="lede">Lower thirds, an on-air light that reads OBS's real state, audio visualizers, and full starting-soon and break scenes. Every one is a URL: add it as a Browser Source, tune it with a few parameters, done. One accent, one type, one gap, so a stream reads as one design.</p>
	<p class="actions"><a class="btn btn--primary" href="/install/">Install in two minutes</a><a class="btn" href="/scenes/">Import the scene collection</a></p>
</section>
${groups.map((k) => `
<section class="catalogue">
	<h2>${KINDS[k]}s</h2>
	<div class="cards">
		${OVERLAYS.filter((o) => o.kind === k).map((o) => `
		<a class="card" href="/docs/${o.slug}/">
			${preview(o, { ...o.example, demo: 1 })}
			<span class="card__body"><strong>${esc(o.name)}</strong><span>${esc(o.description)}</span><code>${o.size.w}×${o.size.h}</code></span>
		</a>`).join('')}
	</div>
</section>`).join('')}
<section class="strip">
	<h2>How it holds together</h2>
	<div class="three">
		<div><h3>One runtime</h3><p>Every overlay loads the same 12 KB of CSS and JS. Change the accent once with <code>?accent=</code> and every overlay on the scene follows.</p></div>
		<div><h3>OBS knows it is there</h3><p>Inside a Browser Source, Tally listens to OBS's own events. The tally light turns red when you go live, not when you remember to click something.</p></div>
		<div><h3>Yours to change</h3><p>Plain HTML and CSS under MIT. Fork it, restyle it, or point the scene collection at your own copy.</p></div>
	</div>
</section>`,
	});
}

function overlay(ctx, o) {
	const all = [...o.params, ...GLOBAL_PARAMS];
	const field = (p) => {
		const id = `p-${p.key}`;
		let input;
		switch (p.type) {
			case 'select': input = `<select id="${id}" name="${p.key}">${p.options.map((v) => `<option value="${esc(v)}"${String(v) === String(p.default) ? ' selected' : ''}>${v === '' ? 'default' : esc(v)}</option>`).join('')}</select>`; break;
			case 'toggle': input = `<input type="checkbox" id="${id}" name="${p.key}" value="1"${p.default ? ' checked' : ''}>`; break;
			case 'color': input = `<span class="color"><input type="color" id="${id}" name="${p.key}" value="${esc(p.default)}"><input type="text" name="${p.key}" value="${esc(p.default)}" data-mirror="${id}" aria-label="${esc(p.label)} as text"></span>`; break;
			case 'number': input = `<input type="number" id="${id}" name="${p.key}" value="${esc(p.default)}" min="${p.min ?? ''}" max="${p.max ?? ''}" step="${p.step ?? 'any'}">`; break;
			default: input = `<input type="text" id="${id}" name="${p.key}" value="${esc(p.default)}">`;
		}
		return `<div class="field${p.type === 'toggle' ? ' field--toggle' : ''}"><label for="${id}">${esc(p.label)}</label>${input}${p.hint ? `<small>${esc(p.hint)}</small>` : ''}</div>`;
	};
	return shell(ctx, {
		title: o.name,
		description: o.description,
		body: `
<nav class="crumbs" aria-label="Breadcrumb"><a href="/">Overlays</a> / <span>${esc(o.name)}</span></nav>
<section class="doc-head">
	<p class="eyebrow">${KINDS[o.kind]} · ${o.size.w}×${o.size.h}</p>
	<h1>${esc(o.name)}</h1>
	<p class="lede">${esc(o.description)}</p>
</section>
<section class="builder" data-builder="${o.slug}" data-base="${esc(ctx.base)}" data-preview-demo="${esc(JSON.stringify({ ...o.example, demo: 1 }))}">
	<div class="builder__stage">
		${preview(o, { ...o.example, demo: 1 }, 'data-builder-preview')}
		<div class="url">
			<label for="url-out">Browser Source URL</label>
			<div class="url__row"><input id="url-out" type="text" readonly value="${esc(overlayUrl(ctx.base, o.slug, {}))}"><button type="button" class="btn btn--primary" data-copy="#url-out">Copy</button></div>
			<p class="url__hint">In OBS: Sources → + → Browser → paste this URL, set width <b>${o.size.w}</b> and height <b>${o.size.h}</b>. <a href="/install/">Full steps.</a></p>
		</div>
	</div>
	<form class="builder__form" data-builder-form>
		<h2>Tune it</h2>
		${o.params.map(field).join('')}
		<h2>Every overlay</h2>
		${GLOBAL_PARAMS.map(field).join('')}
		<p class="hint">Only values that differ from the default go into the URL.</p>
	</form>
</section>
<section class="doc-params">
	<h2>Parameters</h2>
	<table>
		<thead><tr><th>Key</th><th>Default</th><th>What it does</th></tr></thead>
		<tbody>${all.map((p) => `<tr><td><code>${p.key}</code></td><td><code>${esc(p.default === '' ? '—' : p.default)}</code></td><td>${esc(p.label)}${p.options ? ` — <code>${p.options.filter(Boolean).join('</code> · <code>')}</code>` : ''}${p.hint ? `. ${esc(p.hint)}` : ''}</td></tr>`).join('')}
		<tr><td><code>hidden</code></td><td><code>—</code></td><td>Start hidden; <code>Tally.show()</code> from the page's console or a custom script reveals it.</td></tr>
		</tbody>
	</table>
</section>
<section class="doc-source">
	<h2>Source</h2>
	<p>The page is <a href="https://github.com/imswarnil/Tally/blob/main/overlays/${o.slug}/index.html">overlays/${o.slug}/index.html</a>; its styles live in <code>src/</code>. Copy the file, keep the two <code>assets/</code> links, and you have your own.</p>
</section>`,
	});
}

function install(ctx) {
	return shell(ctx, {
		title: 'Install',
		description: 'How to add a Tally overlay to OBS Studio as a Browser Source, import the scene collection, and use the pack offline.',
		current: '/install/',
		body: `
<section class="doc-head"><p class="eyebrow">Install</p><h1>Two minutes, no download.</h1><p class="lede">Tally overlays are web pages. OBS renders web pages natively through its Browser Source, so an overlay is a URL and nothing else.</p></section>
<section class="prose">
<h2>1. One overlay as a Browser Source</h2>
<ol>
	<li>Open any overlay on this site, tune it, and press <b>Copy</b> under the preview.</li>
	<li>In OBS: <b>Sources → + → Browser</b>. Name it, press OK.</li>
	<li>Paste the URL. Set <b>Width</b> and <b>Height</b> to the numbers the page shows (most are 1920 × 1080 — the overlay positions itself on the canvas).</li>
	<li>Leave "Custom CSS" as it is. Tick <b>Shutdown source when not visible</b> if you like; Tally replays its entrance every time.</li>
</ol>
<p>Drag it into place if it is smaller than the canvas. That is the whole install.</p>

<h2>2. The whole scene collection</h2>
<p>Four scenes — Starting soon, Live, Be right back, Ending — already wired to hosted overlays. <b>Scene Collection → Import</b>, choose <a href="/scenes/Tally.json" download>Tally.json</a>, then switch to it. Edit any source's URL to put your own name in. <a href="/scenes/">Details.</a></p>

<h2>3. The profile</h2>
<p>A 1080p60 profile with sane simple-output settings. <b>Profile → Import</b>, pick the <code>Tally</code> folder from the <a href="https://github.com/imswarnil/Tally/releases">release zip</a>. A profile carries no stream key; add yours under Settings → Stream.</p>

<h2>4. Offline</h2>
<p>The <a href="https://github.com/imswarnil/Tally/releases">release zip</a> holds every overlay page, the runtime and the fonts. In a Browser Source tick <b>Local file</b> and pick a page. OBS offers no query string for a local file, so edit the <code>data-tally-default</code> attributes in the HTML instead, or keep using hosted URLs.</p>

<h2>Audio for the visualizers</h2>
<p>A Browser Source can hear the machine's <b>default input device</b> through <code>getUserMedia</code>. Set the device you want — the microphone, or a virtual cable carrying your desktop mix — as the system default, and the bars follow it. If OBS grants no device, Tally falls back to a generated demo signal so the scene never sits still; force that with <code>?source=demo</code>.</p>

<h2>The on-air light</h2>
<p>Inside OBS the light reads the program's real state: red for streaming, orange for recording, grey for neither. It needs the Browser Source's page permission at its default ("Read access to OBS status information") or higher. Outside OBS, <code>?demo=1</code> cycles the states so you can see it move.</p>

<h2>Change everything at once</h2>
<p>Four parameters work on every overlay: <code>accent</code>, <code>scale</code>, <code>tone</code> and <code>font</code>. Put the same <code>?accent=00a3ff</code> on each source and the scene changes colour together.</p>
</section>`,
	});
}

function scenes(ctx) {
	return shell(ctx, {
		title: 'Scene collection',
		description: 'The Tally scene collection and profile for OBS Studio: four scenes wired to the hosted overlays.',
		current: '/scenes/',
		body: `
<section class="doc-head"><p class="eyebrow">Scenes</p><h1>Four scenes, wired.</h1><p class="lede">Import once, then rename, re-word and reposition in OBS as you would any source. The collection references the hosted overlays, so it stays current as Tally improves.</p>
<p class="actions"><a class="btn btn--primary" href="/scenes/Tally.json" download>Download Tally.json</a><a class="btn" href="/scenes/profile/Tally/basic.ini" download="basic.ini">Profile basic.ini</a></p></section>
<section class="prose">
${collection.scenes.map((s) => `<h2>${esc(s.name)}</h2><ul>${s.items.map((i) => { const o = OVERLAYS.find((x) => x.slug === i.overlay); return `<li><a href="/docs/${o.slug}/">${esc(i.name || `Tally · ${o.name}`)}</a> — ${i.size?.w ?? o.size.w}×${i.size?.h ?? o.size.h} at ${i.pos?.x ?? 0},${i.pos?.y ?? 0}${i.params && Object.keys(i.params).length ? ` <code>${esc(new URLSearchParams(i.params).toString())}</code>` : ''}</li>`; }).join('')}</ul>`).join('')}
<h2>Importing</h2>
<ol><li><b>Scene Collection → Import</b>, choose the JSON, press Import.</li><li><b>Scene Collection → Tally</b> to switch to it.</li><li>Double-click a source to change its URL — put your own name on the lower third, your own handle on the chips.</li></ol>
<p>The profile is a folder named <code>Tally</code> holding <code>basic.ini</code>: 1920×1080 at 60 fps, simple output at 6000 kbps, MKV recording. <b>Profile → Import</b> and pick the folder.</p>
<h2>Generated, not hand-made</h2>
<p>The JSON is produced by <code>npm run scenes</code> from <a href="https://github.com/imswarnil/Tally/blob/main/scenes/scenes.config.mjs">scenes/scenes.config.mjs</a>. To ship a different arrangement, edit that list and regenerate.</p>
</section>`,
	});
}

function notFound(ctx) {
	return shell(ctx, { title: 'Not found', description: 'That page is not here.', body: `<section class="doc-head"><p class="eyebrow">404</p><h1>Off air.</h1><p class="lede">That page is not here. <a href="/">Back to the overlays.</a></p></section>` });
}
