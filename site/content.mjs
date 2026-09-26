/*  Everything the site says about the kit, in one file.

    The source list here is the same list `src/plugin-main.c` registers and
    `data/locale/en-US.ini` names. It is written out rather than parsed: the
    site describes what a source is FOR, which no header can tell it, and a
    generator that scraped the C would still need this prose beside it.
    `npm-less` on purpose — `node site/build.mjs` and nothing else.  */

export const NAME = 'Swarnil Broadcast Kit';
export const SHORT = 'SBK';
export const REPO = 'https://github.com/imswarnil/Swarnil-Broadcast-Kit';
export const OBS_MIN = '30';
export const BUILT_FOR = 'OBS Studio 32.2.2';

export const SOURCES = [
	{
		id: 'sbk_onair',
		name: 'SBK Light',
		tag: 'Indicator',
		one: 'The tally light. LIVE while you stream, REC while you record, OFF AIR otherwise.',
		body: `It reads OBS itself, so the lamp turns red the moment you go live rather than when
		you remember to click something. Five shapes, because where the light sits decides what it
		should look like: a pill or a squared badge in a corner, a bare dot when the corner is
		crowded, a bar across the head of the frame, or the edge of the whole canvas lit up.`,
		props: [
			['Shape', 'Pill, badge, dot, bar, or the canvas edge'],
			['Words', 'What it says in each state — they are yours to change'],
			['Fill with the accent while lit', 'The pill floods red instead of just the dot'],
			['Hide entirely when off air', 'Nothing on screen until you are live'],
		],
	},
	{
		id: 'sbk_logo',
		name: 'SBK Logo',
		tag: 'Brand',
		one: 'A channel bug that is alive. Your PNG or a drawn mark, looping, without a video file.',
		body: `The corner mark everybody wants and nobody wants to render a transparent video for.
		Point it at a PNG or pick one of the kit's marks and choose how it should move: a breath, a
		pulse of the glow, a slow spin, a dot going round it, a ring drawing itself on and off, a
		gentle bob. It is driven by the clock, so it is already moving the first time the scene goes
		out — a page in a Browser Source cannot promise that, because a browser will not start
		anything until something has been clicked.`,
		props: [
			['Image', 'A PNG with transparency, or nothing for a drawn mark'],
			['Mark', 'Fifteen to choose from, the recording ring among them'],
			['Loop', 'Still, breathe, pulse, spin, orbit, draw, bob'],
			['One cycle takes', 'Slower than you think is right, for a bug'],
			['Caption', 'A line under it, for a sign-off card'],
		],
	},
	{
		id: 'sbk_plate',
		name: 'SBK Plate',
		tag: 'Backdrop',
		one: 'A drop shadow for a camera or a screen capture. Put it behind, size it to the same aspect.',
		body: `OBS has no shadow. A source is composited flat, so a camera box floating over a
		backdrop has nothing under it and reads as a sticker. The plate is the missing layer: a
		rounded rectangle with a soft offset shadow, the same seven aspect ratios as the frame, and
		the shadow drawn in padding outside the shape so nothing is clipped. Behind a camera it is a
		shadow; with the fill turned on it is the card the camera sits on when the feed drops.`,
		props: [
			['Aspect', '16:9, 9:16, 1:1, 4:5, 4:3, 21:9, or a size you type'],
			['Corner radius', 'Match it to the frame in front'],
			['Offset and blur', 'How far the shadow falls and how soft it lands'],
			['Spread', 'Grow the shadow past the shape before it blurs'],
			['Fill', 'Off for a pure shadow, or the Look\u2019s glass so the source sits on a card'],
			['Glow', 'The accent bleeding out instead of a shadow falling in'],
		],
	},
	{
		id: 'sbk_social',
		name: 'SBK Social',
		tag: 'Titles',
		one: 'Where to find you: a bar of every handle, a stack of them, or one at a time on a timer.',
		body: `The third shape is the one worth having. A row of six handles is read by nobody; the
		same six shown for eight seconds each, each arriving with the kit's own motion, are read by
		everyone. Accounts are typed one per line as <code>platform: handle</code>, and a known
		platform brings its colour with it.`,
		props: [
			['Accounts', 'One per line, as platform: handle'],
			['Shape', 'One at a time, a bar of all of them, or a stack'],
			['Change every', 'Seconds each account holds the lower third'],
			['Use each platform\u2019s own colour', 'Or the Look\u2019s accent throughout'],
		],
		note: 'The marks are generic — a play triangle, a camera, an at-sign — never a company\u2019s logo. A platform is told apart by its colour and its name, which is what keeps this kit free to give away.',
	},
	{
		id: 'sbk_prompt',
		name: 'SBK Prompt',
		tag: 'Titles',
		one: 'The like-and-subscribe card, on a timer. Slides in, holds, slides out, works through your lines.',
		body: `Asking is the part everyone forgets and nobody wants to say out loud for the ninth
		time. Two decisions make it bearable rather than irritating: it is genuinely off screen
		between showings rather than parked behind your camera at zero opacity, and it can be told to
		stay quiet unless you are actually on air, so a rehearsal is not spent being asked to
		subscribe. A hotkey brings it in now, for the moment you have just said something worth
		pinning it to.`,
		props: [
			['What it asks', 'One per line, as mark: Title | Body'],
			['Comes in from', 'Any of the four edges'],
			['Every / stays for', 'How often, and how long it holds'],
			['Only when on air', 'Nothing during a rehearsal'],
			['Hotkey', 'Show the prompt now'],
		],
	},
	{
		id: 'sbk_lower_third',
		name: 'SBK Lower Third',
		tag: 'Titles',
		one: 'A name and a line under it, with the accent as a bar.',
		body: `Five variants that differ in where the weight sits: a card carries it on glass, a
		split puts the title on the accent, an underline carries none at all and leans on the rule.
		It rises into place when the source is shown, and a hotkey plays that arrival again
		mid-stream when a guest joins.`,
		props: [
			['Variant', 'Card, pill, split, minimal, underline'],
			['Accent bar', 'The red rule down the left'],
			['Hotkey', '“Play the lower third in again”, under Settings → Hotkeys'],
		],
	},
	{
		id: 'sbk_ticker',
		name: 'SBK Ticker',
		tag: 'Titles',
		one: 'A tag and a row of items sliding across the foot of the screen.',
		body: `Items are separated with a pipe and kept apart rather than joined into one string, so
		each can be drawn in its own chip and the separator between them is a drawn dot in the
		accent rather than a character. The lane is a render target, which is what clips the text
		and what lets both ends fade out instead of being cut off square.`,
		props: [
			['Variant', 'Strip on glass, bare over the video, or one chip per item'],
			['Direction and speed', 'Either way, in pixels per second'],
			['Fade at both ends', 'The text leaves softly rather than at a hard edge'],
		],
	},
	{
		id: 'sbk_frame',
		name: 'SBK Cam Frame',
		tag: 'Camera',
		one: 'The treatment drawn over your camera, with a chip on one corner.',
		body: `Pick a shape and the box is sized for you: 16:9, <strong>9:16 for Shorts and
		Reels</strong>, 1:1, 4:5, 4:3 or 21:9, scaled by one slider. Seven treatments come out of a
		single distance field, so the line keeps its weight around the corner radius instead of
		thinning on the curve the way four stacked borders do. Put your camera under it in the
		layer stack.`,
		props: [
			['Shape', '16:9, 9:16, 1:1, 4:5, 4:3, 21:9, or custom'],
			['Treatment', 'Ring, inset, corner brackets, brackets stood off, head and foot rules, glow, double'],
			['Chip', 'A handle in any corner, in the accent or on glass'],
		],
	},
	{
		id: 'sbk_visualizer',
		name: 'SBK Visualizer',
		tag: 'Audio',
		one: 'Seven ways to draw what OBS is actually playing.',
		body: `It listens to the <strong>program mix by default</strong> — literally what your
		viewers hear, every source and every filter — because a visualizer that only watches the
		microphone is a visualizer of the wrong stream. Desktop audio, any Mic/Aux channel and any
		individual source are all in the same list. When there is nothing to hear it paints a
		demo signal, so an overlay is never a dead rectangle.`,
		props: [
			['Listen to', 'Program, Desktop Audio, Mic/Aux, any source, or the demo signal'],
			['Style', 'Bars, mirrored bars, waveform, dot matrix, ring, block ladder, filled line'],
			['Audio', 'Gain, floor in dB, fall smoothing, peak hold'],
		],
	},
	{
		id: 'sbk_meter',
		name: 'SBK Meter',
		tag: 'Audio',
		one: 'A real level meter, in dB, with the zones where a broadcaster expects them.',
		body: `Not a bouncing decoration: the scale is in decibels, the peak sits where the loudest
		recent moment reached rather than falling with the bar, and the colour changes at the
		thresholds you set — comfortable below −18, loud by −6. You can see at a glance that you
		are clipping, which is the one thing a pretty bar has never told anyone.`,
		props: [
			['Listen to', 'The same list as the visualizer, program mix included'],
			['Style', 'Solid or segments, horizontal or vertical'],
			['Zones', 'Where warning and hot begin, in dB'],
		],
	},
	{
		id: 'sbk_stats',
		name: 'SBK Stats',
		tag: 'Indicator',
		one: 'How the broadcast is actually going: uptime, bitrate, dropped frames, render rate.',
		body: `The source a web overlay could never be. Uptime, dropped frames and network
		congestion live inside OBS and no page in a Browser Source can reach them. Lagged frames
		are reported as a recent change rather than the total since OBS started, because a running
		total says “something is wrong” on a machine that is perfectly fine. Put it in a scene of
		its own and open that as a windowed projector on a second monitor.`,
		props: [
			['Rows', 'Uptime, bitrate, dropped frames, render rate — each optional'],
			['Health lamp', 'Green, amber, red, from congestion and dropped frames'],
		],
	},
	{
		id: 'sbk_counter',
		name: 'SBK Counter',
		tag: 'Live data',
		one: 'A live number from an API: subscribers, members, anything your own endpoint returns.',
		body: `Three providers. <strong>YouTube</strong> wants an API key and a channel id and gives
		you subscribers, views or videos. <strong>Ghost</strong> wants your site and an Admin API
		key and gives you members, or paid members only — it signs a short-lived token for every
		request the way Ghost's own docs describe. <strong>Any JSON endpoint</strong> wants a URL
		and a dot-path into the response, which covers everything else: a Patreon proxy, a
		Cloudflare Worker you write, a webhook's cached answer.
		<br><br>
		The number counts up to a new value rather than snapping, a small lamp says whether the
		last poll worked, and a failed poll keeps the number that was there — a subscriber count
		that blinks to zero mid-stream is worse than one that is thirty seconds old.`,
		props: [
			['Provider', 'YouTube, Ghost members, or any JSON endpoint'],
			['Key', 'Typed in, or read from a file by beginning the field with @'],
			['Goal bar', 'Optional, with the target you are counting toward'],
			['Change since it started', 'The “+12 so far today” line'],
			['Check every', 'Fifteen seconds at the fastest — an overlay must not eat someone’s API quota'],
		],
		note: 'Keys typed into a source are saved in the scene collection as plain text. Begin the field with @ and a file path to keep the secret out of a collection you might share.',
	},
	{
		id: 'sbk_qr',
		name: 'SBK QR',
		tag: 'Live data',
		one: 'A scannable code for the thing you are asking people to do.',
		body: `Membership, a donation link, the channel, your site. The code is generated inside the
		plugin, so it works with no connection, nothing is logged by a third party, and the link is
		not silently rewritten by whoever owns some QR service. Rounded modules, a light-on-dark
		option, the accent colour, and a cut-out in the middle for a logo — which forces the error
		correction up to H, because punching a hole in a low-correction code destroys it.`,
		props: [
			['Link or text', 'Up to about a thousand characters; shorter scans from further away'],
			['Caption and second line', 'What it says under the code'],
			['Error correction', 'L, M, Q or H'],
			['Quiet zone', 'Never dropped — a code butted against the picture does not scan'],
			['Logo hole', 'Clears a square in the middle to drop an image on top'],
		],
	},
	{
		id: 'sbk_colour',
		name: 'SBK Colour',
		tag: 'Filter',
		one: 'A grade for any source: exposure, white balance, contrast, vibrance.',
		body: `A dark, orange webcam is the most common and most fixable problem on a stream, and
		fixing it means exposure and white balance before anything else. The order the filter applies
		things in is the order a colourist would use, and it matters: exposure before contrast
		(contrast pivots on middle grey, so exposing afterwards throws the pivot off), white balance
		before saturation (or saturation exaggerates a cast you are about to correct). Seven presets,
		and they are corrections rather than looks — “warm room” takes the orange <em>out</em>.`,
		props: [
			['Exposure and contrast', 'In stops, pivoting on middle grey'],
			['Temperature and tint', 'Blue to amber, green to magenta'],
			['Saturation and vibrance', 'Vibrance spares what is already vivid, so a red shirt does not shout'],
			['Lift, gamma and gain', 'Per channel, for when a preset is nearly right'],
		],
	},
	{
		id: 'sbk_punch',
		name: 'SBK Punch',
		tag: 'Filter',
		one: 'A zoom into the picture, on a hotkey.',
		body: `In a tutorial you constantly want to push in on the thing you are pointing at and come
		back out, and doing that by hand means grabbing a scene item mid-sentence. One key instead: it
		eases in, holds, and eases back. The visible window is kept inside the frame, so a punch near
		an edge slides along rather than smearing the edge pixel across a third of the picture, and
		pressing again mid-move carries the current zoom across instead of snapping.`,
		props: [
			['Zoom and focus', 'How far in, and which point of the picture stays still'],
			['In and out', 'Seconds each way, eased'],
			['Come back out on its own', 'After a hold you set'],
			['Hotkeys', 'Punch in, punch out, or one key that does both'],
		],
	},
	{
		id: 'sbk_voice',
		name: 'SBK Voice',
		tag: 'Audio filter',
		one: 'The chain a spoken voice wants, in one filter with one set of presets.',
		body: `High-pass, gate, compressor, presence lift, saturation and limiter, in that order.
		Nothing here is exotic — OBS ships every one of these separately. The value is that the order
		is right, the defaults are sane, and a preset moves all of it at once, because most people
		never chain six filters and so never sound better. Stream, Podcast, Noisy room and Quiet mic
		cover almost everyone.`,
		props: [
			['High-pass', 'Desk thumps and air conditioning live under 80 Hz and a voice has nothing to lose down there'],
			['Gate', 'Opens and closes at levels you set, with a hold so it does not chatter'],
			['Compressor', 'Threshold, ratio, soft knee, attack, release, make-up'],
			['Presence, saturation, limiter', 'The lift that cuts through, a little warmth, and a ceiling'],
		],
		note: 'Everything is per channel. A compressor whose detector is the sum of two channels pumps audibly on anything panned.',
	},
	{
		id: 'sbk_radio',
		name: 'SBK Radio',
		tag: 'Audio filter',
		one: 'Telephone, AM radio, megaphone, tannoy, walkie-talkie.',
		body: `Band-limit the voice, squash it, and add the distortion the medium would have added.
		It exists because the alternative is stacking three of OBS's filters and guessing at the
		frequencies. The <em>Amount</em> control blends against the untouched voice, which is usually
		more convincing than all of it.`,
		props: [
			['Band', 'What it cuts below and above'],
			['Squash and distortion', 'One slider each, mapped onto a threshold and ratio behind the scenes'],
			['Hiss', 'Because a clean radio does not sound like a radio'],
		],
	},
	{
		id: 'sbk_round',
		name: 'SBK Round Corners',
		tag: 'Filter',
		one: 'Rounds the corners of your camera — the picture itself, not a frame over it.',
		body: `A frame drawn on top is a rectangle with a hole in it: the camera's square corners are
		still there underneath, so the “rounded” webcam only looks rounded against a background that
		happens to match. This is a filter, so the corners are actually gone and anything can sit
		behind them. Add it to the camera under <em>Filters</em>, and put <strong>SBK Cam Frame</strong>
		over the top if you also want a chip or corner brackets.`,
		props: [
			['Radius', 'As a percentage of the shorter side, so one setting suits a small webcam box and a full-frame share — or in pixels'],
			['Border', 'Drawn just inside the cut edge, in any colour'],
			['Pull the shape in', 'Room for the border to sit on without eating the picture'],
		],
		note: 'A filter, not a source. It appears under Filters on a source or a scene, not in the + menu.',
	},
	{
		id: 'sbk_scanlines',
		name: 'SBK Scanlines',
		tag: 'Filter',
		one: 'A CRT treatment for any source: scanlines, aperture mask, fringing, curvature, grain.',
		body: `Every part dials to zero on its own, because the whole difference between a tasteful
		hint of a monitor and something unwatchable is the amounts. Four presets cover what people
		actually reach for — <em>Fine</em>, <em>CRT</em>, <em>VHS</em>, <em>Arcade</em> — and each one
		just fills in the sliders, so you can start from one and move on. The scanlines are a raised
		cosine rather than a hard stripe: at one or two pixels a square wave turns into moiré the
		moment anything moves.`,
		props: [
			['Scanlines', 'Spacing, depth, roll speed, and the brightness to win back'],
			['Tube', 'Aperture mask and triad width, colour fringing, barrel curvature'],
			['Wear', 'Vignette, grain, mains flicker'],
		],
		note: 'Put it on one source, or on a whole scene to treat everything at once.',
	},
	{
		id: 'sbk_card',
		name: 'SBK Card',
		tag: 'Scenes',
		one: 'The announcement: an eyebrow with the recording light, a title, a line, chips.',
		body: `Starting soon, Be right back and Thanks for watching are all this card with
		different words. The body wraps to the width you set, and five variants take it from a
		glass panel to bare type straight on the video.`,
		props: [
			['Variant', 'Panel, split with an accent stripe, outline, the whole card in the accent, or plain'],
			['Align', 'Left or centred'],
			['Chips', 'A row, separated with a pipe, wrapping to the width'],
		],
	},
	{
		id: 'sbk_backdrop',
		name: 'SBK Backdrop',
		tag: 'Scenes',
		one: 'The ground under it all — twelve of them, and the patterns drift.',
		body: `A solid, a scrim that fades so a camera can sit behind the card, a vignette, a
		two-colour gradient, or a pattern: grid, dot grid, diagonal stripes, waves, concentric
		rings, a hex lattice, or grain. Anything with a pattern can drift slowly, which is what
		keeps a waiting screen from looking like a frozen stream.`,
		props: [
			['Kind', 'Solid, scrim from the foot or head, vignette, gradient, grid, dots, stripes, waves, rings, hex, grain'],
			['Drift', 'Pixels per second, either direction'],
			['Spacing and weight', 'How big the pattern is and how heavy its line'],
		],
	},
	{
		id: 'sbk_chip',
		name: 'SBK Chip',
		tag: 'Indicator',
		one: 'One small badge: a handle, a hashtag, “Q&A”, a follower count.',
		body: `The piece every overlay set needs a dozen of and nobody wants to build a dozen times.
		A label, optionally a second segment holding a value in the accent, and optionally a dot in
		front that can be wired to the tally state so the chip itself says you are live.`,
		props: [
			['Label and value', 'The value rides in its own accent capsule'],
			['Leading dot', 'None, accent, breathing, or lit only when on air'],
			['Variant', 'Card, pill, outline, accent, or plain'],
		],
	},
	{
		id: 'sbk_progress',
		name: 'SBK Progress',
		tag: 'Indicator',
		one: 'A goal: subscribers, a fundraiser, chapter 3 of 8.',
		body: `The number is the point, so it is set in the mono face and the bar eases toward a new
		value rather than jumping — a counter that snaps reads as a glitch. Two hotkeys nudge the
		value up and down without opening the dialog, which is what you actually need while live.`,
		props: [
			['Value and target', 'Shown as value / target or as a percentage'],
			['Bar', 'Solid, segments, or a thin line'],
			['Hotkeys', '“Nudge the goal up / down”, under Settings → Hotkeys'],
		],
	},
	{
		id: 'sbk_clock',
		name: 'SBK Clock',
		tag: 'Scenes',
		one: 'The time, set in the mono face, 12- or 24-hour.',
		body: `Small, and the thing a waiting audience checks. Seconds optional.`,
		props: [['Format', '12- or 24-hour, with or without seconds']],
	},
	{
		id: 'sbk_countdown',
		name: 'SBK Timer',
		tag: 'Scenes',
		one: 'Counts down or up, in four styles, including a progress ring.',
		body: `Down to a duration, down to a time of day, up from zero for a segment, or up since the
		broadcast actually started — the same digits with a different source of truth. Counting to a
		time of day rolls over to tomorrow if it has already passed, so “21:30” keeps working after
		21:30. A countdown restarts each time its scene is shown, so the starting-soon screen is right
		every time you switch to it rather than only the first. The last ten seconds turn red, which
		is the only warning a waiting screen gets to give.`,
		props: [
			['Counts', 'Down for a duration, down to a time of day, up from zero, or up since the stream started'],
			['Style', 'Digits, digits in a ring, ring only, or digits over a bar'],
			['At zero', 'The word it shows when it arrives'],
			['Hotkeys', 'Restart, and pause or resume'],
		],
	},
	{
		id: 'sbk_comments',
		name: 'SBK Comments',
		tag: 'Chat',
		one: 'Questions on screen — typed by you, pulled from a YouTube live chat, or read from any JSON.',
		body: `Built for teaching, where the chat gets ahead of you and you want the question up
		while you answer it. It holds forty and shows a few at a time, rotating pages on a timer so a
		long queue still gets its turn. Manual mode takes one line per question as “Name: the
		question”, which is all a rehearsal needs. YouTube mode finds the live chat from the video id
		and then polls it, honouring the interval the API asks for rather than hammering it. JSON mode
		points at anything you already run and tells it where the name and the text live.`,
		props: [
			['From', 'Typed here, a YouTube live chat, or any JSON endpoint'],
			['Show at once', 'How many fit before it pages'],
			['Rotate every', 'Seconds a page stays up'],
			['Newest first', 'Or keep the order they arrived in'],
			['Initials', 'A lettered dot beside each name'],
		],
		note: 'A YouTube key typed here is saved in the scene collection in plain text. Type @/path/to/a/file to keep it out.',
	},
	{
		id: 'sbk_wipe',
		name: 'SBK Wipe',
		tag: 'Transition',
		one: 'A real transition, in the Scene Transitions panel beside Fade and Cut.',
		body: `The other thing no browser source can be: a transition is composited by OBS between
		two scene textures, and nothing running inside a page can see both. Five styles — a bar with
		the accent riding its leading edge, a dip through the accent, a slide, an iris, and blinds —
		so a cut carries the same red as the tally light.`,
		props: [
			['Style', 'Bar, dip, slide, iris, blinds'],
			['Direction', 'Any of the four'],
			['Bar width and softness', 'How much accent rides the edge, and how hard it is'],
		],
		note: 'Added from the Scene Transitions panel’s + button, not from Sources.',
	},
	{
		id: 'sbk_sting',
		name: 'SBK Logo Sting',
		tag: 'Transition',
		one: 'A cut hidden behind your logo. The field crosses, the mark lands, the field leaves.',
		body: `The stinger every channel has, without a video file to render. A colour field sweeps
		in as a band, an iris or a curtain, holds long enough for a logo to land on it, and sweeps
		out on the next scene. The cut happens under the field where nobody sees it. Point it at a
		PNG and that is the mark; leave it empty and it draws the tally ring instead. The audio ducks
		through the middle rather than crossfading, which is what makes it feel like a sting and not
		a dissolve.`,
		props: [
			['Style', 'Band, iris, or curtain'],
			['Hold', 'How much of the transition the field stays up for'],
			['Logo', 'A PNG with transparency, or nothing for the drawn mark'],
			['Field colour', 'The accent by default'],
		],
		note: 'Added from the Scene Transitions panel’s + button, not from Sources.',
	},
];

