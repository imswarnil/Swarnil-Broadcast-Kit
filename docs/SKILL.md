# The agent skill

`skills/sbk-scenes/` is an **Agent Skill**: a folder of instructions and
reference material that an AI coding agent loads when it recognises the task.
Installed, it lets Claude Code — or anything else that reads the same format —
design OBS scenes out of this kit and hand back a collection OBS can import.

It exists because the kit has twenty sources and about four hundred settings
between them. Nobody memorises that, and an agent asked to guess will invent a
key that sounds right. OBS silently ignores a setting it does not recognise, so
a guess does not fail loudly: it produces a scene that looks almost correct and
cannot be debugged by looking at it. The skill removes the guessing.

## Installing it

```bash
git clone https://github.com/imswarnil/Swarnil-Broadcast-Kit.git
mkdir -p ~/.claude/skills
cp -R Swarnil-Broadcast-Kit/skills/sbk-scenes ~/.claude/skills/
```

That is the whole of it. There is nothing to build and no dependency beyond a
Node that can run an ES module, which is any Node 18 or newer. To update it,
copy the folder again.

For a project rather than a machine, put it in `.claude/skills/sbk-scenes/`
inside the repository instead, and it travels with the checkout.

## What is in it

```
skills/sbk-scenes/
├── SKILL.md                    what the agent reads first
├── references/
│   ├── sources.md              GENERATED — every source, every setting key
│   ├── layout.md               canvas, anchors, aspects, the two placement rules
│   └── recipes.md              six worked scenes
├── scripts/
│   ├── registry.json           GENERATED — the same thing, for the validator
│   └── build-collection.mjs    spec in, OBS scene collection out
└── examples/
    └── teaching.json           a two-scene spec that builds
```

`SKILL.md` carries a `description` in its frontmatter that decides when an agent
loads it: designing a scene, a stream overlay, a starting-soon screen, a lower
third, a screen-share layout, and so on. Everything else it reads on demand.

## The generated half

**`references/sources.md` and `scripts/registry.json` are written by
`scripts/skill-sync.mjs`, which reads the plugin's C.** Ids come from each
`obs_source_info`, display names from `data/locale/en-US.ini`, keys and defaults
from each `*_defaults()`, allowed values from the property lists, ranges from the
sliders, and the shared Look and Motion groups from the headers that define them.

Written by hand, that list would be wrong within a release. A key renamed in a
shader has no reason to remind anybody to edit a markdown file. Generated, it
cannot drift — and CI fails if the checked-in copy is stale:

```bash
node scripts/skill-sync.mjs           # rewrite it
node scripts/skill-sync.mjs --check   # what CI runs
```

Anything the parser cannot resolve is printed as *set at runtime* rather than
guessed at. A registry that is incomplete is recoverable; one that is
confidently wrong sends somebody to OBS to find a source that will not take the
setting they were promised.

## The generator

`build-collection.mjs` turns a spec into a scene collection. It validates first:

- an unknown source id, with the nearest real ones offered
- an unknown setting key on a known source, likewise
- a string value outside a setting's enumerated list, with the list printed
- an item named after a scene — OBS keeps scenes and sources in **one**
  namespace, so such an item resolves to the scene, is asked to contain itself,
  and silently never appears
- an anchor that does not exist, and a position off the canvas

```bash
node skills/sbk-scenes/scripts/build-collection.mjs show.json --check
node skills/sbk-scenes/scripts/build-collection.mjs show.json -o show.collection.json
```

The output is a **strict subset** of what OBS itself writes — every field it
emits is one OBS emits, and the ones it leaves out (uuids, relative positions,
scale references) OBS fills in on import. That was checked by diffing against a
collection OBS had written, and again by loading a generated file in OBS and
reading the scene tree out of the log.

## The spec format

```json
{
  "name": "My show",
  "canvas": { "width": 1920, "height": 1080 },
  "look": { "accent": 4282329077, "scale": 1.0 },
  "scenes": [
    { "name": "Live", "items": [ … ] }
  ]
}
```

| field | |
| --- | --- |
| `look` | merged into every `sbk_*` source in the collection, so one accent covers the set |
| `items[].id` | a source id, or `camera`, `screen`, or `external` with a `source_id` |
| `items[].name` | optional; must not match a scene name |
| `items[].x` `y` `anchor` | where it goes and what the coordinates mean |
| `items[].box` | `[w, h]`, crops rather than squashes — use it for every camera and capture |
| `items[].settings` | the source's own keys |
| `items[].visible` | `false` to import it switched off |

`references/layout.md` has the anchors, the aspect table, the plate-centring
rule and the colour conversion. `references/recipes.md` has six scenes worth
copying.

## What it cannot do

- **Choose a camera or a display.** The device list only exists inside a running
  OBS, so those items arrive with nothing selected and the person picks once in
  Properties. When they would rather not, the plugin's own **Tools → Broadcast
  Kit: create the scene collection** builds the full twenty-two-scene show with
  the camera and microphone already wired in — that is the better answer more
  often than not.
- **Add a transition.** OBS's frontend API can select one but not add one. Only
  the Scene Transitions panel's **+** can.
- **See the result.** It checks numbers against the canvas; it has not looked at
  a rendered frame.

## Keeping it honest

Two checks run in CI beside the site's:

```bash
node scripts/skill-sync.mjs --check   # the registry matches the C
node scripts/check-recipes.mjs        # every documented example still builds
```

The second matters more than it sounds. A worked example that no longer
validates is worse than none at all: somebody copies it, OBS drops the key it
got wrong, and the scene quietly is not what the page promised. Every JSON block
in `SKILL.md` and `recipes.md`, and every file in `examples/`, goes through the
same validator the skill tells an agent to use.

This already earned its keep. The first run of `check-recipes.mjs` failed on a
`variant` of `"glass"` in two recipes — a value no surface has ever accepted.
The same wrong value turned out to be in four places in the plugin's own
`scenes.c` and twice in the web builder, silently falling back to `card`
everywhere. Nothing looked broken, which is exactly why it had survived.
