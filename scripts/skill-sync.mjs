/*  Regenerate the agent skill's registry from the C sources.

    The skill tells an agent which sources exist and what settings each one
    takes. Written by hand, that list would be wrong within a release — a key
    renamed in a shader has no reason to remind anyone to edit a markdown file.
    So it is read out of `src/*.c` instead: the ids from `obs_source_info`, the
    names from the locale, the keys and their defaults from `get_defaults`, the
    allowed values from the property lists, the ranges from the sliders.

    Anything the parser cannot see, the skill does not claim. That is the point:
    a registry that is incomplete is recoverable, and one that is confidently
    wrong sends somebody to OBS to find a source that will not take the setting
    they were told to give it.

        node scripts/skill-sync.mjs           write it
        node scripts/skill-sync.mjs --check   fail if it is out of date
*/

import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import crypto from 'node:crypto';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const SKILL = path.join(ROOT, 'skills/sbk-scenes');
const CHECK = process.argv.includes('--check');

const read = (p) => fs.readFileSync(path.join(ROOT, p), 'utf8');

/* ---- the locale, for the name OBS shows in the "+" menu ------------------- */

const locale = {};
for (const line of read('data/locale/en-US.ini').split('\n')) {
	const m = line.match(/^([\w.]+)="(.*)"$/);
	if (m) locale[m[1]] = m[2];
}

/* ---- the shared groups every source mixes in ------------------------------ */

/* Look and Motion are added by a helper rather than written out per source, so
   the per-file parse cannot see them. They are read once, from the headers that
   define them, and attached to everything that calls the helper. */
function sharedGroup(file, fn) {
	const src = read(file);
	const body = src.slice(src.indexOf(`static inline void ${fn}`));
	return parseDefaults(body.slice(0, body.indexOf('\n}\n')));
}

function parseDefaults(body) {
	const out = {};
	const re = /obs_data_set_default_(string|int|double|bool)\(\s*\w+\s*,\s*"([\w.]+)"\s*,\s*([^;]+?)\);/g;
	let m;
	while ((m = re.exec(body))) {
		const [, kind, key, raw] = m;
		let value = raw.trim();
		if (kind === 'string') {
			const q = value.match(/^"((?:[^"\\]|\\.)*)"$/s);
			value = q ? q[1].replace(/\\n/g, '\n').replace(/\\"/g, '"') : value;
			/* a default built from several adjacent string literals */
			if (!q && /^"/.test(raw.trim()))
				value = [...raw.matchAll(/"((?:[^"\\]|\\.)*)"/g)]
					.map((x) => x[1])
					.join('')
					.replace(/\\n/g, '\n')
					.replace(/\\"/g, '"');
		} else if (kind === 'bool') {
			value = value === 'true';
		} else {
			/* a cast or a macro resolves; anything computed at runtime does not,
			   and is left out rather than printed as C nobody can paste */
			const bare = value.replace(/^\((?:long long|int|float|double|uint32_t)\)\s*/, '').replace(/u?$/, '');
			if (/^-?[\d.]+f?$/.test(bare)) value = Number(bare.replace(/f$/, ''));
			else if (/^0x[0-9A-Fa-f]+$/.test(bare)) value = parseInt(bare, 16);
			else if (TOKENS[bare] !== undefined) value = TOKENS[bare];
			else value = undefined;
		}
		out[key] = { type: kind, default: value };
	}
	return out;
}

/* ---- the allowed values, from the property lists -------------------------- */