export const SCENES = [
	{ img: 'starting-soon', name: 'Starting soon', body: 'The countdown screen people sit on. A card, a countdown, a drifting grid, the clock, and a spectrum along the foot.', uses: ['Backdrop (drifting grid)', 'Visualizer (bars)', 'Social (stack)', 'Light', 'Clock', 'Card', 'Countdown'] },
	{ img: 'welcome', name: 'Welcome', body: 'The first seconds, with nothing to read. A gradient, a line of type, and a chip that pops in when you go live.', uses: ['Backdrop (gradient)', 'Visualizer (line)', 'Card', 'Chip', 'Light (bar)'] },
	{ img: 'lesson', name: 'Lesson', body: 'The title card a course module opens on: which module, what it covers, and how far through the set you are.', uses: ['Backdrop (gradient)', 'Card', 'Progress (segments)', 'Light (dot)'] },
	{ img: 'screen-share', name: 'Screen share', body: 'The code has the frame. A small camera box on a plate, a chapter chip, a segment progress bar, and a mic meter so silence never goes unnoticed.', uses: ['Camera', 'Plate', 'Cam Frame', 'Chip', 'Progress (segments)', 'Meter', 'Prompt', 'Light (badge)'] },
	{ img: 'pair-share', name: 'Screen share + two', body: 'Two people over one shared screen. Both cameras stack on the same edge so the screen keeps the middle, and a meter under each says who is talking.', uses: ['Camera', 'Plate ×2', 'Cam Frame ×2', 'Chip', 'Meter ×2', 'Ticker'] },
	{ img: 'two-up', name: 'Two up', body: 'The screen and you, in two different shapes: a wide 21:9 crop so code has room for long lines, and a 9:16 column beside it, which is the shape a face actually fills.', uses: ['Display capture', 'Camera', 'Plate \u00d72', 'Cam Frame (21:9 + 9:16)', 'Logo (orbiting)', 'Chip', 'Social (bar)', 'Light (badge)'] },
	{ img: 'three-up', name: 'Three up', body: 'A 16:9 screen across the top, a square host and a 4:5 guest under it, each with a meter so a silent guest is visible at a glance.', uses: ['Display capture', 'Camera', 'Plate \u00d73', 'Cam Frame (16:9 + 1:1 + 4:5)', 'Meter \u00d72', 'Lower Third', 'Light (dot)'] },
	{ img: 'comments', name: 'Comments', body: 'Questions stacked down the right while the screen keeps the left. Type them, or let it read your YouTube live chat. The one to cut to when the chat has got ahead of you.', uses: ['Comments', 'Chip', 'Plate', 'Camera', 'Cam Frame', 'Light (badge)'] },
	{ img: 'talking-head', name: 'Talking head', body: 'The camera is the whole picture, so the chrome shrinks to corner brackets, a minimal name and a bare dot.', uses: ['Camera', 'Cam Frame (brackets)', 'Logo (orbiting)', 'Lower Third (minimal)', 'Chip', 'Light (dot)'] },
	{ img: 'interview', name: 'Interview', body: 'Two cameras on plates, two names on split lower thirds, and nothing else competing.', uses: ['Camera', 'Plate ×2', 'Cam Frame ×2', 'Lower Third (split) ×2', 'Ticker', 'Light'] },
	{ img: 'live', name: 'Live', body: 'The everyday scene: your camera on a plate under the frame, a lower third, the ticker, the light in the corner.', uses: ['Camera', 'Plate', 'Cam Frame', 'Ticker', 'Lower Third', 'Prompt', 'Light'] },
	{ img: 'gameplay', name: 'Gameplay', body: 'The capture has the frame, so the camera shrinks into a corner behind stood-off brackets and everything else hugs the edges.', uses: ['Camera', 'Plate', 'Cam Frame (brackets)', 'Chip', 'Progress', 'Ticker'] },
	{ img: 'music', name: 'Music', body: 'The visualizer as the whole scene, on the program mix, so it moves to whatever is actually playing.', uses: ['Backdrop (checkers)', 'Visualizer (ring)', 'Chip', 'Clock', 'Light'] },
	{ img: 'highlight', name: 'Highlight', body: 'One sentence, full bleed, over a drifting starfield. For reading a question out, or landing a point you want people to screenshot.', uses: ['Backdrop (starfield)', 'Card', 'Chip', 'Light (dot)'] },
	{ img: 'intermission', name: 'Intermission', body: 'A four-minute ring growing into place on drifting aurora, and nothing to read. The one to cut to when you need a moment and would rather not explain.', uses: ['Backdrop (aurora)', 'Timer (ring)', 'Chip', 'Light'] },
	{ img: 'brb', name: 'Be right back', body: 'A drifting dot grid, a waveform, and a five-minute countdown that restarts every time you switch to it.', uses: ['Backdrop (dots)', 'Visualizer (wave)', 'Light', 'Clock', 'Card', 'Countdown'] },
	{ img: 'podcast', name: 'Podcast', body: 'Two people, no camera, the meters doing the showing — it should be obvious at a glance which microphone is live. A running-time bar underneath.', uses: ['Backdrop (plasma)', 'Card', 'Meter ×2', 'Timer (bar)', 'Light'] },
	{ img: 'trouble', name: 'Technical difficulties', body: 'The one scene that should look wrong on purpose. A vignette, the card in full accent, and the canvas edge lit red.', uses: ['Backdrop (vignette)', 'Card (accent)', 'Light (canvas edge)'] },
	{ img: 'support', name: 'Support', body: 'The ask, with something to scan: a membership QR, a live member count with a goal, and a live subscriber count beside it.', uses: ['Backdrop (rings)', 'Card', 'QR', 'Counter (Ghost)', 'Counter (YouTube)', 'Ticker', 'Light'] },
	{ img: 'ending', name: 'Ending', body: 'The ask, with the goal it is asking for: a subscriber bar, a subscribe chip, a channel QR, and drifting stripes.', uses: ['Backdrop (stripes)', 'Visualizer (dots)', 'Card', 'Progress', 'Chip', 'Social (bar)', 'Logo (drawing)', 'QR', 'Light'] },
	{ img: 'vertical', name: 'Vertical', body: 'A 9:16 box inside the landscape canvas, on its own plate. Frame yourself inside it and the same take cuts to a Short.', uses: ['Backdrop (hex)', 'Plate', 'Camera', 'Cam Frame (9:16)', 'Card', 'Light (dot)'] },
	{ img: 'desk', name: 'Desk (private)', body: 'Not for the stream. Open it as a windowed projector on a second monitor: stats, both meters, the questions queue, and time on air.', uses: ['Stats', 'Meter (program)', 'Meter (mic)', 'Comments', 'Timer (uptime)', 'Light'] },
];

