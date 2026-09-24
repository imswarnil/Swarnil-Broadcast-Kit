# Tally — lives in `obs.imswarnil.com/`, deployed to https://obs.imswarnil.com

OBS Studio overlays as web pages. Repo `imswarnil/Tally` (public, MIT). Read `README.md` first.

## What it is, in one breath

Every overlay is one HTML file under `overlays/<slug>/` that loads `assets/tally.css` and
`assets/tally.js` and is configured by URL parameters. `overlays/registry.mjs` is the single
list of overlays, their sizes and their params; the docs, the URL builder, the scene
collection, the pack and `npm run check` are all derived from it. Nothing is listed twice.

## Rules

- **Namespace is `tally-` / `--tally-`, nothing else.** `npm run check` fails on `im-`, `ck-`
  or `kg-` in `src/`. Tally is *inspired by* the Im Design System (neutral ramp, Geist, one
  accent, the recording-light dot, pills) but copies no code from it: the design system is
  all-rights-reserved and sold; Tally is MIT. Keep the two apart in both directions.
- **An overlay page must work without its own script.** The runtime mounts everything from
  `data-tally-*` attributes. A page-specific `<script type="module">` is fine for URL → DOM
  plumbing (chips, sizes) and must come BEFORE the runtime `<script>` so its changes land
  before `Tally` mounts.
- **The page is transparent and sized for a 1080p canvas.** Everything scales through
  `--tally-scale`, so never hard-code a pixel size outside `tokens.css`; `?scale=2` must grow
  it all.
- **Never depend on a click.** OBS never clicks. The demo audio signal is pure maths, not an
  AudioContext, because browsers suspend audio until a gesture. Anything that animates must
  start on load.
- **Never hand-edit `scenes/Tally.json`.** Edit `scenes/scenes.config.mjs`, run
  `npm run scenes`; check fails if the JSON is stale.
- **Params must be declared.** A page may only read `?keys` that are in its registry entry or
  in the runtime's global set (check enforces this by grepping the HTML).
- **CI builds on a clean machine.** Nothing in the build may read outside this folder.

## Where things are

```
src/tokens/tokens.css     every colour, size, font, ease
src/base.css              transparent page, fonts, .tally-stage / .tally-pin / .tally-glass / .tally-dot
src/components/*.css      lower-third, onair, chip, card, frame, ticker, clock (+ count)
src/visualizers/          the <canvas> styling; painters are in src/js/viz.mjs
src/layouts/layouts.css   .tally-layout-announce (starting soon / brb / ending), .tally-brand
src/js/tally.js           entry; params.mjs, obs.mjs, audio.mjs, viz.mjs, widgets.mjs
overlays/registry.mjs     THE list. slug, name, kind, size, params, example
scenes/scenes.config.mjs  the collection; scenes/profile/Tally/basic.ini the profile
site/build.mjs            template functions; site.css, site.js (previews, URL builder)
scripts/                  build.mjs, dev.mjs (port 4800), check.mjs, scenes.mjs, pack.mjs
```

## OBS facts the code relies on

- Browser Source injects `window.obsstudio` and fires `obsStreamingStarted/Stopped`,
  `obsRecordingStarted/Paused/Unpaused/Stopped`, `obsSceneChanged`, `obsSourceVisibleChanged`,
  `obsSourceActiveChanged` on `window`; `obsstudio.getStatus(cb)` gives the initial state.
  `src/js/obs.mjs` maps these to `data-tally-state` on `<html>` and the `tally:status` event.
- `getUserMedia` inside a Browser Source hands over the system's default input device. Tally
  asks with `echoCancellation/noiseSuppression/autoGainControl: false`; if refused it paints
  the demo signal and logs one `console.info`.
- obs-browser is Chromium 103+; esbuild targets `chrome103`.
- A local-file Browser Source has no query string; the pack's README tells people to edit
  `data-tally-default` attributes instead.

## Gotchas already hit

- `[data-part] { display: inline-block }` beat the `hidden` attribute on the countdown's hours;
  `base.css` now has `[hidden] { display: none !important }`.
- `.site a { color }` (0,1,1) outranked `.btn--primary` (0,1,0) on the docs site, so the
  primary button's text vanished. Site rules that fight `.site a` are written `.site .x`.
- A preview iframe is laid out at the overlay's real size and CSS-transformed down; `site.js`
  measures the box and sets `--scale`. CSS alone cannot divide two lengths.

## Dev

```bash
nvm use && npm install
npm run dev        # http://localhost:4800
npm run build && npm run check
```

Deploy: push to `main` (Cloudflare Worker `tally-obs`, route `obs.imswarnil.com/*`). Secrets
`CLOUDFLARE_API_TOKEN` + `CLOUDFLARE_ACCOUNT_ID` on the repo; a proxied DNS record for `obs`
in the imswarnil.com zone. Release: `npm version minor && git push --follow-tags`.
