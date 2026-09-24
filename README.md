# Tally

**Stream overlays, audio visualizers and scene layouts for OBS Studio.**
Every overlay is a URL: add it as a Browser Source, tune it with a few parameters, done.

→ **https://obs.imswarnil.com**

Tally is named after the tally light, the small red lamp on a studio camera that says *this
one is on air*. Its own on-air overlay reads OBS's real state, so the lamp turns red when
you go live rather than when you remember to click something.

## What ships

| Overlay | Kind | Size | What it does |
| --- | --- | --- | --- |
| `lower-third` | Component | 1920×1080 | A name and a line under it, the accent as a bar. Slides in on load. |
| `onair` | Component | 1920×1080 | The tally light. LIVE while streaming, REC while recording, OFF AIR otherwise. |
| `frame` | Component | 640×400 | A rounded outline with a chip on its edge, to sit over the camera. |
| `ticker` | Component | 1920×120 | A strip of text sliding across the foot of the screen. |
| `visualizer` | Visualizer | 1920×240 | Bars, a waveform, a ring or a dot matrix, from the microphone. |
| `starting-soon` | Scene | 1920×1080 | A card with a countdown, chips, a visualizer along the foot, a clock. |
| `brb` | Scene | 1920×1080 | The break screen: the same card, worded for a pause, with the on-air light. |

Plus a **scene collection** (`scenes/Tally.json`: Starting soon, Live, Be right back,
Ending) and a **1080p60 profile** (`scenes/profile/Tally/basic.ini`), both importable
from OBS's menus.

## Use it

1. Open an overlay on the site, tune it, press **Copy**.
2. In OBS: **Sources → + → Browser**, paste the URL, set the width and height shown.

Four parameters work on every overlay: `accent`, `scale`, `tone`, `font`. Give each
source the same `?accent=00a3ff` and the scene changes colour together.

Offline: every [release](https://github.com/imswarnil/Tally/releases) attaches a zip of
the overlay pages, the runtime and the fonts, for a Browser Source set to *Local file*.

## Develop it

```bash
nvm use            # Node 22
npm install
npm run dev        # http://localhost:4800 — rebuilds on change
npm run build      # dist/ — the whole site plus the overlays
npm run check      # what CI runs: registry ↔ pages, params, scenes, namespace, links
npm run scenes     # regenerate scenes/Tally.json from scenes/scenes.config.mjs
npm run pack       # dist/pack/tally-<version>.zip, the release asset
```

```
src/          tokens → base → components → visualizers → layouts, all `tally-` / `--tally-`
src/js/       the runtime: params, the OBS bridge, the audio engine, the painters, widgets
overlays/     one folder per overlay, plus registry.mjs — the single list of what exists
scenes/       the scene-collection config, its generated JSON, and the profile
site/         the docs site: a builder, previews, install and scenes pages
scripts/      build, dev, check, scenes, pack
```

**Add an overlay:** create `overlays/<slug>/index.html` (copy a neighbour), add an entry to
`overlays/registry.mjs`, and if it belongs in the collection add it to
`scenes/scenes.config.mjs` and run `npm run scenes`. The docs page, the URL builder, the
catalogue card, the pack and the checks all follow from the registry.

## Ship it

- Push to `main` → GitHub Actions builds, checks and deploys `dist/` to a Cloudflare Worker
  serving `obs.imswarnil.com` (`.github/workflows/deploy.yml`). Needs the repository
  secrets `CLOUDFLARE_API_TOKEN` and `CLOUDFLARE_ACCOUNT_ID`; without them the build still
  runs and the deploy step is skipped.
- Pull requests run `ci.yml`: build, check, pack.
- `npm version minor && git push --follow-tags` → `release.yml` attaches the pack to a
  GitHub release.

## Design

Tally speaks the visual language of the [Im Design System](https://design.imswarnil.com) —
a neutral palette, Geist, one accent, a pill, a recording-light dot — but shares no code
with it. It is its own small system with its own namespace, so it can be MIT while the
design system is not.

## Licence

MIT. Geist and Geist Mono are © Vercel under the SIL Open Font License 1.1.
