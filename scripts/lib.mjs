import { fileURLToPath } from 'node:url';
import path from 'node:path';
import fs from 'node:fs';

export const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
export const DIST = path.join(ROOT, 'dist');
export const pkg = JSON.parse(fs.readFileSync(path.join(ROOT, 'package.json'), 'utf8'));

/* The site's public base. CI sets SITE_URL; locally it is the dev server. */
export const BASE = (process.env.SITE_URL || 'http://localhost:4800/').replace(/\/?$/, '/');

export const rel = (p) => path.relative(ROOT, p) || '.';
export const read = (p) => fs.readFileSync(p, 'utf8');
export function write(p, s) {
	fs.mkdirSync(path.dirname(p), { recursive: true });
	fs.writeFileSync(p, s);
}
export function copy(from, to) {
	fs.mkdirSync(path.dirname(to), { recursive: true });
	fs.copyFileSync(from, to);
}
export const log = (...a) => console.log('  ·', ...a);
