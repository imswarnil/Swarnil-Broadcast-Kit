/*  The ready-made shows.

    Each one is a real spec in `presets/`, built at site-build time by the same
    generator the agent skill uses, and offered as a file OBS can import. That
    is deliberate: a page that describes a layout is a page somebody has to
    reproduce by hand, and reproducing a layout by hand is exactly the work this
    kit exists to remove. The download is the documentation.

    The prose here is the only thing authored twice — the scene list, the source
    list and the counts are all read back out of the built collection, so a page
    cannot claim a scene the file does not contain.  */

export const PRESETS = [
	{
		id: 'course',
		name: 'Course',
		who: 'Teaching something in modules',
		one: 'A title card per module, the screen with your camera on it, a questions panel, a timed exercise, and a wrap-up.',
		body: `The scene most courses are missing is the one where you stop talking. <strong>Exercise</strong>
		puts a ring timer on screen and says what to go and do, so ten minutes of silence reads as
		deliberate rather than as a stream that has died. <strong>Questions</strong> gives the chat the
		right-hand column and pages through it on a timer, which is what you want when the chat has got
		ahead of you.`,
		pieces: ['Card', 'Progress', 'Comments', 'Countdown', 'Plate', 'Cam Frame', 'Meter', 'Social', 'Logo'],
	},
	{
		id: 'podcast',
		name: 'Podcast',
		who: 'Two people, one conversation',
		one: 'A cold open, a two-up with both faces on plates, a levels view, a break, and an outro.',
		body: `<strong>Levels</strong> is the one worth having and the one nobody builds. Two big meters
		and a running clock, no faces — cut to it when somebody is telling a long story and you want the
		picture to stop competing with it. It is also the fastest way to see that a guest's microphone
		has died, which is the failure that ruins an episode in post.`,
		pieces: ['Plate ×2', 'Cam Frame ×2', 'Lower Third ×2', 'Meter ×2', 'Timer', 'Ticker', 'Social', 'QR'],
	},
	{
		id: 'gaming',
		name: 'Gaming',
		who: 'A capture with you in the corner',
		one: 'A waiting screen, the game with a corner camera, a clean full-screen cut, a break, and an ending.',
		body: `<strong>Full screen</strong> exists so you have somewhere to go when the game needs the
		whole frame. It keeps only the bug and the tally light, so the cut reads as intentional rather
		than as the overlay falling over. The playing scene carries the prompt, which asks people to
		follow every five minutes so you never have to say it again out loud.`,
		pieces: ['Cam Frame (brackets)', 'Plate', 'Chip', 'Progress', 'Prompt', 'Ticker', 'Logo', 'Countdown'],
	},
	{
		id: 'demo',
		name: 'Product demo',
		who: 'Showing software to people who might buy it',
		one: 'A title, a 21:9 screen beside a 9:16 you, a close-up cut, and the ask with a QR.',
		body: `The screen gets a <strong>21:9 crop</strong> and you get a <strong>9:16 column</strong>.
		Matching the two shapes is the instinct and it is wrong: a 16:9 camera next to a 16:9 screen
		spends half its pixels on your desk, and code needs width far more than a face does. The ask at
		the end carries a QR, because a URL read aloud is a URL nobody types.`,
		pieces: ['Plate ×2', 'Cam Frame (21:9 + 9:16)', 'Chip', 'QR', 'Social', 'Logo', 'Card'],
	},
	{
		id: 'shorts',
		name: 'Vertical',
		who: 'Shorts and Reels, shot on the same canvas',
		one: 'A hook, a point, and a call to action, all inside a 9:16 box on the landscape canvas.',
		body: `A 9:16 box inside a 1920 × 1080 canvas, so the same take cuts to a Short without a second
		profile or a second OBS. Frame yourself inside the box and crop afterwards. The call to action
		uses the prompt on a twenty-second cycle, which is the right rhythm for something sixty seconds
		long.`,
		pieces: ['Plate (9:16)', 'Cam Frame (9:16)', 'Card', 'Chip', 'Prompt', 'Backdrop'],
	},
];

/*  What the pieces make when you put them together.

    The source list answers "what is there". This answers the question people
    actually arrive with, which is "what do I put where". */
export const COMBOS = [
	{
		make: 'A camera that sits on the scene',
		from: ['Plate', 'Camera', 'Cam Frame'],
		how: `Plate behind, camera on top, frame over it, all three on the same centre. OBS composites
		flat, so without the plate a camera box has nothing under it and reads as a sticker. Give the
		plate the Look&rsquo;s glass and it is also what you see when the feed drops.`,
	},
	{
		make: 'A waiting screen nobody minds staring at',
		from: ['Backdrop', 'Card', 'Countdown', 'Visualizer', 'Clock'],
		how: `A drifting ground so the picture is not frozen, something to read, a ring that says how
		long, and a clock to prove the stream is alive. The countdown restarts every time you cut to the
		scene, so it is right on the fourth visit as well as the first.`,
	},
	{
		make: 'Chat on screen without a browser source',
		from: ['Comments', 'Chip'],
		how: `Type the questions, point it at a YouTube live chat by video id, or read any JSON. It
		holds forty and pages through them on a timer, so a queue that has got ahead of you still gets
		its turn.`,
	},
	{
		make: 'Asking for the follow without saying it again',
		from: ['Prompt'],
		how: `A card slides in from an edge, holds, and leaves, working through your lines on a timer.
		Genuinely off screen in between, and it can stay quiet unless you are actually on air, so a
		rehearsal is not spent being asked to subscribe.`,
	},
	{
		make: 'Where to find you, in a form people read',
		from: ['Social'],
		how: `A bar of every handle, a stack, or one at a time on a timer. The third is the one worth
		having: a row of six handles is read by nobody, and the same six shown for eight seconds each
		are read by everyone.`,
	},
	{
		make: 'A channel mark that is alive',
		from: ['Logo'],
		how: `Your PNG or a drawn mark, looping — a breath, a slow spin, a dot going round, a ring
		drawing itself on. It runs off the clock, so it is already moving the first time the scene goes
		out. A page in a browser source cannot promise that, because a browser will not start anything
		until something has been clicked.`,
	},
	{
		make: 'Knowing your microphone is on before the chat tells you',
		from: ['Meter', 'Stats'],
		how: `A real dB meter on the mic, the program mix, or any source, and a stats panel with a
		health lamp fed by congestion and dropped frames. Put both on a scene you open as a projector on
		a second monitor and never put them on air.`,
	},
	{
		make: 'A cut that carries your brand',
		from: ['Logo Sting', 'Wipe'],
		how: `Two real transitions, composited by OBS between two scene textures — something nothing
		running inside a page can do, because a page cannot see both scenes. The sting hides the cut
		behind your mark with no video file to render.`,
	},
];