export const STEPS = [
	{
		n: 1,
		title: 'Put the plugin in place',
		body: `Quit OBS. Download the latest <code>sbk.plugin</code> from the releases page and drop
		it into your user plugin folder, then start OBS again.`,
		code: '~/Library/Application Support/obs-studio/plugins/sbk.plugin',
	},
	{
		n: 2,
		title: 'Let the fonts in',
		body: `The kit sets type in Geist and Geist Mono. Copy the files from <code>fonts/</code>
		into your Fonts folder, or pick any font you already have in a source’s
		<em>Look → Font</em>. Building from source does this for you.`,
		code: 'open ~/Library/Fonts',
	},
	{
		n: 3,
		title: 'Build the show',
		body: `<strong>Tools → Broadcast Kit: create the scene collection</strong> writes twelve
		complete scenes and switches to them. Every source in them is an ordinary source: select
		it, open Properties, change anything. Nothing is locked.`,
	},
	{
		n: 4,
		title: 'Put your camera under the frames',
		body: `The kit draws treatments, not captures — creating a camera on your behalf would
		switch your webcam on just because you opened a menu. Add your own <em>Video Capture
		Device</em> to a scene and drag it below the <em>SBK Cam Frame</em> in the Sources list, so
		the frame draws over it.`,
	},
	{
		n: 5,
		title: 'Add the transition',
		body: `In the <strong>Scene Transitions</strong> panel press <strong>+</strong> and choose
		<strong>SBK Wipe</strong>. Set the duration beside it — 300–500 ms suits the bar. OBS only
		lets the panel add transitions, which is why this one step is by hand.`,
	},
	{
		n: 6,
		title: 'Bind the hotkeys you will use live',
		body: `Under <strong>Settings → Hotkeys</strong>: play the lower third in again, restart the
		countdown, nudge the goal up and down. These are the four you reach for mid-stream.`,
	},
];

