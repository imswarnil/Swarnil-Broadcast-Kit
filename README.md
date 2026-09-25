# Swarnil Broadcast Kit

**Native OBS Studio overlays: fifteen sources, two filters, a transition and a
show of thirteen scenes — drawn by OBS itself.**

No browser source, no web server, no URL to paste. Drop `sbk.plugin` into OBS
and *Sources → +* fills up with **SBK …** sources drawn by libobs: type is OBS's
own FreeType text, every box is one small shader. The *Tools* menu builds a
whole show out of them.

→ **[obs.imswarnil.com](https://obs.imswarnil.com)** — the documentation, with a
picture of every scene.

## Why native

These are not stylistic preferences. Each one is something a page inside a
Browser Source has no way to do:

| | |
| --- | --- |
| **Know you are live** | The light reads OBS's own streaming and recording state. |
| **Hear the program mix** | The visualizer and meter listen to what OBS is actually outputting — every source, every filter. |
| **See dropped frames** | Uptime, bitrate, dropped frames and congestion come from the running output. |
| **Be a transition** | A transition is composited between two scene textures; nothing in a page sees both. |
| **Cost almost nothing** | One or two draw calls per source instead of a Chromium process per overlay. |
| **Work offline** | The QR code is generated in the plugin. No third party sees your link. |

## What ships

| Source | What it does |
| --- | --- |
| **SBK Light** | LIVE / REC / OFF AIR from OBS's own state. Five shapes: pill, badge, bare dot, a bar across the frame, or the whole canvas edge lit red. |
| **SBK Lower Third** | A name and a line under it. Card, pill, split, minimal or underline; replays its arrival on a hotkey. |
| **SBK Ticker** | A tag and items sliding across the foot. On glass, bare, or one chip per item, fading at both ends. |
| **SBK Cam Frame** | The treatment over your camera. 16:9, **9:16**, 1:1, 4:5, 4:3 or 21:9, and seven line treatments. |
| **SBK Visualizer** | Bars, mirrored bars, waveform, dot matrix, ring, block ladder or filled line — from the **program mix** by default. |
| **SBK Meter** | A real level meter in dB, with peak hold and the zones a broadcaster expects. |
| **SBK Stats** | Uptime, bitrate, dropped frames, render rate, and a health lamp. |
| **SBK Counter** | A live number from **YouTube**, **Ghost members**, or any JSON endpoint, with an optional goal bar. |
| **SBK QR** | A scannable code for a membership page, a donation link or your site. Generated in the plugin. |
| **SBK Card** | The announcement: eyebrow, title, body, chips. Five variants. |
| **SBK Backdrop** | Twelve grounds: solid, scrim, vignette, gradient, grid, dots, stripes, waves, rings, hex, grain — and they drift. |
| **SBK Chip** | One badge: a handle, a count, a “Q&A”, with a dot that can light only when you are live. |
| **SBK Progress** | A goal, nudged up and down on a hotkey. |
| **SBK Clock** · **SBK Countdown** | The time, and a countdown to a duration or a time of day. |
| **SBK Round Corners** | *A filter.* Rounds your camera's corners — the picture itself, not a frame over it. |
| **SBK Scanlines** | *A filter.* A CRT treatment for any source: scanlines, aperture mask, fringing, curvature, grain. Four presets. |
| **SBK Wipe** | A real transition: bar, dip, slide, push, iris, blinds, or a band of accent that takes the cut with it. |

Every source shares a **Look** group — one accent colour, a scale slider that
grows type, padding and radius together, a tone (glass, solid or light) and a
font. Give every source the same accent and the scene changes together.

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

## Live numbers

`SBK Counter` polls on a worker thread — the picture never waits on the
network, and a failed request keeps the last good number rather than blinking
to zero.

- **YouTube** — an API key and a channel id.
- **Ghost** — your site and an Admin API key; the kit signs a fresh
  five-minute token for every request.
- **Any JSON endpoint** — a URL and a dot-path such as `data.total`.

Keys typed into a source are saved in the scene collection as plain text.
Begin the field with `@` and a path — `@/Users/you/.youtube-key` — and the kit
reads the key from the file instead.

## Control it from your phone

`remote/serve.command` serves a remote on your own network. It drives OBS
through the WebSocket server OBS already has: scenes, stream and record, the
mic, the transition, and a button for every hotkey the kit registers — read out
of OBS rather than hard-coded.

It has to be served over plain `http` from your own machine, and that is not a
shortcut: a page loaded over `https` cannot open the unencrypted `ws://`
connection obs-websocket speaks, so a hosted copy could never reach your OBS.
Nothing goes through the website, and the password stays in that phone's
browser.

## Develop it

```
src/sbk-common.h      tokens, the card shader helper, the shared Look and surfaces
src/sbk-text.h        a line of type as a child text_ft2 source
src/sbk-stage.h       offscreen render target: fades, slides, clipping, edge fades
src/sbk-anim.h        the arrival
src/sbk-state.c       streaming / recording state and uptime, from the frontend
src/sbk-audio.c       program mix, channels or any source → FFT → bands and levels
src/sbk-net.c         the polled HTTPS GET, and the JSON dot-path walk
src/sbk-qr.c          a QR encoder: byte mode, versions 1–16, all four ECC levels
src/source-*.c        one file per source
src/filter-*.c        rounded corners, and the CRT treatment
src/transition-wipe.c the transition
src/scenes.c          the show, the live pack, the profile switch, the self-test
data/effects/         card, frame, viz, backdrop, qr, wipe, blit
site/                 the documentation site (plain Node, no dependencies)
remote/               the phone remote: one page, one hand-rolled SHA-256, no build
docs/screens/         real frames from the self-test, used by the site
deps/include/         libobs headers for OBS 32.2.2 (GPL-2.0; see deps/README.md)
```

```bash
brew install cmake simde jansson
./build.command          # build, install the plugin, fonts and profile (quit OBS first)
node site/build.mjs      # the docs site → dist/
node site/check.mjs      # what CI runs on it
```

Nothing is downloaded at build time. CI builds the plugin on a clean Mac with
OBS from Homebrew, checks that every source is registered, builds the site and
deploys it; a `v*` tag attaches the bundle to a release. The site deploys by
hand with `npx wrangler@4 deploy` — a Cloudflare Worker (`sbk-obs`) serving
`dist/` on a route over the `obs` hostname.

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
