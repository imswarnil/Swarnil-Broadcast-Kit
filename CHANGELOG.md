# Changelog

All notable changes to Swarnil Broadcast Kit (called Tally until 0.3.0). The format follows [Keep a Changelog](https://keepachangelog.com/);
versions follow [SemVer](https://semver.org/).

## [0.7.2] — 2026-09-26

A page that answers the question people actually arrive with.

### Added
- **Five ready-made shows** at [obs.imswarnil.com/make/](https://obs.imswarnil.com/make/):
  a course, a podcast, a gaming set, a product demo and a vertical set. Each is
  a real scene collection — download it, import it, pick your camera once. A
  page that merely describes a layout is a page somebody has to reproduce by
  hand, and reproducing a layout by hand is the work this kit exists to remove.
- **They are generated, not drawn.** The specs live in `presets/` and the site's
  own build runs them through the same generator the agent skill uses. Every
  number and every scene list on the page is read back out of the file that was
  just built, so the page cannot promise a scene the collection does not
  contain, and a spec that stops validating fails the build.
- **"What the pieces make"**, eight combinations rather than a list of parts. The
  sources page answers *what is there*; this answers *what do I put where* — a
  camera that sits on the scene rather than floating on it, a waiting screen
  nobody minds staring at, chat without a browser source, a cut that carries
  your brand.
- **Three ways to build your own**, side by side with what each is actually for:
  the Tools menu, which is the only one that wires up your devices; the browser
  builder; and the agent skill.
- The preset specs are checked by `scripts/check-recipes.mjs` in CI, alongside
  every example in the skill's documentation. Thirteen now.
- **A real menu on a narrow screen.** Below 52rem the nav row became a strip of
  unlabelled icons, which is a guessing game. It is now a button that opens a
  panel with the words back on, and it closes every way a person expects: the
  button again, Escape, a click anywhere else, following a link, or the viewport
  growing wide enough that the panel should be a row again. Miss one of those and
  the menu is the thing on a site that feels broken.
- **A roadmap**, saying what is next and what is deliberately not — a universal
  bundle, Linux and Windows, reading a spec back into the plugin; and never a
  browser-source fallback, anybody's logos, or a hosted service.
- **A `Makefile`**, so every routine job is one word and `make` on its own lists
  them. `make check` is exactly what CI runs, in the order it runs it.
  `make shots` replaces the line of shell that imported a self-test run and
  quietly went wrong whenever the scene order changed — it now refuses rather
  than guesses if the counts disagree, because a run that is one short shifts
  every picture after the gap and the result looks plausible.
- **Issue and pull request templates**, and structured data plus search metadata
  on every page so the site can actually be found.

### Fixed
- **The builder's scale slider changed the export and nothing on screen.** The
  plugin grows type, padding and radius from one number; the preview did not, so
  the control was a lie. Every preview length is now a multiple of the same `u`,
  and moving the slider to 1.75 makes a 40px line 70px, exactly as it does in
  OBS.
- **Five sources drew as a grey box with a name on it.** Plate, Logo, Social,
  Prompt and Comments went into the palette before they went into the preview. A
  schematic may simplify; it may not be a placeholder, because a placeholder
  tells you nothing about whether the thing fits where you have put it.
- **The builder sized a plate as a generic box.** It shares the frame's aspect
  table — and the plugin's real numbers, where 21:9 is 756 × 324 rather than a
  round figure.
- **CI reported `deploy: success` for a deploy it had skipped.** The repository
  has no Cloudflare credentials, so the gate turned the step off and the job
  passed anyway — a green tick next to a site that had not changed. Every deploy
  so far has in fact been a manual `wrangler deploy`. The job now raises a
  warning and writes a plain-English notice into the run summary, and `CLAUDE.md`
  says outright that a green deploy does not mean the site moved.
- One of the new presets named a chip after the scene it sat in. The generator
  refused it, which is the check earning its place for the second time in two
  releases.

## [0.7.1] — 2026-09-26

An agent skill, pictures in the README, a value that was wrong in six places,
and the first release that says which Macs it runs on.

**The released bundle is Apple Silicon only.** The runner that builds it is
arm64 and so are the OBS frameworks it links, so that is what comes out. Nothing
said so before, and the site's own wording implied any Mac — which would have
sent the first Intel user to strip a Gatekeeper attribute that was never their
problem. The README, the install guide, the site and the release notes now say
it, and the artifact carries the architecture in its filename. On an Intel Mac,
build from source; `./build.command` produces a plugin for whatever machine it
runs on.

### Added
- **An agent skill**, `skills/sbk-scenes/`. Copy it into `~/.claude/skills/` and
  Claude Code — or anything else that reads the Agent Skills format — can design
  a scene, or a whole show, out of these sources and hand back a collection OBS
  will import. There is a generator you can run yourself without an agent, and
  `docs/SKILL.md` explains the whole of it.
- **Its reference is generated from the plugin's own C** by
  `scripts/skill-sync.mjs`: ids from each `obs_source_info`, names from the
  locale, keys and defaults from each `*_defaults()`, allowed values from the
  property lists, ranges from the sliders, and the shared Look and Motion groups
  from the headers that define them. Twenty-eight registrations, 387 settings.
  Written by hand that list would be wrong within a release; CI fails if the
  checked-in copy is stale. Anything the parser cannot resolve is printed as
  *set at runtime* rather than guessed at.
- **A scene-collection generator**, `build-collection.mjs`, which validates a
  spec before it writes anything: an unknown id or key with the near misses
  offered, a string outside an enumerated list with the list printed, a position
  off the canvas, and an item named after a scene — which OBS accepts and then
  silently never draws, because scenes and sources share one namespace. Its
  output is a strict subset of what OBS itself writes, checked by diffing
  against a real collection and by loading a generated one and reading the scene
  tree out of the log.
- **Pictures in the README**: six real self-test frames with a line each, and a
  section on using the kit commercially and on contributing to it.
- `scripts/check-recipes.mjs`, which runs every JSON block in the skill's
  documentation through the same validator the skill tells an agent to use. Both
  new checks run in CI.

### Fixed
- **The skill registry was stamped with the git commit, so it invalidated
  itself.** Regenerating after the commit that carried the last regeneration
  produced a different file, the check failed on a clean tree, and CI could
  never be green. It is stamped with a digest of the source files the parser
  actually read instead, which changes exactly when the answer changes — the
  only thing the stamp was ever for.
- **`variant: "glass"` is not a surface and never was.** The five are `card`,
  `pill`, `outline`, `accent` and `none`; anything else falls back to `card`, so
  four scenes in `scenes.c` and two entries in the web builder's palette had
  been quietly asking for something that does not exist and getting away with
  it. Found by the new example check on its first run, which is the argument for
  having written it.

## [0.7.0] — 2026-09-26

The mark, the handles, the ask, and two layouts that put a screen and a camera
in the same frame at different shapes.

### Added
- **`SBK Logo`**, a channel bug that is alive. Your PNG or one of fifteen drawn
  marks, with a loop: breathe, pulse, spin, a dot going round it, a ring drawing
  itself on and off, or a bob. It is driven by the clock, so it is already
  moving the first time the scene goes out — a page in a Browser Source cannot
  promise that, because a browser will not start anything until something has
  been clicked. Blown up to 400 px with a caption under it, the same source is
  the card a sign-off lands on.
- **`SBK Social`**, where to find you, in three shapes: a bar of every handle
  along an edge, a stack of them in a corner, and one at a time on a timer. The
  third is the one worth having — a row of six handles is read by nobody, and
  the same six shown for eight seconds each are read by everyone. Accounts are
  typed one per line as `platform: handle`.
- **`SBK Prompt`**, the like-and-subscribe card, on a timer. It slides in from an
  edge, holds, and slides back out, working through your lines. Two decisions
  make it bearable rather than irritating: it is genuinely off screen in
  between rather than parked behind your camera at zero opacity, and it can be
  told to stay quiet unless you are actually on air, so a rehearsal is not spent
  being asked to subscribe. A hotkey brings it in now.
- **A mark set**, fifteen of them, drawn as distance fields in `glyph.effect` and
  shared by all three: play, camera, at-sign, chat, heart, bell, star, share,
  globe, code, person, bookmark, plus, the recording ring, and an arrow. They are
  **generic on purpose** — never a company's logo. A platform is told apart by
  its colour and its name in type, which is what keeps this kit free to give
  away and what stops it going stale the week somebody redraws their mark.
- **Two layout scenes with a real display capture.** *Two up* gives the screen a
  wide 21:9 crop so code has room for long lines and puts a 9:16 camera column
  beside it, because matching the two aspect ratios would waste half the picture
  on a desk. *Three up* runs a 16:9 screen across the top with a square host and
  a 4:5 guest under it — square because it crops a talking head without cutting
  the shoulders, 4:5 because it is the shape every remote call hands you — and a
  meter under each, so a silent guest is visible at a glance.
- The social bar, the prompt and the logo bug are now in the show: handles
  stacked on Starting soon, the ask on Live and Screen share, the bug on Talking
  head and Two up, and every handle plus a drawing ring on Ending.

### Fixed
- **A brand colour too dark to see is no longer used.** Two of the platforms are
  black, which is fine on their own white pages and invisible on a dark overlay.
  Anything below a luminance threshold falls back to the Look's ink.
- **The display capture is hidden by the clean self-test**, alongside the camera.
  It is every bit as private, and a published screenshot must not carry whatever
  happens to be on the desktop.
- **Two plates were sized against the wrong numbers.** 21:9 is 756 × 324 in the
  frame's table and 1:1 is 480 × 480, not the round figures assumed, so the
  plates behind them sat proud of the frame.

## [0.6.0] — 2026-09-25

The show, rebuilt in the order you would run it, and the pieces a course needs.

### Added
- **`SBK Plate`**, the drop shadow OBS does not have. A source is composited
  flat, so a camera box over a backdrop has nothing under it and reads as a
  sticker. The plate is the missing layer: a rounded rectangle with a soft offset
  shadow, the same seven aspect ratios as the frame, and the shadow drawn in
  padding outside the shape so nothing is clipped. Optional fill for when the
  feed drops, optional accent glow instead of a shadow.
- **`SBK Comments`**, questions on screen. Typed one per line as
  `Name: question`, pulled from a **YouTube live chat** by video id, or read from
  any JSON endpoint. It holds forty and pages through them on a timer, so a queue
  that has got ahead of you still gets its turn. YouTube mode resolves the chat
  id from the video first, then polls at the interval the API asks for rather
  than at one we picked.
- **`SBK Logo Sting`**, a second transition. A colour field crosses as a band, an
  iris or a curtain, your PNG lands on it, and the field leaves on the next
  scene — the cut happens underneath where nobody sees it. No video file to
  render. The audio ducks through the middle rather than crossfading, which is
  the difference between a sting and a dissolve.
- **Three arrivals**: *pop* overshoots and settles, *grow* comes up from small,
  *settle* drops the last few pixels with a lift in scale. All three animate
  scale, which the stage could not do before — it now presents with a scale about
  the middle as well as an alpha and an offset.
- **Two teaching scenes.** *Screen share + two* stacks two cameras on one edge
  with a meter under each, so a shared screen keeps the middle of the frame and
  it is obvious who is talking. *Comments* gives the questions the right-hand
  column. A *Lesson* title card opens a module with a segment bar for how far
  through the set you are.
- **A Teaching preset** in the Broadcast Builder, and Plate and Comments in its
  palette.

### Changed
- **The whole show is rebuilt in running order.** Twenty scenes, built back to
  front so the Scenes panel reads from *Starting soon* at the top down to the
  private desk at the bottom, grouped as the opening, the teaching scenes, the
  talking scenes, the breaks, the closing, and one that is not for the stream at
  all. A scene that already exists is emptied and refilled rather than removed,
  so anything you dragged somewhere else stays where you put it.
- **Cameras sit on plates now** in Live, Screen share, Interview, Gameplay and
  Vertical, which is most of what makes the set look composited rather than
  stacked.
- The *Q&A* scene is gone; *Comments* does what it was for, with real questions
  in it.
- The self-test walks the running order itself rather than a hand-kept copy of
  it, so a new scene cannot be left out of the sweep.

### Fixed
- **A source named after a scene never appeared.** Scenes and sources share one
  namespace, so looking up "SBK · Comments" found the *scene* of that name, which
  was then quietly asked to contain itself. OBS refuses and logs nothing. The
  source is renamed, and building a scene now refuses a source named after one
  and says so in the log.
- **The self-test shot through the transition.** A 400 ms fade plus each source's
  own arrival meant a published picture could carry a ghost of the scene before
  it. The walk sets the duration to zero, cuts, waits for the scene to settle,
  and puts the duration back afterwards.
- **A null pattern argument crashed the build.** The Intermission scene asked for
  a backdrop with no extra settings and the helper dereferenced the null.
- **A plate sat a shadow's width off the box it was behind.** It reports itself
  bigger than its shape so the shadow has room to fall; anchoring one by a corner
  therefore misplaced the shape. Plates are now centred on the box's centre,
  which registers whatever the shadow is set to.
- **The comments panel repeated itself** when fewer questions existed than slots.

## [0.5.1] — 2026-09-25

The site, mostly.

### Added
- **A Broadcast Builder** at obs.imswarnil.com/builder/. Place the kit's pieces
  on a 1920 × 1080 canvas, set the words, and export a scene collection to import
  into OBS. The export is the real thing — the same source ids and setting keys
  the plugin registers, verified as a strict subset of what OBS itself writes, so
  nothing is approximated on the way in. The preview is deliberately a schematic:
  anything that looked pixel-perfect would only drift from the plugin the first
  time a shader changed.
- **Illustrations for the README**, generated by `docs/art/build.mjs` — a banner,
  a diagram of what a browser source cannot reach, and an annotated scene. Each
  is emitted for a dark and a light reader, because GitHub honours `<picture>`
  with a colour-scheme query but strips inline `<svg>` from markdown.
- **Icons in the navigation**, a two-column hero, the kit's own grid and dot
  patterns behind the section heads, and a Docs entry — `/setup/` moved to
  `/docs/` and leaves a redirect behind.

### Fixed
- **The nav icons rendered at the size of the whole bar.** They are 24-grid SVGs
  with no intrinsic size and the rule that constrained them never made it into
  the stylesheet: a search-and-replace had silently matched nothing. The theme
  toggle is now a labelled pill rather than an unmarked circle, which is the
  other half of why it was hard to find.

## [0.5.0] — 2026-09-25

Effects, a timer that does more than count down, five more scenes, and a control
panel inside OBS.

### Added
- **Audio filters**, which the kit had none of. **`SBK Voice`** is the chain a
  spoken voice wants — high-pass, gate, compressor, presence, saturation,
  limiter — in the right order, with presets that move all of it at once. OBS
  ships every one of these separately and most people never chain them, because
  doing so means understanding six dialogs first. **`SBK Radio`** is the
  character set: telephone, AM radio, megaphone, tannoy, walkie-talkie.
- **Two more video filters.** **`SBK Colour`** grades a source — exposure, white
  balance, contrast, vibrance, lift/gamma/gain — with presets that are
  corrections rather than looks. **`SBK Punch`** zooms into the picture on a
  hotkey and eases back out, which is the thing you want constantly in a tutorial
  and cannot do by hand mid-sentence.
- **`SBK Timer`** replaces the countdown and does four things: down to a
  duration, down to a time of day, up from zero, or up since the stream started.
  Four styles, including a progress ring that empties as the time runs out and
  turns red for the last ten seconds. Pause and restart hotkeys. The id is
  unchanged, so existing scenes keep working.
- **Four more backgrounds** — a drifting aurora, plasma, a twinkling starfield
  and checkers — bringing the backdrop to sixteen.
- **A control dock** (Docks → Broadcast Kit): uptime and bitrate, stream, record
  and mic, a button per scene, a button for every hotkey the kit registers, and
  the build actions. Stock Qt with no `Q_OBJECT`, so the build gains no code
  generation step.
- **Two more transitions** and **five more scenes** — Intermission, Podcast,
  Gameplay, Highlight and Music — which between them use every new background and
  timer style.

### Notes
- The build now wants `brew install qt` for the dock's headers. The frameworks
  come from OBS.app at the same version; linking Homebrew's would load a second
  Qt into the process and crash on the first widget.
- The self-test gained a clean variant (`.sbk-selftest-clean`) that hides every
  camera item for the walk and restores it after. An ordinary run captures
  whatever the webcam is pointed at, and those images are what the site
  publishes, so anything going on the web is taken this way.

## [0.4.0] — 2026-09-25

Filters, real devices, and a remote.

### Added
- **`SBK Round Corners`** — a *filter* that rounds the corners of the picture
  rather than covering them. A frame drawn on top is a rectangle with a hole in
  it; the camera's square corners are still underneath, so it only ever looked
  rounded against a matching background. Radius as a percentage of the shorter
  side, so one setting suits a webcam box and a full-frame share.
- **`SBK Scanlines`** — a CRT treatment for any source or whole scene:
  scanlines, aperture mask, colour fringing, barrel curvature, vignette, grain
  and flicker, with Fine / CRT / VHS / Arcade presets that just fill the sliders.
- **The camera and the microphone are set up for you.** Building the collection
  adds a camera with the rounding filter already on it, placed under the frame in
  every scene that has one and cropped rather than squashed, and puts a
  microphone on OBS's Mic/Aux channel — which is what `@mic` means to the meter
  and the visualizer. An input you already chose is left alone.
- **A phone remote** (`remote/`) driving OBS through the WebSocket server OBS
  already ships: scenes, stream and record, the mic, the transition, and a button
  for every hotkey the kit registers — read from OBS rather than hard-coded, so a
  new one appears without the remote changing. It carries its own SHA-256,
  because `crypto.subtle` does not exist at `http://192.168.x.x`.
- **Two more transitions** — a push, and a band of accent that crosses the frame
  and takes the cut with it, which is a stinger with no video file to render.
- The scene shots on the site open full size, and the site has a theme toggle
  that follows the system until you choose, applied before the first paint.

### Notes
- The screenshots on the site are still camera-free. A self-test run now contains
  whatever the webcam is pointed at, and those images are published.

## [0.3.0] — 2026-09-25

Renamed from **Tally** to **Swarnil Broadcast Kit**. Everything is `sbk_` now;
the phrase "tally light" survives only where it means the real broadcast lamp.
The kit grew from nine sources to sixteen, gained a transition, live API data, a
QR encoder, and a documentation site at obs.imswarnil.com.

### Added
- **Live data.** `SBK Counter` fetches a number from **YouTube**, **Ghost
  members** (signing a fresh five-minute Admin API token per request) or **any
  JSON endpoint** with a dot-path. `sbk-net.c` polls on a worker thread, floors
  the interval at fifteen seconds and keeps the last good value when a request
  fails. A key field beginning `@` is read from a file instead of the collection.
- **`SBK QR`** — a QR encoder written here (byte mode, versions 1–16, all four
  error-correction levels), verified by decoding its own output. Rounded
  modules, light-on-dark, the accent, and a logo hole that pins correction to H.
- **`SBK Meter`** — a level meter in real dB with peak hold and broadcast zones.
- **`SBK Stats`** — uptime, bitrate, dropped frames, render rate, health lamp.
- **`SBK Wipe`** — a real transition: bar, dip, slide, iris, blinds.
- **`SBK Chip`** and **`SBK Progress`** — a badge and a goal, both hotkeyable.
- **Program audio.** The visualizer and meter listen to OBS's master mix by
  default, via `obs_add_raw_audio_callback`, instead of only a microphone.
- **9:16 and friends.** The cam frame takes a shape — 16:9, 9:16, 1:1, 4:5, 4:3,
  21:9 — and a size slider, plus seven line treatments out of one distance field.
- **More of everything else.** Five light shapes, five lower-third variants,
  three ticker variants with end fades, seven visualizer styles, five card
  variants with centring, twelve backdrops including four drifting patterns.
- **The show** is thirteen scenes, now including Support (a membership QR and
  two live counters), Vertical (a 9:16 box for Shorts) and a private Desk.
- **obs.imswarnil.com** is a documentation site again — for the plugin, not
  overlays. Plain Node, no dependencies, built from `site/content.mjs`, with
  real self-test frames as the screenshots.

### Fixed
- **The self-test only ever saved one screenshot.** It slept on the UI thread,
  which is the thread OBS writes a queued screenshot on. The walk now runs on a
  worker and hands each step back with `obs_queue_task`.
- **The QR code could not be scanned**, twice over. Rounding every module the
  same way dissolved the three finder patterns, so rounding is now applied only
  to corners with no dark neighbour; and a `GS_R8` matrix texture with an odd
  module count had misaligned rows, so it goes up as `GS_RGBA`.
- **A double free when rebuilding the collection.** Removing a scene released
  everything in it, so a source used by only that scene began being destroyed
  while still answering to its name — the next lookup returned a corpse and OBS
  aborted inside malloc. Every source is now referenced for the whole build, and
  a scene that exists is emptied and refilled rather than removed and remade.
- **Centred cards were not centred.** A wrapped `text_ft2` source reports the
  width it was given, not the width of its glyphs, so the centring arithmetic
  cancelled out. Text is measured unwrapped first and only wrapped if it overruns.
- **The clock showed seconds everywhere** once the Desk scene turned them on:
  both scenes used one source name. Lagged frames were reported as a total since
  OBS started, which read as a fault on a healthy machine; it is now the recent
  change. Tracking a caps label with thin spaces drew boxes, because Geist has no
  U+2009 — that is now a plain caps label.

## [0.2.0] — 2026-09-25

Tally is now a **native OBS plugin**. The hosted site, the browser-source
overlays, the Node build, the dev server and the Cloudflare Worker are gone;
nothing is served anywhere. Everything the overlays did is now a real OBS
source, drawn by libobs, configured in OBS's own Properties dialog.

### Added
- `tally.plugin`: nine sources — Tally Light, Lower Third, Ticker, Frame,
  Visualizer, Clock, Countdown, Card, Backdrop — with a shared Look (accent,
  scale, tone, font) and an enter animation.
- Tools menu: build the four scenes, create the *Tally* collection, add the
  live pack to a scene, switch to the Tally profile.
- A fresh 1080p60 profile (`profile/Tally/`) with an installer.
- `build.command`: compiles and installs the plugin, the Geist fonts and the
  profile. CI builds on a clean Mac and attaches the bundle to tags.
- A self-test trigger (`.tally-selftest`) that builds the collection and
  screenshots every scene.

### Removed
- `src/` (CSS + JS runtime), `overlays/`, `site/`, `scripts/`, `scenes/`
  (the generated JSON), `wrangler.jsonc`, `package.json`, the three workflows.

## [0.1.0] — 2026-09-24

The base. Everything here is the first cut and will be refined one piece at a time.

### Added
- Tokens (`--tally-*`), a transparent base, the glass panel, the recording-light dot.
- Components: lower third, on-air light, chip, card, webcam frame, ticker, clock, countdown.
- Visualizer: bars, wave, ring, dots, from the microphone or a synthetic demo signal.
- Layout: the announcement layout used by Starting soon and Be right back.
- Overlays: `lower-third`, `onair`, `visualizer`, `frame`, `ticker`, `starting-soon`, `brb`.
- The runtime: URL params, the OBS bridge, the audio engine, painters, widgets.
- A generated OBS scene collection (four scenes) and a 1080p60 profile.
- The docs site at obs.imswarnil.com: catalogue, per-overlay page with a live preview and a
  URL builder, install guide, scenes page.
- CI (build, check, pack), deploy to Cloudflare Workers on push to `main`, and a release
  workflow that attaches the offline pack to a tag.
