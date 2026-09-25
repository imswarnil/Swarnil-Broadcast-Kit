# The sources

Every source shares two groups. **Look** is the accent colour, a scale slider
that grows type, padding and radius together, a tone (glass, solid or light) and
the font. **Motion** is how it arrives — fade, rise, drop, or slide from either
side — and whether that replays each time the source is shown.

Sizes below are at scale 1 on a 1080p canvas.

| Source | What it does |
| --- | --- |
| **SBK Light** | The tally light: LIVE, REC, LIVE · REC or OFF AIR, from OBS's own state. Shapes: pill, badge, bare dot, a bar across the frame, or the whole canvas edge lit. Can fill with the accent while lit, and hide entirely when off. |
| **SBK Lower Third** | A name and a line under it. Card, pill, split (the title on an accent slab), minimal (shadowed type, no card) or underline. Hotkey: *play the lower third in again*. |
| **SBK Ticker** | A tag and items separated with `\|`. On glass, bare, or one chip per item. Either direction, any speed, fading at both ends. |
| **SBK Cam Frame** | The treatment over a camera. Shapes 16:9, **9:16**, 1:1, 4:5, 4:3, 21:9 or custom, scaled by one slider. Treatments: ring, inset, corner brackets, brackets stood off, head and foot rules, glow, double. A chip on any corner. |
| **SBK Visualizer** | Bars, mirrored bars, waveform, dot matrix, ring, block ladder or filled line. Listens to the program mix by default. |
| **SBK Meter** | A level meter in dB: solid or segments, horizontal or vertical, peak hold, and colour zones you set (calm below −18, hot by −6). |
| **SBK Stats** | Uptime, bitrate, dropped frames, render rate, and a health lamp from congestion and drops. |
| **SBK Counter** | A live number from YouTube, Ghost members, or any JSON endpoint. Optional goal bar, the change since it started, and a lamp for the poll. |
| **SBK QR** | A scannable code, generated in the plugin. Rounded modules, light-on-dark, the accent, a logo hole (which forces error correction to H). |
| **SBK Card** | Eyebrow with the recording light, title, body, chips. Panel, split, outline, accent or plain; left or centred. |
| **SBK Backdrop** | Solid, scrim from the foot or head, vignette, gradient, grid, dot grid, diagonal stripes, waves, concentric rings, hex lattice, grain. Patterns drift. |
| **SBK Chip** | A label, optionally a value in an accent capsule, optionally a dot that lights only when you are on air. |
| **SBK Progress** | A goal: solid, segments or a thin line, as value / target or a percentage. Hotkeys: *nudge the goal up / down*. |
| **SBK Clock** | The time in the mono face, 12- or 24-hour, seconds optional. |
| **SBK Countdown** | To a duration or a time of day, with a word at zero. Restarts when shown. Hotkey: *restart the countdown*. |
| **SBK Wipe** | A transition: bar, dip through the accent, slide, push, iris, blinds, or a band of accent that crosses the frame and takes the cut with it — a stinger with no video file. Any of four directions. Added from the Scene Transitions panel's **+**. |

## Filters

These appear under **Filters** on a source or a whole scene, not in the **+** menu.

| Filter | What it does |
| --- | --- |
| **SBK Round Corners** | Rounds the corners of the picture itself. A frame drawn on top is a rectangle with a hole in it — the camera's square corners are still there underneath, so it only looks rounded against a matching background. This cuts the picture, so anything can sit behind it. Radius as a percentage of the shorter side (one setting suits a webcam box and a full-frame share) or in pixels, plus an inner border. |
| **SBK Scanlines** | A CRT treatment: scanline spacing, depth and roll; aperture mask and triad width; colour fringing; barrel curvature; vignette, grain and mains flicker. Four presets — Fine, CRT, VHS, Arcade — each of which just fills in the sliders so you can start from one and move on. |

## Audio

The visualizer and the meter share one engine and one picker:

| Choice | What it listens to |
| --- | --- |
| **Program** | The master mix — every source and filter, what a viewer hears. The default. |
| **Desktop Audio 1–2**, **Mic/Aux 1–3** | OBS's own output channels, named as they are in Settings. |
| Any source | A source in the collection that carries audio, after its own filters. |
| **Demo signal** | Synthetic, for setting the look up in silence. |

When the chosen source is not there — a collection still loading, a device
unplugged — the demo signal plays. An overlay is never a dead rectangle.

## Live data

`SBK Counter` polls on a worker thread. The picture never waits on the network,
nothing is requested faster than every fifteen seconds, and a failed request
keeps the last good number rather than blinking to zero.

| Provider | What it needs |
| --- | --- |
| **YouTube** | An API key (Google Cloud → YouTube Data API v3) and a channel id beginning `UC`. Subscribers, views or videos. |
| **Ghost** | Your site URL and an Admin API key from Settings → Integrations. All members or paid members. A fresh five-minute token is signed for every request. |
| **Any JSON endpoint** | A URL and a dot-path: `count`, `data.total`, `items.0.stats.followers`. An optional bearer token. |

Keys typed into a source are saved in the scene collection as plain text.
Begin a key field with `@` and a path — `@/Users/you/.youtube-key` — and the kit
reads it from the file, so a collection you share carries no secret.

## The camera and the microphone

Building the collection adds both: a **Camera** source with **SBK Round Corners**
already on it, placed under the frame in every scene that has one and cropped to
fit rather than squashed; and a microphone on OBS's own Mic/Aux channel, which is
what `@mic` means to the meter and the visualizer. An input you have already
chosen is left alone. Both are ordinary sources — swap the device, disable them,
or delete them.

**Tools → Broadcast Kit: add my camera and microphone** does the same for a scene
you built yourself.

## The phone remote

`remote/serve.command` prints an address to open on a phone on the same network.
The remote drives OBS through the WebSocket server OBS already ships: scenes,
stream and record, the mic, the transition, and a button for every hotkey the kit
registers, discovered from OBS rather than hard-coded.

It must be served over plain `http` from your own machine. A page loaded over
`https` cannot open the unencrypted `ws://` connection obs-websocket speaks —
browsers block it — so a hosted copy could never reach your OBS. Nothing goes
through the website, and the password stays in that phone's browser.

## The Tools menu

- **Broadcast Kit: build the show here** — the thirteen scenes into the current
  collection. A scene of the same name is emptied and refilled, so the order you
  arranged survives.
- **Broadcast Kit: create the scene collection** — a collection called *Swarnil
  Broadcast Kit* with those scenes, switched to.
- **Broadcast Kit: add the live pack to this scene** — a light, a lower third, a
  ticker and a frame dropped into the current scene, placed.
- **Broadcast Kit: add my camera and microphone** — the same devices, into the
  scene you are on.
- **Broadcast Kit: use the Broadcast Kit profile** — switches to the installed
  profile.

Every built source is an ordinary source: select it, open Properties, change
anything. The scenes are a starting point, not a template you are locked into.

## Hotkeys

Under **Settings → Hotkeys**, per source: *play the lower third in again*,
*restart the countdown*, *nudge the goal up*, *nudge the goal down*, *refresh the
counter*.