export const APIS = [
	{
		name: 'YouTube — subscribers, views, videos',
		steps: [
			'Open <a href="https://console.cloud.google.com/">console.cloud.google.com</a> and make a project (any name).',
			'APIs &amp; Services → Library → search <strong>YouTube Data API v3</strong> → Enable.',
			'APIs &amp; Services → Credentials → Create credentials → <strong>API key</strong>. Restrict it to that one API.',
			'Find your channel id: YouTube Studio → Settings → Channel → Advanced. It begins with <code>UC</code>.',
			'In OBS: add <strong>SBK Counter</strong>, provider <em>YouTube</em>, paste both.',
		],
		note: 'YouTube rounds public subscriber counts, so 1,234 shows as 1,230. That is the API, not the kit.',
	},
	{
		name: 'Ghost — members, or paid members',
		steps: [
			'Ghost Admin → Settings → <strong>Integrations</strong> → Add custom integration, call it “OBS”.',
			'Copy the <strong>Admin API key</strong>. It looks like <code>640…:e3f…</code> — an id, a colon, a hex secret.',
			'In OBS: add <strong>SBK Counter</strong>, provider <em>Ghost</em>, paste your site URL and the key.',
		],
		note: 'Member counts are admin-only in Ghost, so the Content API key will not work. The kit signs a fresh five-minute token for every request rather than storing one.',
	},
	{
		name: 'Anything else — one JSON endpoint',
		steps: [
			'Point the provider at <em>Any JSON endpoint</em>.',
			'Give it the URL and a dot-path to the number: <code>count</code>, <code>data.total</code>, <code>items.0.stats.followers</code>.',
			'Add a bearer token if the endpoint needs one.',
		],
		note: 'This is how you reach a service with no direct support — put a tiny Cloudflare Worker in front of it that returns the one number, and point the counter at the Worker. Your key stays on the server.',
	},
];

