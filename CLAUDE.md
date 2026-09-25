# Swarnil Broadcast Kit — lives in `obs.imswarnil.com/`

A **native OBS Studio plugin** (`sbk.plugin`), plus the documentation site served
at **https://obs.imswarnil.com**. Repo `imswarnil/Swarnil-Broadcast-Kit`, public,
MIT code. Read `README.md` first.

Named **Tally** until 2026-09-25, when it was renamed. Nothing should say Tally
any more except the phrase "tally light", which is the real broadcast lamp the
Light source is named after.

## What it is, in one breath

C built with CMake against `/Applications/OBS.app`, the same way every plugin in
`~/OBS/` is built. Fifteen `sbk_*` sources in OBS's "+" menu, two filters under Filters,
one transition in the Scene Transitions panel, five Tools-menu actions that build
a thirteen-scene show, and a profile. Type is OBS's own `text_ft2_source` as a private child;
every box is `card.effect`; anything that animates draws into an `sbk_stage`
(offscreen render target) and is presented with an alpha and an offset.

## Rules

- **The overlays are native. Only the docs are a website.** No browser source,
  no HTML overlay, no server in the render path. `site/` builds a documentation
  site and nothing the plugin draws is ever fetched from it.
- **Namespace `sbk_` / `SBK_` / `SBK.`** for source ids, hotkeys, locale keys.
  The kit is *inspired by* the Im Design System (neutral ramp, Geist, one accent,
  the recording-light dot, pills) but copies no code from it, and none from
  `~/OBS/nsds-plugin` (which belongs to Namaste Salesforce). Kept apart in both
  directions: that system is sold, this is MIT.
- **Every length is `u × n`** where `u = sbk_u(&look)` = 4px × scale. No bare
  pixel counts outside `sbk-common.h` tokens and property defaults; the scale
  slider must grow a component uniformly.
- **Never depend on a click.** Anything that moves starts in `create()`/`show()`.
  The audio engine paints a demo signal when it hears nothing.
- **The graphics thread never waits.** Network polling lives in `sbk-net.c` on a
  worker; the source reads the last good value under a mutex. A failed poll keeps
  the previous value on screen rather than showing a zero.
- **Text children re-rasterise only on change** — `sbk_text_set*` compares first.
- **Stage in premultiplied blending, present ONE/INVSRCALPHA.** Direct-draw
  sources (visualizer, backdrop, QR) use plain SRCALPHA/INVSRCALPHA.
- **Nothing in the build reads outside this folder.** libobs headers are in
  `deps/include` (GPL-2.0, see `deps/README.md`); SIMDe and jansson come from
  Homebrew, curl from macOS.
- **Quit OBS before `./build.command`.** A loaded plugin cannot be replaced.

## Where things are

```
src/sbk-common.h     tokens (ABGR), sbk_card/_fill/_dot, struct sbk_look, surfaces
src/sbk-text.h       struct sbk_text: set / measure / draw / free / enum
src/sbk-stage.h      struct sbk_stage: begin / end / present (+ edge fade)
src/sbk-anim.h       the enter animation
src/sbk-state.c      sbk_status + sbk_state_word() + sbk_state_uptime()
src/sbk-audio.c      "@program" (raw mix), "@desktop", "@mic", a source, or demo
src/sbk-net.c        polled GET on a worker + JSON dot-path walk
src/sbk-qr.c         QR encoder, versions 1–16, all four ECC levels
src/source-*.c       onair lower-third ticker frame visualizer meter stats counter
                     qr card backdrop chip progress clock countdown
src/filter-round.c   rounds the SOURCE's corners, not an overlay over them
src/filter-scanlines.c  the CRT treatment, with presets
src/transition-wipe.c
src/scenes.c         the show, live pack, profile, self-test
data/effects/        card frame viz backdrop qr wipe blit
site/                content.mjs (the registry), build.mjs, check.mjs, site.css
remote/              the phone remote: index.html + a hand-rolled sha256.js
docs/screens/        real self-test frames; the site uses them
profile/Swarnil Broadcast Kit/basic.ini + profile/install.command
```

