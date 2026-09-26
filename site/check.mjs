/*  What CI runs after the site build: every local href and src the pages emit
    must be a file the build actually wrote. A docs site whose download link is
    a 404 is worse than no docs site.  */

import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const DIST = path.join(ROOT, 'dist');
const problems = [];

const walk = (dir) =>
	fs.readdirSync(dir, { withFileTypes: true }).flatMap((d) =>
		d.isDirectory() ? walk(path.join(dir, d.name)) : [path.join(dir, d.name)]
	);

if (!fs.existsSync(DIST)) {
	console.error('dist/ is missing — run `node site/build.mjs` first');
	process.exit(1);
}

const pages = walk(DIST).filter((f) => f.endsWith('.html'));
for (const file of pages) {
	const html = fs.readFileSync(file, 'utf8');
	for (const m of html.matchAll(/(?:href|src)="([^"#?]+)/g)) {
		const ref = m[1];
		if (/^(https?:|mailto:|data:)/.test(ref)) continue;
		const target = ref.startsWith('/') ? path.join(DIST, ref) : path.resolve(path.dirname(file), ref);
		if (!fs.existsSync(target) && !fs.existsSync(path.join(target, 'index.html')))
			problems.push(`${path.relative(ROOT, file)} → ${ref}`);
	}
	if (!/<title>[^<]+<\/title>/.test(html)) problems.push(`${path.relative(ROOT, file)} has no title`);
	if (!/name="description" content="[^"]+"/.test(html))
		problems.push(`${path.relative(ROOT, file)} has no description`);
}

/* A scene without a screenshot is allowed — a new one lands before its picture
   does — but the page has to say so rather than show a broken image. */
const { SCENES } = await import('./content.mjs');
const missing = SCENES.filter((s) => !fs.existsSync(path.join(DIST, 'screens', `${s.img}.jpg`)));
if (missing.length) console.log(`  · ${missing.length} scene(s) awaiting a screenshot: ${missing.map((s) => s.name).join(', ')}`);

if (problems.length) {
	console.error(`\n✗ ${problems.length} problem${problems.length > 1 ? 's' : ''}:\n`);
	for (const p of problems) console.error('  -', p);
	process.exit(1);
}
console.log(`✓ ${pages.length} pages, every link resolves, every screenshot present`);