export const FAQ = [
	{
		q: 'Can I use this on a channel I make money from, or inside something I sell?',
		a: `Yes, and you owe nobody a credit. The source is MIT: fork it, rename it, ship it inside a
		paid product. The one thing to know is that a *compiled* plugin links libobs, which is GPL-2.0,
		so a binary you hand out carries the GPL's terms with it. The source in the repository is MIT
		and stays MIT. The marks the kit draws are generic rather than trademarked logos, and it shares
		no code with any commercial design system — both of those are deliberate, and they are what
		make the first sentence of this answer true.`,
	},
	{
		q: 'Can an AI agent build scenes with this?',
		a: `Yes. The repository ships an agent skill — copy \`skills/sbk-scenes\` into
		\`~/.claude/skills/\` and ask for the scene you want. Its reference is generated from the
		plugin's own C, so it cannot claim a setting that does not exist, and it validates a spec
		before writing anything. There is a generator you can run yourself too, without an agent.`,
	},
	{
		q: 'Can I design a scene without installing anything?',
		a: `Yes — the <a href="/builder/">builder</a> runs in the browser. Place the pieces, set the
		words, and export a scene collection. What comes out is the real thing: the same source ids
		and setting keys the plugin registers, so <strong>Scene Collection → Import</strong> in OBS
		builds native sources, not a picture of them. The preview is a schematic — it shows where
		things sit and how they read, not the plugin's own drawing, which is done with shaders and
		real hinted type.`,
	},
	{
		q: 'Can I control it from my phone?',
		a: `Yes — the kit ships a remote that drives OBS over the WebSocket server OBS already has.
		Scenes, stream and record, the mic, the transition, and a button for every hotkey the kit
		registers, discovered from OBS rather than hard-coded. It runs from your own machine on your
		own network: <code>remote/serve.command</code> prints the address to open on your phone.
		Nothing goes through this site, and the password never leaves your browser.`,
	},
	{
		q: 'Why a plugin and not a browser source?',
		a: `Because the things worth having cannot be done in a page. A browser source cannot read
		whether you are live, cannot see dropped frames or congestion, cannot hear the program mix,
		and can never be a transition. It also costs a whole Chromium process per overlay. Every
		source here is drawn by OBS itself.`,
	},
	{
		q: 'Does it work on Windows or Linux?',
		a: `The code is portable — text is OBS’s own FreeType source and the shaders are plain
		HLSL-flavoured effects — and <code>CMakeLists.txt</code> has a branch that builds against an
		installed OBS development package. Only macOS has been tested, and the release ships an
		**Apple Silicon** bundle only — on an Intel Mac, build from source, which produces a plugin for
		whatever machine it runs on. Reports welcome.`,
	},
	{
		q: 'Will it slow my stream down?',
		a: `Each source is one or two draw calls and a small amount of text that is re-rasterised
		only when it changes. The audio analysis is a 2048-point transform once per frame, shared
		between every visualizer listening to the same thing.`,
	},
	{
		q: 'Can I change the colours?',
		a: `Every source has the same <em>Look</em> group: one accent colour, a scale slider that
		grows type, padding and radius together, a tone — glass, solid or light — and the font. Give
		every source in a scene the same accent and the whole scene changes together.`,
	},
	{
		q: 'Where do my API keys end up?',
		a: `In the scene collection, as plain text, because that is where OBS saves every source
		setting. If you are ever going to share a collection, begin the key field with <code>@</code>
		and the path to a file — <code>@/Users/you/.youtube-key</code> — and the kit reads it from
		there instead. Nothing is ever sent anywhere but the provider you chose.`,
	},
	{
		q: 'What licence is it under?',
		a: `The kit’s own code is MIT. A compiled plugin links libobs, which is GPL-2.0, so the
		binary is distributed under the GPL’s terms. Geist and Geist Mono are © Vercel under the SIL
		Open Font License 1.1.`,
	},
];