## OBS facts the code relies on

- `obs_add_raw_audio_callback(0, &conv, cb, param)` is the **program mix**. A
  named source uses `obs_source_add_audio_capture_callback`; channels 1–5 are
  Desktop Audio 1–2 and Mic/Aux 1–3 via `obs_get_output_source`.
- `text_ft2_source`: `font{face,style,size,flags}`, `text`, `color1/2` (ABGR),
  `custom_width` + `word_wrap`, `drop_shadow`. **A wrapped source reports the
  custom width, not the glyph width**, so `source-card.c` measures unwrapped
  first and only wraps the line that overruns — otherwise centring is a no-op.
- `gs_texture_create` with **GS_R8 and an odd width misaligns its rows**. The QR
  matrix goes up as GS_RGBA for that reason; it looked like a QR code and could
  not be scanned.
- A **uniformly rounded QR module destroys the finder patterns.** `qr.effect`
  rounds a corner only when both neighbours are light.
- The frontend API can list and select transitions but **cannot add one** — only
  the Scene Transitions panel's "+" can. `scenes.c` selects SBK Wipe if present
  and logs how to add it if not.
- Screenshots are taken on the UI thread, so **the self-test walk must not block
  it**: `scenes.c` walks on a worker and hands each step back with
  `obs_queue_task`. Sleeping on the UI thread collapsed thirteen shots into one.
- `obs_get_lagged_frames()` is a total since OBS started; the stats panel reports
  the recent change instead, or it reads as a fault on a healthy machine.
- A **filter** must call `obs_source_skip_video_filter` on any path where it does
  not render, or the source vanishes rather than passing through untouched.
- A device list is only populated once a source of that type exists, so
  `first_device()` creates one privately, reads its properties and drops it.
- **`crypto.subtle` does not exist outside a secure context.** The remote is
  opened at `http://192.168.x.x`, which is not one, so it carries its own
  SHA-256. Do not "simplify" that back to the Web Crypto API.
- **A page served over https cannot open a `ws://` socket.** obs-websocket has no
  wss, so the remote can only ever be served over http from the user's own
  machine. The copy on the site says so when you press Connect.

## Screenshots are published

`docs/screens/*.jpg` go straight onto obs.imswarnil.com. Since the collection now
adds a real camera, a self-test run contains **whatever the webcam is pointed at**.
Remove or hide the `Camera` source before taking shots meant for the site, and
never commit a run that has a person in it without asking first.

## Verifying a build

```bash
touch ~/Library/Application\ Support/obs-studio/.sbk-selftest   # then start OBS
```

It builds the collection, walks all thirteen scenes and takes a program
screenshot of each into the profile's recording folder. Then read the newest log
in `~/Library/Application Support/obs-studio/logs/`: `[sbk] v… loaded`, no
`would not create` / `failed to compile`, `walked 13 scenes`.

The QR round-trips: crop a rendered frame and decode it with `CIDetector`
(a small Swift tool does this; see the CHANGELOG entry for 0.3.0).

## The site

Live at **https://obs.imswarnil.com** — Cloudflare Worker `sbk-obs`, static
assets, Worker route `obs.imswarnil.com/*` on the imswarnil.com zone. DNS is a
**proxied AAAA `obs` → `100::`**: a placeholder with no origin, because the
route answers everything. It replaced a leftover CNAME to `imswarnil.github.io`
that served nothing and would have quietly 404'd from the links site if the
route were ever removed.

```bash
node site/build.mjs && node site/check.mjs && npx wrangler@4 deploy
```

CI does the same on a push to `main`, but only once the repo has the secrets
`CLOUDFLARE_API_TOKEN` and `CLOUDFLARE_ACCOUNT_ID`; without them the site is
built and checked and the deploy step is skipped.

## Dev

```bash
./build.command             # quit OBS first
node site/build.mjs && node site/check.mjs
```

Release: `git tag vX.Y.Z && git push --follow-tags` → CI attaches the bundle.
The version lives in `CMakeLists.txt` and `CHANGELOG.md`.
