---
name: sbk-scenes
description: Build OBS Studio scenes and scene collections with Swarnil Broadcast Kit — its native sources (sbk_*), their settings, the 1920×1080 layout rules, and a generator that writes a collection OBS can import. Use when asked to design, lay out or generate an OBS scene, a scene collection, a stream overlay, a starting-soon or be-right-back screen, a talking-head / screen-share / interview / podcast layout, a lower third, a countdown, a chat or comments panel, or anything else that goes on a stream with this kit installed.
license: MIT — see LICENSE in the Swarnil Broadcast Kit repository.
metadata:
  author: imswarnil
  version: "0.7.0"
  homepage: https://obs.imswarnil.com
  repository: https://github.com/imswarnil/Swarnil-Broadcast-Kit
---

# Swarnil Broadcast Kit — scenes

Design a scene, or a whole show, out of the kit's native OBS sources, and hand
back a file OBS can import.

The kit is a **native plugin**, not a browser overlay. Twenty `sbk_*` sources,
six filters and two transitions, drawn by OBS itself. Nothing you place here
fetches anything at render time.

## Before you place anything

1. **Read `references/sources.md`.** It is generated from the plugin's C, so it
   is the only honest list of what exists. Every source, every setting key, its
   type, its default, and the exact strings an enumerated setting will accept.
   **Do not invent a key.** OBS silently ignores a setting it does not know, so
   a typo produces a scene that looks almost right and nobody can say why.
2. **Read `references/layout.md`** for the canvas, the anchors, the aspect table
   and the two placement rules that are easy to get wrong.
3. If the person is composing something unusual, skim `references/recipes.md` —
   it has worked specs for the common shapes.

## How to build one

Write a spec as JSON, then run the generator:

```bash
node scripts/build-collection.mjs my-show.json --check   # validate only
node scripts/build-collection.mjs my-show.json -o my-show.collection.json
```

`--check` validates ids, keys and enumerated values against the registry and
prints the near misses for anything it does not recognise. **Always run
`--check` before writing the file**, and fix what it reports rather than
shipping a spec that half works.

Then tell the person: **OBS → Scene Collection → Import**, pick the file, then
switch to it from the same menu. Importing does not disturb the collection they
are on; it adds a new one beside it.

## The spec

```json
{
  "name": "My show",
  "canvas": { "width": 1920, "height": 1080 },
  "look": { "accent": 4282329077, "scale": 1.0 },
  "scenes": [
    {
      "name": "Live",
      "items": [
        { "id": "sbk_backdrop", "name": "Backdrop", "x": 0, "y": 0,
          "settings": { "mode": "grid", "drift": 6.0 } },
        { "id": "camera", "x": 1800, "y": 560, "anchor": "top-right", "box": [640, 360] },
        { "id": "sbk_frame", "name": "Cam frame", "x": 1800, "y": 560, "anchor": "top-right",
          "settings": { "aspect": "16x9", "size": 1.0, "style": "ring" } }
      ]
    }
  ]
}
```

- **`look`** is merged into every `sbk_*` source in the collection. Put the
  accent, the scale and the font here, once. Giving every source the same Look
  is what makes a set of overlays read as one design rather than as a pile of
  widgets.
- **`id`** is a source id from the registry, or one of three stand-ins for a real
  device: `camera` (macOS AV capture), `screen` (ScreenCaptureKit display
  capture), or `external` with a `source_id` of your own.
- **`name`** is optional. Left out, items get numbered names. Give a name when
  two items of the same kind need to be told apart. **A name must never match a
  scene name** — OBS keeps scenes and sources in one namespace, and an item named
  after a scene resolves to the scene and silently never appears. The validator
  catches this.
- **`x` / `y` / `anchor`** place the item. The anchor decides what the
  coordinates mean: `top-left` (the default), `top-right`, `bottom-left`,
  `bottom-right`, `top-centre`, `centre`, and so on.
- **`box: [w, h]`** crops a source to that rectangle instead of squashing it.
  Use it for every camera and every screen capture — their shape never matches
  the hole you are putting them in.
- **`settings`** are the source's own keys from `references/sources.md`.

## Rules that decide whether it looks designed

- **One Look, everywhere.** Never set a different accent per source unless the
  person asked for one. The `look` block exists so you do not have to.
- **A plate goes behind, centred.** See `references/layout.md`. Getting this
  wrong puts a drop shadow a shadow's width away from the thing it belongs to,
  which is worse than no shadow at all.
- **Match the frame to the camera's box.** A `sbk_frame` at `aspect: 16x9,
  size: 1.0` is 640 × 360, so the camera's `box` must be `[640, 360]` and both
  must share an anchor and a position. The aspect table in the reference gives
  the base size for every shape; multiply by `size`.
- **Do not fill the canvas.** Keep a 120 px margin from every edge for anything
  that must be readable. The kit's own show uses exactly that.
- **Two aspect ratios beat one.** A screen and a face want different shapes. A
  21:9 crop gives code room for long lines; a 9:16 or 1:1 column suits a face.
  Matching them wastes half the picture on a desk.
- **Pick the fewest pieces that say it.** Four well-placed sources read better
  than nine. If a scene needs a legend, it has too much in it.

## What this cannot do

- **It cannot choose a camera or a display for you.** A `camera` or `screen`
  item arrives with no device selected, because the device list only exists
  inside a running OBS. The person picks it once in Properties. If they would
  rather not, the plugin's own **Tools → Broadcast Kit: create the scene
  collection** builds a twenty-two-scene show with the camera and microphone
  already wired in — say so when it is the better answer.
- **It cannot add a transition.** OBS's frontend API can select one but not add
  one; only the Scene Transitions panel's **+** can. Tell the person to add
  **SBK Wipe** or **SBK Logo Sting** there.
- **It cannot preview.** Check the numbers against the canvas, and say plainly
  that you have not seen it rendered.

## If the plugin is not installed

Everything here assumes `sbk.plugin` is in
`~/Library/Application Support/obs-studio/plugins/`. Without it OBS imports the
collection and shows every `sbk_*` item as missing. Point the person at
<https://github.com/imswarnil/Swarnil-Broadcast-Kit/releases>, or at
`docs/INSTALL.md` in the repository.
