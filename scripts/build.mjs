/* Build — writes the whole deployable into dist/:

     dist/assets/tally.{css,js}   the runtime every overlay loads
     dist/assets/fonts/           Geist and Geist Mono (OFL, from the geist package)
     dist/<slug>/index.html       one page per overlay, from overlays/
     dist/scenes/                 the scene collection and the profile
     dist/index.html, docs/…      the site, from site/

   Nothing here reads outside the repository. */

import fs from 'node:fs';
import path from 'node:path';
import * as esbuild from 'esbuild';
import { ROOT, DIST, BASE, pkg, copy, write, log } from './lib.mjs';
import { OVERLAYS } from '../overlays/registry.mjs';
import { buildSite } from '../site/build.mjs';

const t0 = performance.now();
fs.rmSync(DIST, { recursive: true, force: true });
fs.mkdirSync(DIST, { recursive: true });

/* 1. Runtime */
await esbuild.build({
	entryPoints: [path.join(ROOT, 'src/index.css')],
	outfile: path.join(DIST, 'assets/tally.css'),
	bundle: true,
	minify: true,
	external: ['*.woff2'],
	legalComments: 'none',
	banner: { css: `/* Tally ${pkg.version} — https://obs.imswarnil.com — MIT */` },
});
await esbuild.build({
	entryPoints: [path.join(ROOT, 'src/js/tally.js')],
	outfile: path.join(DIST, 'assets/tally.js'),
	bundle: true,
	minify: true,
	format: 'esm',
	target: ['chrome103'],  // obs-browser ships Chromium 103+
	define: { __TALLY_VERSION__: JSON.stringify(pkg.version) },
	banner: { js: `/* Tally ${pkg.version} — https://obs.imswarnil.com — MIT */` },
});
/* `'__TALLY_VERSION__'` sits inside a string in the source; esbuild's define
   only replaces identifiers, so patch the string form as well. */
const jsPath = path.join(DIST, 'assets/tally.js');
write(jsPath, fs.readFileSync(jsPath, 'utf8').replace('__TALLY_VERSION__', pkg.version));
log('assets/tally.css, assets/tally.js');

/* 2. Fonts */
const geist = path.join(ROOT, 'node_modules/geist/dist/fonts');
copy(path.join(geist, 'geist-sans/Geist-Variable.woff2'), path.join(DIST, 'assets/fonts/Geist-Variable.woff2'));
copy(path.join(geist, 'geist-mono/GeistMono-Variable.woff2'), path.join(DIST, 'assets/fonts/GeistMono-Variable.woff2'));
copy(path.join(ROOT, 'assets/brand/mark.svg'), path.join(DIST, 'assets/mark.svg'));
log('assets/fonts, assets/mark.svg');

/* 3. Overlays */
for (const o of OVERLAYS) {
	copy(path.join(ROOT, 'overlays', o.slug, 'index.html'), path.join(DIST, o.slug, 'index.html'));
}
log(`${OVERLAYS.length} overlays`);

/* 4. Scenes */
for (const f of fs.readdirSync(path.join(ROOT, 'scenes'))) {
	if (f.endsWith('.json') || f.endsWith('.ini')) copy(path.join(ROOT, 'scenes', f), path.join(DIST, 'scenes', f));
}
copy(path.join(ROOT, 'scenes/profile/Tally/basic.ini'), path.join(DIST, 'scenes/profile/Tally/basic.ini'));
log('scenes');

/* 5. Site */
const pages = buildSite({ dist: DIST, base: BASE, version: pkg.version });
log(`${pages} site pages`);

write(path.join(DIST, 'robots.txt'), `User-agent: *\nAllow: /\nSitemap: ${BASE}sitemap.xml\n`);
write(path.join(DIST, '_headers'), [
	'/assets/*',
	'  Cache-Control: public, max-age=3600, stale-while-revalidate=86400',
	'  Access-Control-Allow-Origin: *',
	'/scenes/*',
	'  Access-Control-Allow-Origin: *',
	'',
].join('\n'));

console.log(`\nTally ${pkg.version} built → dist/ in ${Math.round(performance.now() - t0)}ms (base ${BASE})`);
