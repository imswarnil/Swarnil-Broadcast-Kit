/* Pack — zips the installable half of dist/ for the GitHub release: every
   overlay page, the runtime, the fonts, the scene collection and the profile,
   with a README on how to use them offline. Needs `zip` on the PATH. */

import fs from 'node:fs';
import path from 'node:path';
import { execFileSync } from 'node:child_process';
import { ROOT, DIST, pkg, copy, write } from './lib.mjs';
import { OVERLAYS } from '../overlays/registry.mjs';

const stage = path.join(DIST, 'pack-stage', `tally-${pkg.version}`);
fs.rmSync(path.join(DIST, 'pack-stage'), { recursive: true, force: true });

for (const f of ['assets/tally.css', 'assets/tally.js', 'assets/fonts/Geist-Variable.woff2', 'assets/fonts/GeistMono-Variable.woff2']) copy(path.join(DIST, f), path.join(stage, f));
for (const o of OVERLAYS) copy(path.join(DIST, o.slug, 'index.html'), path.join(stage, o.slug, 'index.html'));
copy(path.join(DIST, 'scenes/Tally.json'), path.join(stage, 'scenes/Tally.json'));
copy(path.join(DIST, 'scenes/profile/Tally/basic.ini'), path.join(stage, 'scenes/profile/Tally/basic.ini'));
write(path.join(stage, 'README.txt'), `Tally ${pkg.version} — OBS overlays
https://obs.imswarnil.com

Hosted (recommended): add a Browser Source with a URL from the site. Nothing
in this zip is needed for that.

Offline: keep this folder together. In a Browser Source tick "Local file" and
pick one of:
${OVERLAYS.map((o) => `  ${o.slug}/index.html   (${o.size.w}×${o.size.h})`).join('\n')}
The URL parameters described on the site work on a local file too — OBS
does not let you type them for a local file, so use the hosted URL, or edit
the data-tally-default attributes in the HTML.

scenes/Tally.json          Scene Collection → Import (uses the hosted URLs)
scenes/profile/Tally/      Profile → Import, pick this folder (1080p60)

MIT — do what you like, keep the notice.
`);
copy(path.join(ROOT, 'LICENSE'), path.join(stage, 'LICENSE'));

const out = path.join(DIST, 'pack', `tally-${pkg.version}.zip`);
fs.mkdirSync(path.dirname(out), { recursive: true });
execFileSync('zip', ['-qr', out, `tally-${pkg.version}`], { cwd: path.dirname(stage) });
fs.rmSync(path.join(DIST, 'pack-stage'), { recursive: true, force: true });
console.log(`packed → ${path.relative(ROOT, out)} (${(fs.statSync(out).size / 1024).toFixed(0)} KB)`);
