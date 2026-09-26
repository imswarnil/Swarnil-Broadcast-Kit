/*  Turn the newest self-test run into the pictures the site uses.

    The walk writes one PNG per scene, in show order, into the profile's
    recording folder. This pairs them with the scene list the site publishes and
    writes them out as the JPEGs `docs/screens/` holds — which is the step that
    used to be a line of shell nobody could remember and that quietly went wrong
    when the scene order changed.

    It refuses rather than guesses if the counts disagree, because a run that is
    one short silently shifts every picture after the gap by one, and the result
    looks plausible.  */

import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { execFileSync } from 'node:child_process';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const { SCENES } = await import(path.join(ROOT, 'site/content.mjs'));

const profile = path.join(
	os.homedir(),
	'Library/Application Support/obs-studio/basic/profiles/Swarnil Broadcast Kit/basic.ini'
);
let dir = path.join(os.homedir(), 'Movies');
if (fs.existsSync(profile)) {
	const m = fs.readFileSync(profile, 'utf8').match(/^FilePath=(.*)$/m);
	if (m) dir = m[1].trim();
}
if (!fs.existsSync(dir)) {
	console.error(`✗ no such folder: ${dir}`);
	process.exit(1);
}

const shots = fs
	.readdirSync(dir)
	.filter((f) => f.toLowerCase().endsWith('.png'))
	.map((f) => ({ f, t: fs.statSync(path.join(dir, f)).mtimeMs }))
	.sort((a, b) => a.t - b.t)
	.map((x) => x.f);

if (shots.length !== SCENES.length) {
	console.error(`✗ ${shots.length} screenshots in ${dir}, but the site lists ${SCENES.length} scenes.`);
	console.error('  Pairing them anyway would shift every picture after the gap, so nothing was written.');
	console.error('  Clear the folder, run `make selftest`, start OBS, and let the whole walk finish.');
	process.exit(1);
}

const out = path.join(ROOT, 'docs/screens');
fs.mkdirSync(out, { recursive: true });
SCENES.forEach((scene, i) => {
	execFileSync('sips', [
		'-s', 'format', 'jpeg',
		'-s', 'formatOptions', '82',
		'-Z', '1600',
		path.join(dir, shots[i]),
		'--out', path.join(out, `${scene.img}.jpg`),
	], { stdio: 'ignore' });
});

console.log(`✓ ${SCENES.length} screenshots → docs/screens/`);
console.log('  Look at them before committing. A run with a person or a desktop in it does not ship.');
