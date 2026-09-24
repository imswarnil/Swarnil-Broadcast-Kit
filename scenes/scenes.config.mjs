/* The scene collection Tally ships — four scenes, built from the overlays.
   Positions are on a 1920×1080 canvas. `npm run scenes` turns this into
   scenes/Tally.json, the file OBS imports. Add an overlay to a scene here;
   never edit the JSON by hand. */

export const collection = {
	name: 'Tally',
	canvas: { w: 1920, h: 1080 },
	scenes: [
		{
			name: 'Starting soon',
			items: [
				{ overlay: 'starting-soon', params: { at: '15m' } },
			],
		},
		{
			name: 'Live',
			items: [
				{ overlay: 'ticker', pos: { x: 0, y: 960 } },
				{ overlay: 'lower-third' },
				{ overlay: 'frame', name: 'Tally · Webcam frame', pos: { x: 1232, y: 48 }, size: { w: 640, h: 400 }, params: { w: 640, h: 360 } },
				{ overlay: 'onair' },
			],
		},
		{
			name: 'Be right back',
			items: [
				{ overlay: 'brb' },
			],
		},
		{
			name: 'Ending',
			items: [
				{ overlay: 'brb', name: 'Tally · Ending', params: { eyebrow: 'That is a wrap', title: 'Thanks for watching', body: 'Subscribe for the next one.', style: 'dots' } },
			],
		},
	],
};
