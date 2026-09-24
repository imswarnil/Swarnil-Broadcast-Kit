/* The site's own behaviour: scale previews to fit, run the URL builder,
   copy the URL. */

function fitPreviews() {
	for (const p of document.querySelectorAll('[data-preview]')) {
		const w = parseFloat(getComputedStyle(p).getPropertyValue('--pw'));
		p.style.setProperty('--scale', (p.clientWidth / w).toFixed(5));
	}
}
new ResizeObserver(fitPreviews).observe(document.body);
fitPreviews();

for (const b of document.querySelectorAll('[data-builder]')) {
	const form = b.querySelector('[data-builder-form]');
	const iframe = b.querySelector('[data-builder-preview] iframe');
	const out = b.querySelector('#url-out');
	const slug = b.dataset.builder;
	const base = b.dataset.base;
	const demo = JSON.parse(b.dataset.previewDemo || '{}');
	let registry = null;
	fetch('/registry.json').then((r) => r.json()).then((r) => { registry = r; update(); });

	function values() {
		const v = {};
		for (const el of form.elements) {
			if (!el.name || el.dataset.mirror) continue;
			if (el.type === 'checkbox') v[el.name] = el.checked;
			else v[el.name] = el.value;
		}
		return v;
	}
	function build(v, extra = {}) {
		const o = registry.overlays.find((x) => x.slug === slug);
		const all = [...registry.globals, ...o.params];
		const q = new URLSearchParams();
		for (const p of all) {
			const val = v[p.key];
			if (val === undefined || val === '' || val === false) continue;
			if (String(val) === String(p.default)) continue;
			q.set(p.key, val === true ? '1' : String(val).replace(/^#/, ''));
		}
		for (const [k, val] of Object.entries(extra)) if (!q.has(k)) q.set(k, String(val));
		const s = q.toString();
		return `${slug}/${s ? `?${s}` : ''}`;
	}
	let t;
	function update() {
		if (!registry) return;
		const v = values();
		out.value = base + build(v);
		clearTimeout(t);
		t = setTimeout(() => { iframe.src = '/' + build(v, demo); }, 250);
	}
	form.addEventListener('input', (e) => {
		const el = e.target;
		if (el.type === 'color') { const m = form.querySelector(`[data-mirror="${el.id}"]`); if (m) m.value = el.value; }
		if (el.dataset.mirror) { const c = document.getElementById(el.dataset.mirror); if (c && /^#[0-9a-f]{6}$/i.test(el.value)) c.value = el.value; }
		update();
	});
}

for (const btn of document.querySelectorAll('[data-copy]')) {
	btn.addEventListener('click', async () => {
		const el = document.querySelector(btn.dataset.copy);
		try { await navigator.clipboard.writeText(el.value); } catch { el.select(); document.execCommand('copy'); }
		const was = btn.textContent;
		btn.textContent = 'Copied';
		setTimeout(() => (btn.textContent = was), 1400);
	});
}
