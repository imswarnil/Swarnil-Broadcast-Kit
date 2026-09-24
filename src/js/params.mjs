/* URL parameters — how a streamer configures an overlay without touching a
   file. `?name=Swarnil&accent=%2300a3ff&scale=1.25`. */

export const params = new URLSearchParams(location.search);

export function get(key, fallback = '') {
	const v = params.get(key);
	return v === null || v === '' ? fallback : v;
}

export function num(key, fallback) {
	const v = parseFloat(params.get(key));
	return Number.isFinite(v) ? v : fallback;
}

export function bool(key, fallback = false) {
	const v = params.get(key);
	if (v === null) return fallback;
	return !['0', 'false', 'no', 'off', ''].includes(v.toLowerCase());
}

/* The four knobs every overlay honours, written onto <html> so the CSS
   tokens see them. */
export function applyGlobals() {
	const root = document.documentElement;
	const accent = get('accent');
	if (accent) root.style.setProperty('--tally-accent', /^[0-9a-f]{3,8}$/i.test(accent) ? `#${accent}` : accent);
	const scale = num('scale', 0);
	if (scale > 0) root.style.setProperty('--tally-scale', String(scale));
	const tone = get('tone');
	if (tone) root.dataset.tallyTone = tone;
	const font = get('font');
	if (font) root.style.setProperty('--tally-font-sans', `"${font.replace(/"/g, '')}", ${getComputedStyle(root).getPropertyValue('--tally-font-sans')}`);
	if (bool('hidden')) root.dataset.tallyHidden = 'true';
}

/* Fill text from params: <b data-tally-param="name" data-tally-default="Guest">. */
export function fill(scope = document) {
	for (const el of scope.querySelectorAll('[data-tally-param]')) {
		const key = el.dataset.tallyParam;
		const value = get(key, el.dataset.tallyDefault ?? el.textContent);
		el.textContent = value;
		if (!value && el.dataset.tallyOptional !== undefined) el.hidden = true;
	}
	for (const el of scope.querySelectorAll('[data-tally-at]')) {
		const at = get(el.dataset.tallyAt);
		if (at) el.dataset.at = at;
	}
	for (const el of scope.querySelectorAll('[data-tally-variant]')) {
		const v = get(el.dataset.tallyVariant);
		if (v) el.classList.add(`${el.dataset.tallyBase}--${v}`);
	}
}
