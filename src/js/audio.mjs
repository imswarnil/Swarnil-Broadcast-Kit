/* Audio engine — the microphone through one AnalyserNode, or a synthetic
   signal when no microphone can be had. The synthetic one is plain maths on
   the clock, NOT an AudioContext: a browser suspends audio until someone
   clicks, and a preview or a Browser Source must move without a click.
   `source: 'demo'` forces the synthetic signal. */

export async function createAudio({ source = 'mic', fft = 2048, smoothing = 0.8 } = {}) {
	if (source !== 'demo' && navigator.mediaDevices?.getUserMedia) {
		try {
			const stream = await navigator.mediaDevices.getUserMedia({
				audio: { echoCancellation: false, noiseSuppression: false, autoGainControl: false },
				video: false,
			});
			const ctx = new (window.AudioContext || window.webkitAudioContext)();
			const analyser = ctx.createAnalyser();
			analyser.fftSize = fft;
			analyser.smoothingTimeConstant = smoothing;
			ctx.createMediaStreamSource(stream).connect(analyser);
			if (ctx.state === 'suspended') ctx.resume().catch(() => {});
			return mic(ctx, analyser, stream);
		} catch {
			/* no device, no permission — fall through to the synthetic signal */
		}
	}
	return demo({ fft, smoothing });
}

function mic(ctx, analyser, stream) {
	const freq = new Uint8Array(analyser.frequencyBinCount);
	const time = new Uint8Array(analyser.fftSize);
	const nyquist = ctx.sampleRate / 2;
	return {
		mode: 'mic',
		bands: (n, opts) => logBands(n, opts, (f) => { analyser.getByteFrequencyData(freq); return { data: freq, hz: nyquist }; }),
		wave() { analyser.getByteTimeDomainData(time); return time; },
		level() {
			analyser.getByteFrequencyData(freq);
			let s = 0;
			for (let i = 0; i < freq.length; i++) s += freq[i];
			return s / (freq.length * 255);
		},
		stop() { stream.getTracks().forEach((t) => t.stop()); ctx.close(); },
	};
}

/* A believable fake: a bass pulse on a beat, a wandering mid, a hiss, all
   under a slow "phrase" envelope so it breathes like speech over music. */
function demo({ fft, smoothing }) {
	const bins = fft / 2;
	const freq = new Uint8Array(bins);
	const time = new Uint8Array(fft);
	const smooth = new Float32Array(bins);
	const hz = 24000;
	const t0 = performance.now();

	function spectrum() {
		const t = (performance.now() - t0) / 1000;
		const beat = Math.pow(Math.max(0, Math.sin(t * Math.PI * 2 * 1.9)), 6);
		const phrase = 0.55 + 0.45 * Math.sin(t * 0.37) * Math.sin(t * 0.11 + 1);
		for (let i = 0; i < bins; i++) {
			const f = (i / bins) * hz;
			const lf = Math.log10(Math.max(20, f));
			let v = 0;
			v += beat * Math.exp(-Math.pow((lf - Math.log10(70)) / 0.25, 2));          // kick
			v += 0.55 * phrase * Math.exp(-Math.pow((lf - Math.log10(240 + 120 * Math.sin(t * 0.9))) / 0.35, 2)); // voice
			v += 0.35 * Math.exp(-Math.pow((lf - Math.log10(1800 + 900 * Math.sin(t * 1.7))) / 0.45, 2)) * (0.6 + 0.4 * Math.sin(t * 5.3)); // mid sparkle
			v += 0.18 * Math.exp(-Math.pow((lf - Math.log10(7000)) / 0.5, 2)) * (0.5 + 0.5 * Math.random()); // hiss
			v += 0.03 * Math.random();
			v *= 1 - Math.min(1, Math.max(0, (lf - 3.2) / 1.4)) * 0.8;                // natural roll-off
			const target = Math.min(1, v);
			smooth[i] = smooth[i] * smoothing + target * (1 - smoothing);
			freq[i] = Math.round(smooth[i] * 255);
		}
		return { data: freq, hz };
	}

	return {
		mode: 'demo',
		bands: (n, opts) => logBands(n, opts, spectrum),
		wave() {
			const t = (performance.now() - t0) / 1000;
			const amp = 0.25 + 0.55 * Math.abs(Math.sin(t * 0.7)) * (0.7 + 0.3 * Math.sin(t * 4.1));
			for (let i = 0; i < fft; i++) {
				const x = i / fft;
				const s = Math.sin(x * 26 + t * 9) * 0.6 + Math.sin(x * 61 + t * 14) * 0.3 + (Math.random() - 0.5) * 0.15;
				time[i] = Math.round(128 + s * amp * 120);
			}
			return time;
		},
		level() {
			const { data } = spectrum();
			let s = 0;
			for (let i = 0; i < data.length; i++) s += data[i];
			return s / (data.length * 255);
		},
		stop() {},
	};
}

/* n bands, 0–1 each, spread on a log scale so bass does not eat the whole
   left half of the picture. */
function logBands(n, { floor = 30, ceil = 16000 } = {}, source) {
	const { data, hz } = source();
	const out = new Float32Array(n);
	const lo = Math.log(floor), hi = Math.log(ceil);
	for (let i = 0; i < n; i++) {
		const f0 = Math.exp(lo + ((hi - lo) * i) / n);
		const f1 = Math.exp(lo + ((hi - lo) * (i + 1)) / n);
		const a = Math.floor((f0 / hz) * data.length);
		const b = Math.max(a + 1, Math.floor((f1 / hz) * data.length));
		let sum = 0;
		for (let k = a; k < b && k < data.length; k++) sum += data[k];
		out[i] = sum / ((b - a) * 255);
	}
	return out;
}
