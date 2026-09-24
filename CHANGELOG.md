# Changelog

All notable changes to Tally. The format follows [Keep a Changelog](https://keepachangelog.com/);
versions follow [SemVer](https://semver.org/).

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
