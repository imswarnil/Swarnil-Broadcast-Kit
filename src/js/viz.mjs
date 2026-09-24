/* Visualizer painters — each takes the canvas, a 2D context, the audio
   engine and the colours read from the canvas's CSS, and paints one frame.
   They are pure functions of (size, signal, style); the loop is shared. */

const painters = {
	bars(c, ctx, audio, s) {
		const n = s.bars;
		const data = audio.bands(n);
		const slot = c.w / n;
		const gap = slot * s.gap;
		const bw = slot - gap;
		const r = Math.min(s.radius, bw / 2);
		ctx.fillStyle = s.color;
		for (let i = 0; i < n; i++) {
			const v = Math.max(s.min, data[i]);
			const h = v * c.h;
			const x = i * slot + gap / 2;
			if (s.mirror) {
				rounded(ctx, x, c.h / 2 - h / 2, bw, h, r);
			} else {
				rounded(ctx, x, c.h - h, bw, h, r);
			}
			ctx.fill();
			if (s.cap) {
				const p = (s.peaks[i] = Math.max(v, (s.peaks[i] || 0) - s.fall));
				ctx.fillRect(x, s.mirror ? c.h / 2 - (p * c.h) / 2 - 3 * s.dpr : c.h - p * c.h - 3 * s.dpr, bw, 2 * s.dpr);
			}
		}
	},

	wave(c, ctx, audio, s) {
		const data = audio.wave();
		ctx.lineWidth = s.line;
		ctx.lineJoin = 'round';
		ctx.strokeStyle = s.color;
		ctx.beginPath();
		const step = c.w / (data.length - 1);
		for (let i = 0; i < data.length; i++) {
			const y = c.h / 2 + ((data[i] - 128) / 128) * (c.h / 2) * 0.95;
			i ? ctx.lineTo(i * step, y) : ctx.moveTo(0, y);
		}
		ctx.stroke();
	},

	ring(c, ctx, audio, s) {
		const n = s.bars;
		const data = audio.bands(n);
		const cx = c.w / 2, cy = c.h / 2;
		const R = Math.min(c.w, c.h) * 0.28;
		const reach = Math.min(c.w, c.h) * 0.2;
		ctx.strokeStyle = s.color;
		ctx.lineCap = 'round';
		ctx.lineWidth = Math.max(2 * s.dpr, ((2 * Math.PI * R) / n) * (1 - s.gap));
		for (let i = 0; i < n; i++) {
			const a = (i / n) * Math.PI * 2 - Math.PI / 2;
			const v = Math.max(s.min, data[i]);
			const r1 = R, r2 = R + v * reach;
			ctx.beginPath();
			ctx.moveTo(cx + Math.cos(a) * r1, cy + Math.sin(a) * r1);
			ctx.lineTo(cx + Math.cos(a) * r2, cy + Math.sin(a) * r2);
			ctx.stroke();
		}
		ctx.fillStyle = s.color2;
		ctx.globalAlpha = 0.9;
		ctx.beginPath();
		ctx.arc(cx, cy, R * 0.92, 0, Math.PI * 2);
		ctx.lineWidth = 1.5 * s.dpr;
		ctx.strokeStyle = s.color2;
		ctx.globalAlpha = 0.35;
		ctx.stroke();
		ctx.globalAlpha = 1;
	},

	dots(c, ctx, audio, s) {
		const n = s.bars;
		const data = audio.bands(n);
		const slot = c.w / n;
		const rows = 12;
		const rowH = c.h / rows;
		const rad = Math.min(slot, rowH) * 0.28;
		for (let i = 0; i < n; i++) {
			const lit = Math.round(Math.max(s.min, data[i]) * rows);
			for (let j = 0; j < rows; j++) {
				ctx.fillStyle = j < lit ? s.color : s.color2;
				ctx.globalAlpha = j < lit ? 1 : 0.14;
				ctx.beginPath();
				ctx.arc(i * slot + slot / 2, c.h - (j + 0.5) * rowH, rad, 0, Math.PI * 2);
				ctx.fill();
			}
		}
		ctx.globalAlpha = 1;
	},
};

function rounded(ctx, x, y, w, h, r) {
	ctx.beginPath();
	if (ctx.roundRect) ctx.roundRect(x, y, w, h, r);
	else ctx.rect(x, y, w, h);
}

export function mount(canvas, audio, { style = 'bars', bars = 48 } = {}) {
	const ctx = canvas.getContext('2d');
	const paint = painters[style] || painters.bars;
	const state = { bars, peaks: [], fall: 0.01 };
	const box = { w: 0, h: 0 };

	function measure() {
		const dpr = window.devicePixelRatio || 1;
		const rect = canvas.getBoundingClientRect();
		box.w = Math.max(1, Math.round(rect.width * dpr));
		box.h = Math.max(1, Math.round(rect.height * dpr));
		canvas.width = box.w;
		canvas.height = box.h;
		const cs = getComputedStyle(canvas);
		state.dpr = dpr;
		state.color = cs.getPropertyValue('--tally-viz-color').trim() || '#f5273f';
		state.color2 = cs.getPropertyValue('--tally-viz-color-2').trim() || '#fff';
		state.gap = clamp(parseFloat(cs.getPropertyValue('--tally-viz-gap')) || 0.35, 0, 0.8);
		state.radius = (parseFloat(cs.getPropertyValue('--tally-viz-radius')) || 3) * dpr;
		state.line = (parseFloat(cs.getPropertyValue('--tally-viz-line')) || 3) * dpr;
		state.min = clamp(parseFloat(cs.getPropertyValue('--tally-viz-min')) || 0.04, 0, 1);
		state.mirror = canvas.classList.contains('tally-viz--mirror');
		state.cap = canvas.classList.contains('tally-viz--cap');
	}

	measure();
	new ResizeObserver(measure).observe(canvas);
	/* Colours can change after mount (a `?accent=` applies at load, a tone
	   later); re-read them once a second rather than every frame. */
	const recolor = setInterval(measure, 1000);

	let raf = 0;
	function frame() {
		ctx.clearRect(0, 0, box.w, box.h);
		paint(box, ctx, audio, state);
		raf = requestAnimationFrame(frame);
	}
	frame();

	return () => { cancelAnimationFrame(raf); clearInterval(recolor); };
}

const clamp = (v, a, b) => Math.min(b, Math.max(a, v));
