# Contributing

Tally is small on purpose. Every overlay is one HTML file, every style is one CSS file, and
one registry describes the lot. Keep it that way.

## Adding an overlay

1. Copy the closest neighbour in `overlays/` to `overlays/<slug>/index.html`.
2. Describe it in `overlays/registry.mjs`: slug, name, kind, size, params, example.
3. If it belongs in the shipped scene collection, add it to `scenes/scenes.config.mjs` and
   run `npm run scenes`.
4. `npm run build && npm run check`. Open `http://localhost:4800/docs/<slug>/` and check the
   preview moves and the builder produces a URL that works.

## Adding a component or a token

- A component is one file in `src/components/`, imported from `src/components/index.css`,
  named `tally-<thing>` with `tally-<thing>__part` and `tally-<thing>--variant`.
- Every length is a multiple of `--tally-unit`; every colour is a token. If a value is not in
  `src/tokens/tokens.css`, add it there first.
- A modifier is a class on the component, never a page-wide scope.

## Pull requests

CI builds, checks and packs on every PR. Keep the check green: it guards the registry, the
params, the generated scenes, the namespace and the site's links.

## Licence

By contributing you agree your work is released under the MIT licence in `LICENSE`.