function parseChoices(body) {
	/* obs_property_t *m = obs_properties_add_list(p, "mode", …) binds m to "mode" */
	const varToKey = {};
	for (const m of body.matchAll(/obs_property_t\s*\*(\w+)\s*=\s*obs_properties_add_list\(\s*\w+\s*,\s*"([\w.]+)"/g))
		varToKey[m[1]] = m[2];

	const out = {};
	for (const m of body.matchAll(/obs_property_list_add_string\(\s*(\w+)\s*,\s*"((?:[^"\\]|\\.)*)"\s*,\s*"([\w.-]*)"\s*\)/g)) {
		const key = varToKey[m[1]];
		if (!key) continue;
		(out[key] ||= []).push({ value: m[3], label: m[2] });
	}

	/* the two helpers that fill a list from a table elsewhere */
	for (const m of body.matchAll(/sbk_surface_list\(\s*\w+\s*,\s*"([\w.]+)"/g))
		out[m[1]] = SURFACES.map((v) => ({ value: v, label: v }));
	for (const m of body.matchAll(/sbk_glyph_list\(\s*(\w+)\s*\)/g)) {
		const key = varToKey[m[1]];
		if (key) out[key] = GLYPHS.map((v) => ({ value: v, label: v }));
	}
	return out;
}

function parseRanges(body) {
	const out = {};
	for (const m of body.matchAll(
		/obs_properties_add_(int|float)(?:_slider)?\(\s*\w+\s*,\s*"([\w.]+)"\s*,\s*"[^"]*"\s*,\s*([-\d.f]+)\s*,\s*([-\d.f]+)/g
	))
		out[m[2]] = { min: Number(m[3].replace(/f$/, '')), max: Number(m[4].replace(/f$/, '')) };
	return out;
}

/* the two value tables that live in headers, not in a source's own properties */
/* the colour tokens, so a default reads as a number rather than as a macro */
const TOKENS = {};
for (const f of ['src/sbk-common.h', 'src/sbk-anim.h', 'src/sbk-glyph.h'])
	for (const m of read(f).matchAll(/#define (SBK_[A-Z0-9_]+)\s+(0x[0-9A-Fa-f]+u?|-?[\d.]+f?)\b/g))
		TOKENS[m[1]] = /^0x/.test(m[2]) ? parseInt(m[2], 16) : Number(m[2].replace(/f$/, ''));

const SURFACES = (() => {
	const src = read('src/sbk-common.h');
	const fn = src.slice(src.indexOf('static inline void sbk_surface_list'));
	return [...fn.slice(0, fn.indexOf('\n}\n')).matchAll(/obs_property_list_add_string\(\s*\w+\s*,\s*"[^"]*"\s*,\s*"([\w-]+)"\)/g)].map(
		(m) => m[1]
	);
})();
const GLYPHS = [...read('src/sbk-glyph.h').matchAll(/\{"([\w-]+)",\s*"[^"]*",\s*SBK_GLYPH_/g)].map((m) => m[1]);
const ASPECTS = [...read('src/source-frame.c').matchAll(/\{"([\w×x]+)",\s*"[^"]*",\s*(\d+),\s*(\d+)\}/g)].map((m) => ({
	id: m[1],
	w: Number(m[2]),
	h: Number(m[3]),
}));
const PLATFORMS = [...read('src/sbk-glyph.h').matchAll(/\{"([\w]+)",\s*"([^"]*)",\s*"([^"]*)",\s*"([\w]*)",\s*(0x[0-9A-Fa-fu]+|0u)\}/g)].map(
	(m) => ({ id: m[1], label: m[2] })
);

const LOOK = sharedGroup('src/sbk-common.h', 'sbk_look_defaults');
const ANIM = sharedGroup('src/sbk-anim.h', 'sbk_anim_defaults');
const ANIM_CHOICES = parseChoices(read('src/sbk-anim.h'));

/* ---- every source, filter and transition ---------------------------------- */

const KIND = { source: 'Source', filter: 'Filter', transition: 'Transition' };

function collect() {
	const files = fs
		.readdirSync(path.join(ROOT, 'src'))
		.filter((f) => /^(source|filter|transition)-.+\.c$/.test(f))
		.sort();

	const out = [];
	for (const f of files) {
		const body = read(`src/${f}`);
		const idm = body.match(/\.id\s*=\s*"(\w+)"/);
		if (!idm) continue;
		const id = idm[1];

		const typem = body.match(/\.type\s*=\s*OBS_SOURCE_TYPE_(\w+)/);
		const type = typem ? typem[1] : 'INPUT';
		const outputm = body.match(/\.output_flags\s*=\s*([^,]+),/);
		const audio = outputm ? /OBS_SOURCE_AUDIO/.test(outputm[1]) : false;

		const namem = body.match(/obs_module_text\("([\w.]+)"\)/);
		const name = namem ? locale[namem[1]] || namem[1] : id;

		/* the defaults function is the honest list of keys — a property with no
		   default is one nothing reads */
		const dstart = body.search(/static void \w+_defaults\(obs_data_t \*\w+\)/);
		const dbody = dstart < 0 ? '' : body.slice(dstart, body.indexOf('\n}\n', dstart));
		const settings = parseDefaults(dbody);

		const pstart = body.search(/static obs_properties_t \*\w+_properties\(/);
		const pbody = pstart < 0 ? '' : body.slice(pstart, body.indexOf('\n}\n', pstart));
		const choices = parseChoices(pbody);
		const ranges = parseRanges(pbody);

		for (const [k, v] of Object.entries(choices)) if (settings[k]) settings[k].values = v.map((x) => x.value);
		for (const [k, v] of Object.entries(ranges)) if (settings[k]) Object.assign(settings[k], v);

		const shares = { look: /sbk_look_defaults/.test(dbody), anim: /sbk_anim_defaults/.test(dbody) };
		if (shares.look) for (const [k, v] of Object.entries(LOOK)) settings[k] ||= { ...v, shared: 'look' };
		const enterm = dbody.match(/sbk_anim_defaults\(\s*\w+\s*,\s*"([\w]+)"\s*\)/);
		if (shares.anim)
			for (let [k, v] of Object.entries(ANIM)) {
				if (k === 'enter' && enterm) v = { ...v, default: enterm[1] };
				settings[k] ||= { ...v, shared: 'motion' };
				if (ANIM_CHOICES[k]) settings[k].values = ANIM_CHOICES[k].map((x) => x.value);
			}

		const hotkeys = [...body.matchAll(/obs_hotkey_register_source\(\s*\w+\s*,\s*"([\w.]+)"\s*,\s*"([^"]*)"/g)].map(
			(m) => ({ id: m[1], label: m[2] })
		);

		out.push({ id, name, kind: KIND[f.split('-')[0]], type, audio, file: `src/${f}`, hotkeys, settings });
	}
	return out;
}

/* ---- write it out ---------------------------------------------------------- */

const entries = collect();

/*  Stamped with a digest of the sources, not with the git commit.

    A commit stamp invalidates itself: regenerating after the commit that
    carried the last regeneration produces a different file, so the check fails
    on a clean tree and CI can never be green. A digest of the files the parser
    actually read changes exactly when the answer changes, which is the only
    thing the stamp was ever for.  */
const head = crypto
	.createHash('sha256')
	.update(
		entries
			.map((e) => e.file)
			.concat(['src/sbk-common.h', 'src/sbk-anim.h', 'src/sbk-glyph.h', 'data/locale/en-US.ini'])
			.sort()
			.map((f) => `${f}\n${read(f)}`)
			.join('\n')
	)
	.digest('hex')
	.slice(0, 12);

const registry = {
	generated_by: 'scripts/skill-sync.mjs',
	source_digest: head,
	canvas: { width: 1920, height: 1080 },
	aspects: ASPECTS,
	surfaces: SURFACES,
	glyphs: GLYPHS,
	platforms: PLATFORMS,
	sources: entries,
};

function markdown() {
	const L = [];
	L.push('<!-- GENERATED by scripts/skill-sync.mjs — do not edit. Run `npm run skill:sync`. -->');
	L.push('');
	L.push('# Every source, and every setting it takes');
	L.push('');
	L.push('Read out of the C. A key that is not here is a key no source reads.');
L.push('');
L.push(`<sub>sources digest \`${head}\` — regenerate with <code>node scripts/skill-sync.mjs</code></sub>`);
	L.push('');
	L.push('Colours are **ABGR** integers, not hex strings: `0xFF3F27F5` is the accent red.');
	L.push('In JSON write them as decimal, which is what OBS itself stores.');
	L.push('');

	const aspectLine = ASPECTS.filter((a) => a.w).map((a) => `\`${a.id}\` ${a.w}×${a.h}`).join(' · ');
	L.push(`**Aspect shapes** (the base box at size 1.0): ${aspectLine}. A frame or a plate at`);
	L.push('`size: k` is `w × k` by `h × k`. Use those numbers when you give a camera or a');
	L.push('capture a bounding box, and when you place a plate — see `references/layout.md`.');
	L.push('');

	for (const kind of ['Source', 'Filter', 'Transition']) {
		const group = entries.filter((e) => e.kind === kind);
		if (!group.length) continue;
		L.push(`## ${kind}s`);
		L.push('');
		for (const e of group) {
			L.push(`### \`${e.id}\` — ${e.name}`);
			L.push('');
			if (e.hotkeys.length) L.push(`Hotkeys: ${e.hotkeys.map((h) => `*${h.label}*`).join(', ')}.`);
			if (e.audio) L.push('Processes audio.');
			if (e.hotkeys.length || e.audio) L.push('');
			L.push('| Key | Type | Default | Values / range |');
			L.push('| --- | --- | --- | --- |');
			const own = Object.entries(e.settings).filter(([, v]) => !v.shared);
			const shared = Object.entries(e.settings).filter(([, v]) => v.shared);
			for (const [k, v] of [...own, ...shared]) {
				if (v.default === undefined) { L.push(`| \`${k}\`${v.shared ? ` *(${v.shared})*` : ''} | ${v.type} | *set at runtime* | ${v.values ? v.values.map((x) => `\`${x}\``).join(', ') : v.min !== undefined ? `${v.min} … ${v.max}` : ''} |`); continue; }
				let d = typeof v.default === 'string' ? v.default.split('\n')[0] : String(v.default);
				if (typeof v.default === 'string' && v.default.includes('\n')) d += ' …';
				if (d.length > 42) d = d.slice(0, 41) + '…';
				let range = '';
				if (v.values) range = v.values.map((x) => `\`${x}\``).join(', ');
				else if (v.min !== undefined) range = `${v.min} … ${v.max}`;
				const mark = v.shared ? ` *(${v.shared})*` : '';
				L.push(`| \`${k}\`${mark} | ${v.type} | ${d === 'undefined' ? '—' : `\`${d}\``} | ${range} |`);
			}
			L.push('');
		}
	}

	L.push('## The shared groups');
	L.push('');
	L.push('Everything marked *(look)* or *(motion)* comes from a helper, so it is on every');
	L.push('source that mixes it in. Give every source in a scene the same `accent`, `scale`,');
	L.push('`face` and `variant` and the scene changes together — that is the whole trick.');
	L.push('');
	L.push('## Platform presets');
	L.push('');
	L.push(`\`sbk_social\` knows: ${PLATFORMS.filter((p) => p.id !== 'custom').map((p) => `\`${p.id}\``).join(', ')}.`);
	L.push('Write accounts one per line as `platform: handle`. Anything else on the left of');
	L.push('the colon is used as a plain name.');
	L.push('');
	return L.join('\n');
}

const files = {
	'references/sources.md': markdown(),
	'scripts/registry.json': JSON.stringify(registry, null, '\t') + '\n',
};

let stale = [];
for (const [rel, content] of Object.entries(files)) {
	const abs = path.join(SKILL, rel);
	const old = fs.existsSync(abs) ? fs.readFileSync(abs, 'utf8') : null;
	if (old === content) continue;
	stale.push(rel);
	if (!CHECK) {
		fs.mkdirSync(path.dirname(abs), { recursive: true });
		fs.writeFileSync(abs, content);
	}
}

if (CHECK) {
	if (stale.length) {
		console.error(`✗ the skill registry is out of date: ${stale.join(', ')}`);
		console.error('  run `npm run skill:sync` and commit the result');
		process.exit(1);
	}
	console.log(`✓ skill registry current — ${entries.length} registrations`);
} else {
	console.log(
		`skill registry → skills/sbk-scenes/ — ${entries.length} registrations, ` +
			`${entries.reduce((n, e) => n + Object.keys(e.settings).length, 0)} settings, at ${head}`
	);
}
