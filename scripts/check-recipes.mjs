/*  Every JSON block in the skill's reference must actually build.

    A worked example that does not validate is worse than none: somebody copies
    it, OBS ignores the key it got wrong, and the scene is subtly not what the
    page promised. So the examples are run through the same validator the skill
    tells an agent to use, on every build.  */

import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import { fileURLToPath } from 'node:url';
import { execFileSync } from 'node:child_process';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const BUILD = path.join(ROOT, 'skills/sbk-scenes/scripts/build-collection.mjs');

const specs = [];
for (const rel of ['skills/sbk-scenes/references/recipes.md', 'skills/sbk-scenes/SKILL.md']) {
	const md = fs.readFileSync(path.join(ROOT, rel), 'utf8');
	for (const [, block] of md.matchAll(/```json\n([\s\S]*?)\n```/g)) {
		let parsed;
		try {
			parsed = JSON.parse(block);
		} catch (e) {
			console.error(`✗ ${rel}: a JSON block does not parse — ${e.message}`);
			process.exit(1);
		}
		/* a block is either a whole spec or a single scene */
		specs.push({ rel, spec: parsed.scenes ? parsed : { name: 'check', scenes: [parsed] } });
	}
}
for (const [dir, label] of [
	['skills/sbk-scenes/examples', 'examples'],
	['presets', 'presets'],
])
	for (const f of fs.readdirSync(path.join(ROOT, dir)).filter((f) => f.endsWith('.json')))
		specs.push({ rel: `${label}/${f}`, spec: JSON.parse(fs.readFileSync(path.join(ROOT, dir, f), 'utf8')) });

let bad = 0;
for (const { rel, spec } of specs) {
	const tmp = path.join(os.tmpdir(), `sbk-check-${Math.random().toString(36).slice(2)}.json`);
	fs.writeFileSync(tmp, JSON.stringify(spec));
	try {
		execFileSync('node', [BUILD, tmp, '--check'], { stdio: 'pipe' });
	} catch (e) {
		bad++;
		console.error(`✗ ${rel} — ${spec.scenes[0]?.name}`);
		console.error((e.stderr || e.stdout || '').toString().trimEnd());
	}
	fs.unlinkSync(tmp);
}
if (bad) process.exit(1);
console.log(`✓ ${specs.length} skill examples build`);
