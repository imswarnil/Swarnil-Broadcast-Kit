/*  The docs site at obs.imswarnil.com.

    Plain Node, no dependencies, no install step: `node site/build.mjs` writes
    dist/ and that is the whole build. The content lives in site/content.mjs and
    the screenshots in docs/screens/ are real frames off the self-test, so the
    pictures on the site are the scenes the plugin actually builds.  */

import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { NAME, SHORT, REPO, BUILT_FOR, SOURCES, SCENES, STEPS, APIS, FAQ } from './content.mjs';

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
const esc = (s) => String(s).replace(/&(?![a-z#]+;)/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
/* the content file writes prose with inline markup already in it */
const prose = (s) => String(s).replace(/\s+/g, ' ').trim();

const NAV = [
	['/', 'Overview'],
	['/scenes/', 'Scenes'],
	['/sources/', 'Sources'],
	['/setup/', 'Setup'],
];

function page({ url, title, description, body }) {
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
<link rel="canonical" href="${BASE.replace(/\/$/, '')}${url}">
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
		<a class="bar__mark" href="/"><i class="dot" aria-hidden="true"></i><span>${esc(SHORT)}</span><span> Broadcast Kit</span></a>
		<nav>${NAV.map(([u, l]) => `<a href="${u}"${u === url ? ' aria-current="page"' : ''}>${l}</a>`).join('')}</nav>
		<button class="theme" type="button" data-theme-toggle aria-label="Switch between the light and dark theme">
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
</body>
</html>
`;
}

/* ---- the pages ------------------------------------------------------------ */

const sourcesByTag = () => {
	const order = ['Indicator', 'Titles', 'Camera', 'Filter', 'Audio', 'Live data', 'Scenes', 'Transition'];
	const seen = new Map();
	for (const s of SOURCES) {
		if (!seen.has(s.tag)) seen.set(s.tag, []);
		seen.get(s.tag).push(s);
	}
	return order.filter((t) => seen.has(t)).map((t) => [t, seen.get(t)]);
};

function home() {
	const body = `
<section class="hero">
	<div class="wrap wide">
		<p class="eyebrow"><i class="dot" aria-hidden="true"></i> Native OBS plugin</p>
		<h1>Overlays OBS draws itself.</h1>
		<p class="lede">A tally light that knows when you are live, a level meter in real decibels,
		live subscriber and member counts, a QR code people can scan, a camera frame in 9:16, and a
		transition — ${SOURCES.length} sources and a full show of ${SCENES.length} scenes. No browser
		source, no web server, no URL to paste.</p>
		<p class="btns">
			<a class="btn btn--primary" href="${REPO}/releases">Download the plugin</a>
			<a class="btn" href="/setup/">How to set it up</a>
			<a class="btn" href="${REPO}">Source on GitHub</a>
		</p>
	</div>
</section>

<section>
	<div class="wrap wide">
		<h2>The show it builds</h2>
		<p class="sub">One menu item writes ${SCENES.length} complete scenes and switches to them.
		These are real frames from that collection, not mock-ups — every picture on this site comes
		out of the plugin’s own self-test.</p>
		<div class="grid grid--2">
			${SCENES.slice(0, 4)
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
		<p class="btns"><a class="btn" href="/setup/#apis">How to get each key</a></p>
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
<section class="hero">
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
			<button class="shot" type="button" data-shot="/screens/${s.img}.jpg" data-title="${esc(s.name)}" aria-label="See the ${esc(s.name)} scene full size"><img src="/screens/${s.img}.jpg" alt="The ${esc(s.name)} scene" loading="lazy" width="1600" height="900"></button>
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
<section class="hero">
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

function setup() {
	const body = `
<section class="hero">
	<div class="wrap wide">
		<p class="eyebrow">Setup</p>
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
		url: '/setup/',
		title: 'Setup',
		description: `How to install ${NAME} in OBS Studio, put your camera under the frames, add the transition, and wire up live YouTube and Ghost counts with your own API keys.`,
		body,
	});
}

/* ---- build ---------------------------------------------------------------- */

const t0 = performance.now();
fs.rmSync(DIST, { recursive: true, force: true });
fs.mkdirSync(DIST, { recursive: true });

write(path.join(DIST, 'index.html'), home());
write(path.join(DIST, 'scenes/index.html'), scenes());
write(path.join(DIST, 'sources/index.html'), sources());
write(path.join(DIST, 'setup/index.html'), setup());

copy(path.join(ROOT, 'site/site.css'), path.join(DIST, 'site.css'));
copy(path.join(ROOT, 'site/site.js'), path.join(DIST, 'site.js'));
for (const f of fs.readdirSync(path.join(ROOT, 'docs/screens')))
	copy(path.join(ROOT, 'docs/screens', f), path.join(DIST, 'screens', f));
/* the remote, as it actually ships — it explains its own https limitation when
   someone presses Connect from here */
for (const f of ['index.html', 'sha256.js'])
	copy(path.join(ROOT, 'remote', f), path.join(DIST, 'remote', f));
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

const urls = ['', 'scenes/', 'sources/', 'setup/'];
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
