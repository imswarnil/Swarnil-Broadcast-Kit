# Changelog

All notable changes to Swarnil Broadcast Kit (called Tally until 0.3.0). The format follows [Keep a Changelog](https://keepachangelog.com/);
versions follow [SemVer](https://semver.org/).

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
- The five new scenes have no screenshot on the site yet. The page says so rather
  than showing a broken image.

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
