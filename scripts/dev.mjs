/* Dev — rebuilds on any change and serves dist/ on http://localhost:4800.
   Each rebuild runs in a child process so an edited build script is picked
   up too (an import would be cached for the life of this process). */

import fs from 'node:fs';
import http from 'node:http';
import path from 'node:path';
import { spawn } from 'node:child_process';
import { ROOT, DIST } from './lib.mjs';

const PORT = Number(process.env.PORT || 4800);
const WATCH = ['src', 'overlays', 'site', 'scenes', 'assets', 'scripts'];

let building = null, queued = false;
function build() {
	if (building) { queued = true; return; }
	building = spawn(process.execPath, [path.join(ROOT, 'scripts/build.mjs')], { stdio: 'inherit', env: { ...process.env, SITE_URL: `http://localhost:${PORT}/` } });
	building.on('exit', () => { building = null; if (queued) { queued = false; build(); } });
}

let timer;
for (const dir of WATCH) {
	fs.watch(path.join(ROOT, dir), { recursive: true }, () => {
		clearTimeout(timer);
		timer = setTimeout(build, 120);
	});
}
build();

const TYPES = { '.html': 'text/html; charset=utf-8', '.css': 'text/css', '.js': 'text/javascript', '.mjs': 'text/javascript', '.json': 'application/json', '.svg': 'image/svg+xml', '.woff2': 'font/woff2', '.ini': 'text/plain', '.txt': 'text/plain', '.xml': 'application/xml', '.zip': 'application/zip', '.png': 'image/png' };

http.createServer((req, res) => {
	let p = decodeURIComponent(new URL(req.url, 'http://x').pathname);
	if (p.endsWith('/')) p += 'index.html';
	let file = path.join(DIST, p);
	if (!file.startsWith(DIST)) { res.writeHead(403); return res.end(); }
	if (!fs.existsSync(file) && fs.existsSync(file + '/index.html')) { res.writeHead(301, { Location: p + '/' }); return res.end(); }
	if (!fs.existsSync(file) || fs.statSync(file).isDirectory()) {
		res.writeHead(404, { 'Content-Type': 'text/html' });
		return res.end(fs.existsSync(path.join(DIST, '404.html')) ? fs.readFileSync(path.join(DIST, '404.html')) : 'Not found');
	}
	res.writeHead(200, { 'Content-Type': TYPES[path.extname(file)] || 'application/octet-stream', 'Cache-Control': 'no-store', 'Access-Control-Allow-Origin': '*' });
	fs.createReadStream(file).pipe(res);
}).listen(PORT, () => console.log(`\n  Tally dev → http://localhost:${PORT}/\n`));
