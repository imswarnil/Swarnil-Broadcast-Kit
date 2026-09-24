/* Check — what CI runs after the build. A failure here stops a deploy.
     1. every registered overlay has a page, and every page is registered
     2. every param a page reads is declared in the registry
     3. the scene collection on disk matches what the config generates
     4. nothing in src/ uses another system's namespace
     5. the built site references only files that exist */

import fs from 'node:fs';
import path from 'node:path';
import { ROOT, DIST, read } from './lib.mjs';
import { OVERLAYS, GLOBAL_PARAMS } from '../overlays/registry.mjs';
import { OUT as SCENES_OUT, json as scenesJson } from './scenes.mjs';

const problems = [];
const fail = (m) => problems.push(m);

/* 1 */
const pages = fs.readdirSync(path.join(ROOT, 'overlays'), { withFileTypes: true }).filter((d) => d.isDirectory()).map((d) => d.name);
for (const o of OVERLAYS) if (!pages.includes(o.slug)) fail(`registry lists "${o.slug}" but overlays/${o.slug}/index.html is missing`);
for (const p of pages) if (!OVERLAYS.some((o) => o.slug === p)) fail(`overlays/${p}/ is not in overlays/registry.mjs`);

/* 2 — params the runtime reads for everyone, plus what each page declares */
const RUNTIME = new Set(['hidden', 'demo', 'h24', 'seconds', 'at', 'done', 'speed', 'source', 'fft', 'smooth', 'style', 'bars', ...GLOBAL_PARAMS.map((p) => p.key)]);
for (const o of OVERLAYS) {
	const html = read(path.join(ROOT, 'overlays', o.slug, 'index.html'));
	const declared = new Set([...RUNTIME, ...o.params.map((p) => p.key)]);
	const used = new Set();
	for (const m of html.matchAll(/data-tally-(?:param|at|variant)="([^"]+)"/g)) used.add(m[1]);
	for (const m of html.matchAll(/q\.get\('([^']+)'\)/g)) used.add(m[1]);
	for (const k of used) if (!declared.has(k)) fail(`overlays/${o.slug} reads ?${k} but the registry does not declare it`);
}

/* 3 */
if (!fs.existsSync(SCENES_OUT) || read(SCENES_OUT) !== scenesJson()) fail(`scenes/${path.basename(SCENES_OUT)} is stale — run \`npm run scenes\``);

/* 4 — Tally is its own system; a leaked class from elsewhere is a bug */
for (const file of walk(path.join(ROOT, 'src'))) {
	const s = read(file);
	const hit = s.match(/(?:\.|--)(im|ck|kg)-[a-z]/);
	if (hit) fail(`${path.relative(ROOT, file)} uses "${hit[0]}" — only the tally- namespace belongs here`);
}

/* 5 */
if (!fs.existsSync(DIST)) fail('dist/ is missing — run `npm run build` first');
else {
	for (const file of walk(DIST).filter((f) => f.endsWith('.html'))) {
		const s = read(file);
		for (const m of s.matchAll(/(?:href|src)="([^"#?]+)/g)) {
			const ref = m[1];
			if (/^(https?:|mailto:|data:)/.test(ref)) continue;
			const target = ref.startsWith('/') ? path.join(DIST, ref) : path.resolve(path.dirname(file), ref);
			const ok = fs.existsSync(target) || fs.existsSync(path.join(target, 'index.html'));
			if (!ok) fail(`${path.relative(ROOT, file)} links to ${ref}, which the build did not write`);
		}
	}
}

function walk(dir) {
	return fs.readdirSync(dir, { withFileTypes: true }).flatMap((d) => d.isDirectory() ? walk(path.join(dir, d.name)) : [path.join(dir, d.name)]);
}

if (problems.length) {
	console.error(`\n✗ ${problems.length} problem${problems.length > 1 ? 's' : ''}:\n`);
	for (const p of problems) console.error('  -', p);
	process.exit(1);
}
console.log(`✓ ${OVERLAYS.length} overlays, scenes current, namespace clean, links resolve`);
