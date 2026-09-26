/*  The docs site at obs.imswarnil.com.

    Plain Node, no dependencies, no install step: `node site/build.mjs` writes
    dist/ and that is the whole build. The content lives in site/content.mjs and
    the screenshots in docs/screens/ are real frames off the self-test, so the
    pictures on the site are the scenes the plugin actually builds.  */

import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { NAME, SHORT, REPO, BUILT_FOR, SOURCES, SCENES, STEPS, APIS, FAQ } from './content.mjs';
import { PRESETS, COMBOS } from './presets.mjs';
import { build as buildCollection, validate as validateSpec } from '../skills/sbk-scenes/scripts/build-collection.mjs';
import { icons, mark } from './icons.mjs';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const DIST = path.join(ROOT, 'dist');
const BASE = (process.env.SITE_URL || 'https://obs.imswarnil.com/').replace(/\/?$/, '/');

const write = (p, s) => {
	fs.mkdirSync(path.dirname(p), { recursive: true });
	fs.writeFileSync(p, s);
};
const copy = (from, to) => {
	fs.mkdirSync(path.dirname(to), { recursive: true });
	fs.copyFileSync(from, to);
};
const isFilter = (s) => /filter/i.test(s.tag);
const hasShot = (img) => fs.existsSync(path.join(ROOT, 'docs/screens', `${img}.jpg`));
const esc = (s) => String(s).replace(/&(?![a-z#]+;)/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
/* the content file writes prose with inline markup already in it */
const prose = (s) => String(s).replace(/\s+/g, ' ').trim();

const NAV = [
	['/', 'Overview', 'overview'],
	['/make/', 'Make', 'make'],
	['/scenes/', 'Scenes', 'scenes'],
	['/sources/', 'Sources', 'sources'],
	['/builder/', 'Builder', 'builder'],
	['/docs/', 'Docs', 'docs'],
];

function page({ url, title, description, body, script }) {
	const full = url === '/' ? `${NAME} — native overlays for OBS Studio` : `${title} · ${SHORT}`;
	return `<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>${esc(full)}</title>
<meta name="description" content="${esc(description)}">
<meta name="color-scheme" content="dark light">
<meta property="og:title" content="${esc(full)}">
<meta property="og:description" content="${esc(description)}">
<meta property="og:type" content="website">
<meta property="og:url" content="${BASE.replace(/\/$/, '')}${url}">
<meta property="og:image" content="${BASE}screens/live.jpg">
<meta name="twitter:card" content="summary_large_image">
<meta name="twitter:image" content="${BASE}screens/live.jpg">
<meta property="og:site_name" content="${esc(NAME)}">
<meta property="og:locale" content="en_GB">
<meta name="author" content="Swarnil Singhai">
<meta name="keywords" content="OBS Studio plugin, OBS overlays, stream overlays, OBS scene collection, native OBS plugin, libobs, lower third, stream deck alternative, OBS transition, OBS filter, twitch overlay, youtube live overlay, macOS OBS plugin, open source stream kit">
<link rel="canonical" href="${BASE.replace(/\/$/, '')}${url}">
<script type="application/ld+json">${JSON.stringify({
		'@context': 'https://schema.org',
		'@type': 'SoftwareApplication',
		name: NAME,
		alternateName: SHORT,
		applicationCategory: 'MultimediaApplication',
		applicationSubCategory: 'OBS Studio plugin',
		operatingSystem: 'macOS (Apple Silicon)',
		description,
		url: BASE,
		image: `${BASE}screens/live.jpg`,
		license: 'https://opensource.org/licenses/MIT',
		isAccessibleForFree: true,
		offers: { '@type': 'Offer', price: '0', priceCurrency: 'USD' },
		author: { '@type': 'Person', name: 'Swarnil Singhai', url: 'https://imswarnil.com' },
		codeRepository: REPO,
		softwareHelp: `${BASE}docs/`,
	})}</script>
<link rel="stylesheet" href="/site.css">
<link rel="icon" href="/icon.svg" type="image/svg+xml">
<script>
/* Before the first paint, or the page flashes the other theme on every
   navigation. No choice stored means follow the system, which the CSS does on
   its own — so the attribute is only set when the reader has actually picked. */
try { var t = localStorage.getItem("sbk-theme"); if (t === "light" || t === "dark") document.documentElement.dataset.theme = t; } catch (e) {}
</script>
</head>
<body>
<a class="skip" href="#main">Skip to content</a>
<header class="bar">
	<div class="wrap wide bar__in">
		<a class="bar__mark" href="/">${mark}<span>${esc(SHORT)}</span><span class="bar__full">Swarnil Broadcast Kit</span></a>
		<button class="burger" type="button" data-menu-toggle aria-expanded="false" aria-controls="sitenav" aria-label="Menu">
			<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.9" stroke-linecap="round" aria-hidden="true"><path class="burger__top" d="M4 7h16"/><path class="burger__mid" d="M4 12h16"/><path class="burger__bot" d="M4 17h16"/></svg>
		</button>
		<nav id="sitenav">${NAV.map(([u, l, i]) => `<a href="${u}"${u === url ? ' aria-current="page"' : ''}>${icons[i]}<span>${l}</span></a>`).join('')}</nav>
		<button class="theme" type="button" data-theme-toggle title="Switch between the light and dark theme" aria-label="Switch between the light and dark theme"><span class="theme__label">Theme</span>
			<svg class="sun" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" aria-hidden="true"><circle cx="12" cy="12" r="4"/><path d="M12 2v2M12 20v2M4.9 4.9l1.4 1.4M17.7 17.7l1.4 1.4M2 12h2M20 12h2M4.9 19.1l1.4-1.4M17.7 6.3l1.4-1.4"/></svg>
			<svg class="moon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M21 12.8A9 9 0 1 1 11.2 3a7 7 0 0 0 9.8 9.8z"/></svg>
		</button>
	</div>
</header>
<main id="main">
${body}
</main>
<footer>
	<div class="wrap wide">
		<p><strong>${esc(NAME)}</strong> — native overlays for OBS Studio. Built for ${esc(BUILT_FOR)}.</p>
		<p>Code is MIT. A compiled plugin links libobs, which is GPL-2.0, so the binary follows the
		GPL’s terms. Geist and Geist Mono are © Vercel under the SIL Open Font License 1.1.</p>
		<p><a href="${REPO}">Source and releases on GitHub</a> · <a href="https://imswarnil.com">imswarnil.com</a></p>
	</div>
</footer>
<script src="/site.js" type="module"></script>
${script ? `<script src="${script}" type="module"></script>` : ''}
</body>
</html>
`;
}

/* ---- the pages ------------------------------------------------------------ */

const sourcesByTag = () => {
	const order = ['Indicator', 'Titles', 'Camera', 'Filter', 'Audio filter', 'Audio', 'Live data', 'Scenes', 'Transition'];
	const seen = new Map();
	for (const s of SOURCES) {
		if (!seen.has(s.tag)) seen.set(s.tag, []);
		seen.get(s.tag).push(s);
	}
	return order.filter((t) => seen.has(t)).map((t) => [t, seen.get(t)]);
};

function home() {
	const body = `
<section class="hero hero--split pattern">
	<div class="wrap wide hero__grid">
		<div class="hero__words">
			<p class="eyebrow"><i class="dot" aria-hidden="true"></i> Native OBS plugin</p>
			<h1>Overlays OBS draws itself.</h1>
			<p class="lede">A tally light that knows when you are live, a level meter in real
			decibels, live subscriber and member counts, a QR code people can scan, a camera frame in
			9:16, filters for your picture and your voice, and a transition — ${SOURCES.length} pieces
			and a show of ${SCENES.length} scenes. No browser source, no web server, no URL to paste.</p>
			<p class="btns">
				<a class="btn btn--primary" href="${REPO}/releases">Download the plugin</a>
				<a class="btn" href="/builder/">Design a scene</a>
				<a class="btn" href="/docs/">Set it up</a>
			</p>
			<dl class="facts">
				<div><dt>${SOURCES.filter((s) => !isFilter(s) && s.tag !== 'Transition').length}</dt><dd>sources</dd></div>
				<div><dt>${SOURCES.filter(isFilter).length}</dt><dd>filters</dd></div>
				<div><dt>${SCENES.length}</dt><dd>scenes</dd></div>
				<div><dt>0</dt><dd>browser sources</dd></div>
			</dl>
		</div>
		<figure class="hero__shot">
			<img src="/screens/live.jpg" alt="The Live scene: a lower third, a camera frame and a ticker over a dark canvas" width="1600" height="900">
			<figcaption>The Live scene, exactly as the plugin builds it.</figcaption>
		</figure>
	</div>
</section>

<section>
	<div class="wrap wide">
		<h2>The show it builds</h2>
		<p class="sub">One menu item writes ${SCENES.length} complete scenes and switches to them.
		These are real frames from that collection, not mock-ups — every picture on this site comes
		out of the plugin’s own self-test.</p>
		<div class="grid grid--2">
			${SCENES.filter((s) => hasShot(s.img))
				.slice(0, 4)
				.map(
					(s) => `<article class="scene">
				<a class="shot" href="/scenes/#${s.img}"><img src="/screens/${s.img}.jpg" alt="The ${esc(s.name)} scene" loading="lazy" width="1600" height="900"></a>
				<h3>${esc(s.name)}</h3><p>${prose(s.body)}</p>
			</article>`
				)
				.join('')}
		</div>
		<p class="btns"><a class="btn" href="/scenes/">See all ${SCENES.length} scenes</a></p>
	</div>
</section>

<section>
	<div class="wrap wide">
		<h2>What it can do that a web overlay cannot</h2>
		<p class="sub">These are not stylistic preferences. They are things a page inside a Browser
		Source has no way to reach.</p>
		<div class="grid grid--3">
			<div class="card"><h3>Know you are live</h3><p>The tally light reads OBS’s own streaming
			and recording state, so it turns red when you go live rather than when you remember to
			click something.</p></div>
			<div class="card"><h3>Hear the program mix</h3><p>The visualizer and the meter listen to
			what OBS is actually outputting — every source, every filter — not just a microphone the
			browser was granted.</p></div>
			<div class="card"><h3>See dropped frames</h3><p>Uptime, bitrate, dropped frames and
			network congestion come from the running output. No page can ask for them.</p></div>
			<div class="card"><h3>Be a transition</h3><p>A transition is composited between two scene
			textures. Nothing running inside a page can see both.</p></div>
			<div class="card"><h3>Cost almost nothing</h3><p>One or two draw calls per source instead
			of a Chromium process per overlay.</p></div>
			<div class="card"><h3>Work offline</h3><p>The QR code is generated inside the plugin. No
			third-party service sees your link, and nothing breaks when the connection does.</p></div>
		</div>
	</div>
</section>

<section>
	<div class="wrap wide">
		<h2>${SOURCES.length} sources</h2>
		<p class="sub">Every one shares a <em>Look</em> group — one accent colour, a scale slider, a
		tone, a font — so a scene changes together rather than one piece at a time.</p>
		${sourcesByTag()
			.map(
				([tag, list]) => `<h3 style="margin-top:1.6rem">${esc(tag)}</h3>
		<div class="grid grid--3">${list
			.map(
				(s) => `<a class="card" href="/sources/#${s.id}" style="text-decoration:none">
			<h3>${esc(s.name)}</h3><p>${prose(s.one)}</p></a>`
			)
			.join('')}</div>`
			)
			.join('')}
	</div>
</section>

<section>
	<div class="wrap wide">
		<h2>Live numbers, from your own accounts</h2>
		<p class="sub">Put in an API key and the counter fetches on a background thread — the
		graphics thread never waits on the network, and a failed request keeps the last good number
		on screen.</p>
		<div class="grid grid--3">
			${APIS.map(
				(a) => `<div class="card"><h3>${esc(a.name.split('—')[0].trim())}</h3><p>${prose(a.name.split('—')[1] || '')}</p></div>`
			).join('')}
		</div>
		<p class="btns"><a class="btn" href="/docs/#apis">How to get each key</a></p>
	</div>
</section>
`;
	return page({
		url: '/',
		title: 'Overview',
		description: `${NAME} — a native OBS Studio plugin: ${SOURCES.length} overlay sources, live API counters, a QR code, a real level meter and a transition. No browser source.`,
		body,
	});
}

function scenes() {
	const body = `
<section class="hero pattern">
	<div class="wrap wide">
		<p class="eyebrow">Scenes</p>
		<h1>A whole show, in one menu item.</h1>
		<p class="lede"><strong>Tools → Broadcast Kit: create the scene collection</strong> writes
		these ${SCENES.length} scenes and switches to them. Every source in them is an ordinary
		source — select it, open Properties, change anything.</p>
	</div>
</section>
<section>
	<div class="wrap wide">
		<div class="grid grid--2">
			${SCENES.map(
				(s) => `<article class="scene" id="${s.img}">
			${hasShot(s.img)
				? `<button class="shot" type="button" data-shot="/screens/${s.img}.jpg" data-title="${esc(s.name)}" aria-label="See the ${esc(s.name)} scene full size"><img src="/screens/${s.img}.jpg" alt="The ${esc(s.name)} scene" loading="lazy" width="1600" height="900"></button>`
				: `<span class="shot shot--none">Screenshot to come</span>`}
			<h3>${esc(s.name)}</h3><p>${prose(s.body)}</p>
			${s.uses ? `<ul class="uses">${s.uses.map((u) => `<li>${esc(u)}</li>`).join('')}</ul>` : ''}
		</article>`
			).join('')}
		<dialog id="lightbox" aria-label="Scene, full size">
			<img alt="">
			<div class="lightbox__bar"><strong></strong><button type="button" data-close>Close</button></div>
		</dialog>
		</div>
		<p class="note" style="margin-top:2rem">The frames are empty where your camera and screen
		capture go. The kit draws treatments, not captures: creating a camera on your behalf would
		switch your webcam on just because you opened a menu. Add your own capture and drag it below
		the frame in the Sources list.</p>
	</div>
</section>
`;
	return page({
		url: '/scenes/',
		title: 'Scenes',
		description: `The ${SCENES.length} scenes ${NAME} builds: starting soon, live, talking head, screen share, interview, Q&A, support, vertical, be right back, ending and a private monitoring desk.`,
		body,
	});
}

function sources() {
	const body = `
<section class="hero pattern pattern--dots">
	<div class="wrap wide">
		<p class="eyebrow">Sources</p>
		<h1>${SOURCES.length} sources in OBS’s own menu.</h1>
		<p class="lede">Every one shares a <em>Look</em> group — one accent colour, a scale slider
		that grows type, padding and radius together, a tone (glass, solid or light) and a font — and
		a <em>Motion</em> group for how it arrives.</p>
	</div>
</section>
${sourcesByTag()
	.map(
		([tag, list]) => `<section>
	<div class="wrap wide">
		<h2>${esc(tag)}</h2>
		${list
			.map(
				(s) => `<article class="src" id="${s.id}">
			<div class="src__head"><h3>${esc(s.name)}</h3><span class="tag">${esc(s.tag)}</span><code class="src__id">${esc(s.id)}</code></div>
			<p class="src__one">${prose(s.one)}</p>
			<p class="src__body">${prose(s.body)}</p>
			<dl class="props">${s.props.map(([k, v]) => `<div><dt>${esc(k)}</dt><dd>${prose(v)}</dd></div>`).join('')}</dl>
			${s.note ? `<p class="note">${prose(s.note)}</p>` : ''}
		</article>`
			)
			.join('')}
	</div>
</section>`
	)
	.join('')}
`;
	return page({
		url: '/sources/',
		title: 'Sources',
		description: `Every source in ${NAME}, what it is for and what you can change about it.`,
		body,
	});
}

function builder() {
	const body = `
<section class="builder">
	<div class="wrap wide builder__head">
		<div>
			<p class="eyebrow">Builder</p>
			<h1>Design a scene. Import it into OBS.</h1>
			<p class="lede">Place the pieces, set the words, and export a scene collection. What
			comes out is the real thing — the same source ids and setting keys the plugin registers —
			so the import builds native sources, not a picture of them.</p>
		</div>
	</div>

	<div class="builder__grid wrap wide">
		<aside class="builder__side">
			<h3>Start from</h3>
			<div class="palette__grid" id="start"></div>
			<div id="palette"></div>
		</aside>

		<div class="builder__main">
			<div class="builder__bar">
				<label class="field field--inline"><span>Scene</span><input id="scene-name" value="SBK Scene"></label>
				<label class="field field--inline"><span>Accent</span><input id="accent" type="color" value="#f5273f"></label>
				<label class="field field--inline"><span>Scale</span><input id="scale" type="range" min="0.5" max="2" step="0.05" value="1"><output id="scale-out">1.00×</output></label>
				<label class="field field--inline"><span>Tone</span><select id="tone"><option value="glass">glass</option><option value="solid">solid</option><option value="light">light</option></select></label>
				<label class="field field--inline field--check"><input id="snap" type="checkbox" checked><span>Snap</span></label>
				<button class="btn btn--ghost" id="clear" type="button">Clear</button>
				<button class="btn btn--primary" id="export" type="button">Export for OBS</button>
			</div>
			<div class="builder__stage" id="stage"><div class="builder__canvas" id="canvas"></div></div>
			<p class="note" id="export-note" hidden></p>
			<p class="note">Drag to move, arrow keys to nudge (hold shift for a bigger step), delete to
			remove. The canvas is 1920 × 1080, and the three controls above it are the plugin&rsquo;s own
			<em>Look</em>: accent, scale and tone apply to every piece at once, live, the same way one
			slider grows type, padding and radius together in OBS. The drawing itself is a schematic — it
			shows where things sit and how they read, not the plugin&rsquo;s shaders and hinted type,
			because anything that looked pixel-perfect would drift the first time a shader changed.</p>
		</div>

		<aside class="builder__side builder__props">
			<h3>Selected</h3>
			<div id="props"></div>
		</aside>
	</div>
</section>
`;
	return page({
		url: '/builder/',
		title: 'Builder',
		description: `Design an OBS scene from ${NAME}'s pieces in the browser and export a scene collection you can import straight into OBS.`,
		body,
		script: '/builder/app.js',
	});
}

/*  The Make page.

    Everything on it that is a number or a list — how many scenes a show has,
    what is in each one, how big the download is — is read back out of the
    collection the build just generated, never typed here. A page that says
    "five scenes" beside a file containing four is the kind of thing nobody
    notices for a year.  */
function make(built) {
	const body = `
<section class="hero pattern">
	<div class="wrap wide">
		<p class="eyebrow">Make</p>
		<h1>Five shows you can have in about a minute.</h1>
		<p class="lede">Each one is a real scene collection, generated from a spec in this repository by
		the same tool the agent skill uses. Download it, <strong>Scene Collection → Import</strong>, pick
		your camera once. It lands beside whatever you are already using rather than replacing it.</p>
		<p class="btns"><a class="btn btn--primary" href="#shows">The shows</a>
		<a class="btn" href="#combos">What the pieces make</a>
		<a class="btn" href="#how">Three ways to build your own</a></p>
	</div>
</section>

<section id="shows">
	<div class="wrap wide">
		<h2>Ready-made</h2>
		<p class="sub">Every download below was built and checked by this site&rsquo;s own build. The
		scene lists are read out of the generated file, so the page cannot promise a scene the collection
		does not contain.</p>
		<div class="grid grid--2">
			${PRESETS.map((pre) => {
				const b = built[pre.id];
				return `<article class="scene" id="${pre.id}">
				<h3>${esc(pre.name)}</h3>
				<p class="eyebrow" style="margin:.1rem 0 .55rem">${esc(pre.who)}</p>
				<p>${prose(pre.one)}</p>
				<p class="sub">${prose(pre.body)}</p>
				<ul class="uses">${b.scenes.map((n) => `<li>${esc(n)}</li>`).join('')}</ul>
				<p class="sub" style="margin-top:.6rem"><strong>Uses:</strong> ${pre.pieces.map(esc).join(' · ')}</p>
				<p class="btns"><a class="btn btn--primary" href="/presets/${pre.id}.json" download="sbk-${pre.id}.json">Download the collection</a>
				<a class="btn" href="${REPO}/blob/main/presets/${pre.id}.json">Read the spec</a></p>
				<p class="sub">${b.scenes.length} scenes · ${b.items} items · ${b.kb}&nbsp;KB</p>
			</article>`;
			}).join('')}
		</div>
	</div>
</section>

<section id="combos" class="pattern">
	<div class="wrap wide">
		<h2>What the pieces make</h2>
		<p class="sub">The source list answers <em>what is there</em>. This answers the question people
		actually arrive with, which is <em>what do I put where</em>.</p>
		<div class="grid grid--2">
			${COMBOS.map(
				(c) => `<article class="scene">
				<h3>${esc(c.make)}</h3>
				<ul class="uses">${c.from.map((f) => `<li>${esc(f)}</li>`).join('')}</ul>
				<p class="sub">${prose(c.how)}</p>
			</article>`
			).join('')}
		</div>
	</div>
</section>

<section id="how">
	<div class="wrap wide">
		<h2>Three ways to build your own</h2>
		<div class="grid grid--3">
			<article class="scene">
				<h3>The Tools menu</h3>
				<p class="sub">Fastest, and the only one that wires up your camera and microphone for you.
				<strong>Tools → Broadcast Kit: create the scene collection</strong> writes ${SCENES.length}
				scenes and switches to them. A scene you have already moved things around in is refilled,
				not replaced.</p>
				<p class="btns"><a class="btn" href="/scenes/">See all ${SCENES.length}</a></p>
			</article>
			<article class="scene">
				<h3>The builder</h3>
				<p class="sub">Drag the pieces onto a 1920 × 1080 canvas in the browser, set the words, and
				export. The preview is deliberately a schematic — anything that looked pixel-perfect would
				only drift from the plugin the first time a shader changed.</p>
				<p class="btns"><a class="btn" href="/builder/">Open the builder</a></p>
			</article>
			<article class="scene">
				<h3>The agent skill</h3>
				<p class="sub">Describe the scene and let an agent write the spec. Its reference is generated
				from the plugin&rsquo;s own C, so it cannot name a setting that does not exist, and it
				validates before it writes — which matters, because OBS silently ignores a setting it does
				not recognise.</p>
				<p class="btns"><a class="btn" href="${REPO}/blob/main/docs/SKILL.md">How it works</a></p>
			</article>
		</div>
	</div>
</section>

<section>
	<div class="wrap wide">
		<h2>Importing one</h2>
		<ol class="steps">
			<li><strong>Scene Collection → Import</strong> in OBS&rsquo;s menu bar, then pick the file you
			downloaded. It is added beside your current collection; nothing is overwritten.</li>
			<li><strong>Scene Collection →</strong> and choose it by name to switch.</li>
			<li>Click the <em>Camera</em> source and pick your device in Properties. A downloaded
			collection cannot choose one for you, because the device list only exists inside a running
			OBS. Shows with a screen capture need the same, and macOS will ask for Screen Recording
			permission the first time.</li>
			<li>Add <strong>SBK Wipe</strong> or <strong>SBK Logo Sting</strong> from the Scene
			Transitions panel&rsquo;s <strong>+</strong>. A collection file cannot carry a transition.</li>
		</ol>
		<p class="sub">All of this assumes the plugin is installed. If every source imports as missing,
		that is what happened — see <a href="/docs/">the docs</a>.</p>
	</div>
</section>
`;
	return page({
		url: '/make/',
		title: 'Make',
		description: `Five ready-made OBS scene collections built with ${NAME}, what its pieces combine into, and three ways to build your own.`,
		body,
	});
}

function setup() {
	const body = `
<section class="hero pattern">
	<div class="wrap wide">
		<p class="eyebrow">Docs</p>
		<h1>Installed in a minute, set up in five.</h1>
		<p class="lede">macOS, OBS Studio 30 or newer. Built and tested against ${esc(BUILT_FOR)}.</p>
	</div>
</section>

<section>
	<div class="wrap wide">
		<h2>Getting it running</h2>
		<div class="steps">
			${STEPS.map(
				(s) => `<div class="step">
			<span class="step__n">${s.n}</span>
			<div><h3>${esc(s.title)}</h3><p>${prose(s.body)}</p></div>
			${s.code ? `<pre><code>${esc(s.code)}</code></pre>` : ''}
		</div>`
			).join('')}
		</div>
		<p class="note">macOS may refuse a plugin downloaded from the internet. If OBS starts but the
		sources are missing, clear the quarantine flag and restart it.</p>
		<pre><code>xattr -dr com.apple.quarantine ~/Library/Application\\ Support/obs-studio/plugins/sbk.plugin</code></pre>
	</div>
</section>

<section id="apis">
	<div class="wrap wide">
		<h2>Live numbers</h2>
		<p class="sub">The counter fetches on a background thread and never blocks the picture. It
		checks every sixty seconds by default, and no faster than every fifteen — an overlay that
		burns through someone’s API quota is a broken overlay.</p>
		<div class="grid grid--2">
			${APIS.map(
				(a) => `<div class="card">
			<h3>${esc(a.name)}</h3>
			<ol style="margin:0.7rem 0 0;padding-left:1.1rem;color:var(--ink-dim);font-size:0.94rem">
				${a.steps.map((st) => `<li style="margin:0.3rem 0">${prose(st)}</li>`).join('')}
			</ol>
			${a.note ? `<p class="note">${prose(a.note)}</p>` : ''}
		</div>`
			).join('')}
		</div>
		<p class="note">OBS saves every source setting into the scene collection as plain text, keys
		included. Begin a key field with <code>@</code> and a file path —
		<code>@/Users/you/.youtube-key</code> — and the kit reads it from there instead, so a
		collection you share carries no secret.</p>
	</div>
</section>

<section id="builder-note">
	<div class="wrap wide">
		<h2>Designing a scene</h2>
		<p class="sub">The <a href="/builder/">builder</a> places the kit's pieces on a 1920 × 1080
		canvas and exports a scene collection. In OBS: <strong>Scene Collection → Import</strong>,
		pick the file it saved. The sources it creates are the real ones — the export carries the
		same ids and setting keys the plugin registers, so nothing is approximated on the way in.</p>
		<p class="note">It saves what you place, so closing the tab does not lose the layout. The
		preview is a schematic rather than a copy of the plugin's drawing; anything that looked
		pixel-perfect there would only drift away from the real thing the first time a shader
		changed.</p>
	</div>
</section>

<section id="remote">
	<div class="wrap wide">
		<h2>The phone remote</h2>
		<p class="sub">Scenes, stream and record, the mic, the transition, and a button for every
		hotkey the kit registers — read out of OBS rather than hard-coded, so one added to the plugin
		later turns up without the remote changing.</p>
		<div class="steps">
			<div class="step"><span class="step__n">1</span><div><h3>Turn OBS's own server on</h3>
			<p><strong>Tools → WebSocket Server Settings</strong>, tick <em>Enable</em>, then
			<em>Show Connect Info</em> for the port and password. This is OBS's server, not
			something the kit runs.</p></div></div>
			<div class="step"><span class="step__n">2</span><div><h3>Serve the remote on your network</h3>
			<p>Double-click <code>remote/serve.command</code>. It prints the address to open on your
			phone and the address to type into the remote.</p></div>
			<pre><code>./remote/serve.command</code></pre></div>
			<div class="step"><span class="step__n">3</span><div><h3>Open it on the phone</h3>
			<p>Same wifi, the address it printed. Put in the password once and it is remembered on
			that phone.</p></div></div>
		</div>
		<p class="note">It has to be served over plain <code>http</code> from your own machine, and
		that is not a shortcut. A page loaded over <code>https</code> cannot open the unencrypted
		<code>ws://</code> connection obs-websocket speaks — browsers block it — so a copy hosted
		here could never connect to your OBS. <a href="/remote/">The remote is on this site</a> if
		you want to see it, and it will tell you the same thing if you press Connect.</p>
		<p class="note">Nothing goes through this site either way. The page talks straight to your
		machine, and the password is kept in that phone's browser and nowhere else.</p>
	</div>
</section>

<section>
	<div class="wrap wide">
		<h2>Build scenes with an agent</h2>
		<p class="sub">The kit ships an <strong>agent skill</strong>. Install it and Claude Code — or
		anything else that reads the Agent Skills format — can design a scene, or a whole show, out of
		these sources and hand back a collection OBS will import.</p>
		<pre><code>git clone ${REPO}.git
mkdir -p ~/.claude/skills
cp -R Swarnil-Broadcast-Kit/skills/sbk-scenes ~/.claude/skills/</code></pre>
		<p class="sub" style="margin-top:1rem">Then ask for what you want: <em>a screen-share scene
		with the camera bottom-right on a plate, a chapter chip, a mic meter and a questions panel down
		the right.</em> It knows every source and every setting because its reference is generated from
		the plugin&rsquo;s own C, and it validates a spec before writing anything — which matters,
		because OBS silently ignores a setting it does not recognise, so a typo gives you a scene that
		looks almost right and nobody can say why.</p>
		<p class="sub">You can drive the generator yourself, without an agent:</p>
		<pre><code>node skills/sbk-scenes/scripts/build-collection.mjs my-show.json --check
node skills/sbk-scenes/scripts/build-collection.mjs my-show.json -o my-show.collection.json</code></pre>
		<p class="btns" style="margin-top:1.25rem"><a class="btn" href="${REPO}/blob/main/docs/SKILL.md">How the skill works</a>
		<a class="btn btn--ghost" href="${REPO}/blob/main/skills/sbk-scenes/references/recipes.md">Six worked scenes</a></p>
	</div>
</section>

<section>
	<div class="wrap wide">
		<h2>Building it yourself</h2>
		<p class="sub">Nothing is downloaded at build time. The libobs headers are vendored; the rest
		is Homebrew and what macOS already has.</p>
		<pre><code>brew install cmake simde jansson
git clone ${REPO.replace('https://github.com/', 'https://github.com/')}.git
cd Swarnil-Broadcast-Kit
./build.command</code></pre>
		<p class="sub" style="margin-top:1rem"><code>build.command</code> compiles, installs the
		plugin, copies the fonts and installs the profile. Quit OBS first — a loaded plugin cannot be
		replaced underneath a running OBS, and the script refuses to run while one is up.</p>
	</div>
</section>

<section>
	<div class="wrap wide">
		<h2>Questions</h2>
		${FAQ.map(
			(f) => `<details><summary>${esc(f.q)}</summary><p>${prose(f.a)}</p></details>`
		).join('')}
	</div>
</section>
`;
	return page({
		url: '/docs/',
		title: 'Docs',
		description: `How to install ${NAME} in OBS Studio, put your camera under the frames, add the transition, and wire up live YouTube and Ghost counts with your own API keys.`,
		body,
	});
}

/* ---- build ---------------------------------------------------------------- */

const t0 = performance.now();
fs.rmSync(DIST, { recursive: true, force: true });
fs.mkdirSync(DIST, { recursive: true });

write(path.join(DIST, 'index.html'), home());
/* Build every preset before the page that lists them, so the page can read the
   real scene names and sizes out of what was actually generated. */
const built = {};
for (const pre of PRESETS) {
	const spec = JSON.parse(fs.readFileSync(path.join(ROOT, 'presets', `${pre.id}.json`), 'utf8'));
	const problems = validateSpec(spec);
	if (problems.length) {
		console.error(`✗ presets/${pre.id}.json does not validate:`);
		for (const p of problems) console.error(`  - ${p}`);
		process.exit(1);
	}
	const json = JSON.stringify(buildCollection(spec), null, '\t') + '\n';
	write(path.join(DIST, 'presets', `${pre.id}.json`), json);
	built[pre.id] = {
		scenes: spec.scenes.map((s) => s.name),
		items: spec.scenes.reduce((n, s) => n + (s.items || []).length, 0),
		kb: Math.max(1, Math.round(Buffer.byteLength(json) / 1024)),
	};
}

write(path.join(DIST, 'make/index.html'), make(built));
write(path.join(DIST, 'scenes/index.html'), scenes());
write(path.join(DIST, 'sources/index.html'), sources());
write(path.join(DIST, 'builder/index.html'), builder());
write(path.join(DIST, 'docs/index.html'), setup());
/* the page used to live at /setup/ and something out there will still link to
   it; a meta refresh costs one file and never breaks */
write(
	path.join(DIST, 'setup/index.html'),
	`<!doctype html><html lang="en"><head><meta charset="utf-8"><title>Docs · ${SHORT}</title>` +
		`<meta name="description" content="This page moved to /docs/."><meta http-equiv="refresh" content="0; url=/docs/">` +
		`<link rel="canonical" href="${BASE}docs/"></head><body><p>Moved to <a href="/docs/">/docs/</a>.</p></body></html>\n`
);

copy(path.join(ROOT, 'site/site.css'), path.join(DIST, 'site.css'));
copy(path.join(ROOT, 'site/site.js'), path.join(DIST, 'site.js'));
for (const f of fs.readdirSync(path.join(ROOT, 'docs/screens')))
	copy(path.join(ROOT, 'docs/screens', f), path.join(DIST, 'screens', f));
/* the remote, as it actually ships — it explains its own https limitation when
   someone presses Connect from here */
for (const f of ['index.html', 'sha256.js'])
	copy(path.join(ROOT, 'remote', f), path.join(DIST, 'remote', f));
/* the builder's palette is shared between the generator and the browser, so the
   same file is served rather than a second copy of the truth */
copy(path.join(ROOT, 'site/builder.mjs'), path.join(DIST, 'builder/builder.mjs'));
copy(path.join(ROOT, 'site/builder-app.js'), path.join(DIST, 'builder/app.js'));
for (const f of ['Geist-Regular.ttf', 'Geist-Medium.ttf', 'Geist-SemiBold.ttf', 'GeistMono-Regular.ttf'])
	copy(path.join(ROOT, 'fonts', f), path.join(DIST, 'fonts', f));

/* the mark: the recording light, which is the whole idea in one shape */
write(
	path.join(DIST, 'icon.svg'),
	`<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 32 32"><rect width="32" height="32" rx="7" fill="#0b0b0b"/><circle cx="16" cy="16" r="7" fill="#f5273f"/></svg>\n`
);

write(
	path.join(DIST, '404.html'),
	page({
		url: '/404',
		title: 'Not found',
		description: 'That page does not exist.',
		body: `<section class="hero"><div class="wrap wide"><p class="eyebrow">404</p><h1>Nothing here.</h1>
		<p class="lede">That page does not exist.</p><p class="btns"><a class="btn btn--primary" href="/">Back to the overview</a></p></div></section>`,
	})
);

const urls = ['', 'make/', 'scenes/', 'sources/', 'builder/', 'docs/'];
write(
	path.join(DIST, 'sitemap.xml'),
	`<?xml version="1.0" encoding="UTF-8"?>\n<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">\n` +
		urls.map((u) => `\t<url><loc>${BASE}${u}</loc></url>`).join('\n') +
		`\n</urlset>\n`
);
write(path.join(DIST, 'robots.txt'), `User-agent: *\nAllow: /\nSitemap: ${BASE}sitemap.xml\n`);
write(
	path.join(DIST, '_headers'),
	['/fonts/*', '  Cache-Control: public, max-age=31536000, immutable', '/screens/*', '  Cache-Control: public, max-age=86400', ''].join('\n')
);

console.log(`${NAME} site → dist/ in ${Math.round(performance.now() - t0)}ms (base ${BASE})`);
