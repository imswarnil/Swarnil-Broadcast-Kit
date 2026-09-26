# Agent skills

## `sbk-scenes`

Design OBS scenes out of Swarnil Broadcast Kit's sources and get back a
collection OBS can import.

```bash
mkdir -p ~/.claude/skills
cp -R sbk-scenes ~/.claude/skills/
```

Or put it in `.claude/skills/sbk-scenes/` inside a project, and it travels with
the checkout. Nothing to build; Node 18 or newer is the only requirement.

Then ask for the scene you want. It reads its own reference before placing
anything, and validates the result before writing it.

The generator runs on its own too:

```bash
node sbk-scenes/scripts/build-collection.mjs my-show.json --check
node sbk-scenes/scripts/build-collection.mjs my-show.json -o my-show.collection.json
```

`references/sources.md` and `scripts/registry.json` are **generated** from the
plugin's C by `scripts/skill-sync.mjs` — do not edit them by hand, and run
`node scripts/skill-sync.mjs` from the repository root after changing a source.

Full documentation: [`docs/SKILL.md`](../docs/SKILL.md).
