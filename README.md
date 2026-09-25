<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/art/banner-dark.svg">
  <img alt="SBK — Swarnil Broadcast Kit. Overlays OBS draws itself. A native plugin, not a browser source: fifteen sources, six filters, a transition, a control dock and a show of eighteen scenes." src="docs/art/banner-light.svg">
</picture>

# Swarnil Broadcast Kit

**Native OBS Studio overlays: fifteen sources, six filters, a transition, a
control dock and a show of eighteen scenes — drawn by OBS itself.**

No browser source, no web server, no URL to paste. Drop `sbk.plugin` into OBS
and *Sources → +* fills up with **SBK …** sources drawn by libobs: type is OBS's
own FreeType text, every box is one small shader. The *Tools* menu builds a
whole show out of them.

→ **[obs.imswarnil.com](https://obs.imswarnil.com)** — documentation, a picture
of every scene, and a [builder](https://obs.imswarnil.com/builder/) that exports
a scene collection you can import straight into OBS.

---

## Why a plugin

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/art/native-dark.svg">
  <img alt="A comparison. A page in a Browser Source cannot know you are live, hear the program mix, see dropped frames, be a transition, or filter the picture or the voice. A native plugin can do all six." src="docs/art/native-light.svg">
</picture>

These are not stylistic preferences. Each one is something a page inside a
Browser Source has no way to do — and a browser source also costs a whole
Chromium process per overlay, against one or two draw calls per source.

---

## Anatomy of a scene

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/art/anatomy-dark.svg">
  <img alt="An annotated 16:9 scene showing where each source sits: the light top left, a card beneath it, a countdown ring to the right, a camera frame below that, a visualizer along the foot and a ticker under it." src="docs/art/anatomy-light.svg">
</picture>

Every piece is an ordinary OBS source. Select it, open Properties, change
anything. The scenes the kit builds are a starting point, not a template you are
locked into.

---

## What ships

### Sources

| Source | What it does |
| --- | --- |
| **SBK Light** | The tally light. LIVE / REC / OFF AIR from OBS's own state. Five shapes: pill, badge, bare dot, a bar across the frame, or the whole canvas edge lit red. |
| **SBK Lower Third** | A name and a line under it. Card, pill, split, minimal or underline; replays its arrival on a hotkey. |
| **SBK Ticker** | A tag and items sliding across the foot. On glass, bare, or one chip per item, fading at both ends. |
| **SBK Cam Frame** | The treatment over your camera. 16:9, **9:16**, 1:1, 4:5, 4:3 or 21:9, and seven line treatments. |
| **SBK Visualizer** | Bars, mirrored bars, waveform, dot matrix, ring, block ladder or filled line — from the **program mix** by default. |
| **SBK Meter** | A real level meter in dB, with peak hold and the zones a broadcaster expects. |
| **SBK Stats** | Uptime, bitrate, dropped frames, render rate, and a health lamp. |
| **SBK Counter** | A live number from **YouTube**, **Ghost members**, or any JSON endpoint, with an optional goal bar. |
| **SBK QR** | A scannable code for a membership page, a donation link or your site. Generated in the plugin. |
| **SBK Card** | The announcement: eyebrow, title, body, chips. Five variants. |
| **SBK Backdrop** | Sixteen grounds: solid, scrim, vignette, gradient, grid, dots, stripes, waves, rings, hex, grain, aurora, plasma, starfield, checkers — and they drift. |
| **SBK Chip** | One badge: a handle, a count, a “Q&A”, with a dot that can light only when you are live. |
| **SBK Progress** | A goal, nudged up and down on a hotkey. |
| **SBK Timer** | Down to a duration or a time of day, or up from zero or since the stream started — digits, a ring, or a bar. |
| **SBK Clock** | The time, in the mono face. |

### Filters

| Filter | What it does |
| --- | --- |
| **SBK Round Corners** | Rounds your camera's corners — the picture itself, not a frame over it. |
| **SBK Colour** | A grade: exposure, white balance, contrast, vibrance, lift/gamma/gain. Seven presets, all corrections rather than looks. |
| **SBK Punch** | A zoom into the picture on a hotkey — it eases in, holds, and eases back. |
| **SBK Scanlines** | A CRT treatment: scanlines, aperture mask, fringing, curvature, grain. Four presets. |
| **SBK Voice** | *Audio.* High-pass, gate, compressor, presence, saturation, limiter, in the right order, with presets. |
| **SBK Radio** | *Audio.* Telephone, AM radio, megaphone, tannoy, walkie-talkie. |

### And

**SBK Wipe**, a real transition — bar, dip, slide, push, iris, blinds, or a band
of accent that takes the cut with it. A **control dock** inside OBS. A **phone
remote**. An **eighteen-scene show** and a 1080p60 profile.

Every source shares a **Look** group — one accent colour, a scale slider that
grows type, padding and radius together, a tone (glass, solid or light) and the
font. Give every source the same accent and the scene changes together.

---

## Install

1. Quit OBS. Put `sbk.plugin` from a [release](https://github.com/imswarnil/Swarnil-Broadcast-Kit/releases) into
   `~/Library/Application Support/obs-studio/plugins/`.
2. Copy the files in `fonts/` into `~/Library/Fonts` — or pick any installed
   font in a source's *Look*.
3. Open OBS. **Tools → Broadcast Kit: create the scene collection** builds the
   show, switches to it, and puts your camera and microphone in — the camera
   with its corners genuinely rounded by a filter, not covered by one.
4. Add **SBK Wipe** from the Scene Transitions panel's **+**.

From source, `./build.command` does all of it. See [docs/INSTALL.md](docs/INSTALL.md).

## Control it

A **dock inside OBS** (Docks → Broadcast Kit): stream, record and mic, the
uptime and bitrate, a button per scene, a button for every hotkey the kit
registers, and the build actions.

A **phone remote** — `remote/serve.command` serves it on your own network and it
drives OBS through the WebSocket server OBS already has. It has to be served
over plain `http` from your own machine, and that is not a shortcut: a page
loaded over `https` cannot open the unencrypted `ws://` connection obs-websocket
speaks, so a hosted copy could never reach your OBS.

## Live numbers

`SBK Counter` polls on a worker thread — the picture never waits on the network,
and a failed request keeps the last good number rather than blinking to zero.

- **YouTube** — an API key and a channel id.
- **Ghost** — your site and an Admin API key; the kit signs a fresh
  five-minute token for every request.
- **Any JSON endpoint** — a URL and a dot-path such as `data.total`.

Keys typed into a source are saved in the scene collection as plain text.
Begin the field with `@` and a path — `@/Users/you/.youtube-key` — and the kit
reads the key from the file instead.

## Develop it

```
src/sbk-common.h      tokens, the card and ring helpers, the shared Look and surfaces
src/sbk-text.h        a line of type as a child text_ft2 source
src/sbk-stage.h       offscreen render target: fades, slides, clipping, edge fades
src/sbk-anim.h        the arrival
src/sbk-state.c       streaming / recording state and uptime, from the frontend
src/sbk-audio.c       program mix, channels or any source → FFT → bands and levels
src/sbk-dsp.h         biquads, compressor, limiter, gate — the audio filters' maths
src/sbk-net.c         the polled HTTPS GET, and the JSON dot-path walk
src/sbk-qr.c          a QR encoder: byte mode, versions 1–16, all four ECC levels
src/source-*.c        one file per source
src/filter-*.c        round, scanlines, colour, punch, voice, radio
src/transition-wipe.c the transition
src/dock.cpp          the control panel; Qt headers from Homebrew, frameworks from OBS
src/scenes.c          the show, the live pack, the profile switch, the self-test
data/effects/         card, frame, ring, viz, backdrop, qr, wipe, blit, round, scanlines, colour, punch
site/                 the documentation site and the builder (plain Node, no dependencies)
remote/               the phone remote: one page, one hand-rolled SHA-256, no build
docs/art/             the illustrations above, generated by docs/art/build.mjs
docs/screens/         real frames from the self-test, used by the site
deps/include/         libobs headers for OBS 32.2.2 (GPL-2.0; see deps/README.md)
```

```bash
brew install cmake simde jansson qt
./build.command          # build, install the plugin, fonts and profile (quit OBS first)
node site/build.mjs      # the docs site → dist/
node site/check.mjs      # what CI runs on it
node docs/art/build.mjs  # regenerate the illustrations
```

Nothing is downloaded at build time. CI builds the plugin on a clean Mac with
OBS from Homebrew, checks that every source is registered, builds the site and
deploys it; a `v*` tag attaches the bundle to a release.

## Design

The kit speaks the visual language of the
[Im Design System](https://design.imswarnil.com) — a neutral palette, Geist, one
accent, pills, the recording-light dot — and **shares no code with it**. That
system is all-rights-reserved and sold; this is MIT and public, so the two are
kept independent in both directions.

## Licence

The kit's code is MIT. A compiled plugin links **libobs** (GPL-2.0), whose
headers are vendored in `deps/`, so the binary is distributed under the GPL's
terms. Geist and Geist Mono are © Vercel under the SIL Open Font License 1.1.
