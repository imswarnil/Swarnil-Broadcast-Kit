# Contributing

Everything here is welcome: a bug report, a scene you built, a source you think
is missing, a sentence in the docs that reads badly. A good report costs you ten
minutes and saves an afternoon, which makes it worth as much as a patch.

The kit is small on purpose: one C file per source, one shader for every box,
one header for the shared look. Keep it that way.

## Reporting something

Open an issue with:

- **What you did and what happened**, in that order.
- **Your OBS version and platform** — *Help → About* in OBS.
- **The newest log** from `~/Library/Application Support/obs-studio/logs/`. It
  usually contains the answer. `[sbk]` lines are ours.
- A screenshot if it is a visual problem. The overlays are drawn, so a picture
  of the wrong pixels is better than a description of them.

If the plugin did not load at all, say whether `[sbk] v… loaded` appears in that
log. Nothing else in the report matters until it does.

## Asking for a source

Say **what you would put on screen and why the existing ones cannot**. That is
the whole test. A source that differs from an existing one only in its defaults
is a preset, not a source, and the kit already has a Look group and a variant
list for exactly that.

## Sending code

### Adding a source

1. Copy the closest `src/source-*.c`. Give it a new `id` (`sbk_<thing>`), a
   locale key (`SBK.<Thing>` in `data/locale/*.ini`) and an `obs_source_info`.
2. Add the file to `CMakeLists.txt`, register the info in `src/plugin-main.c`
   and add its id to the self-test list there. CI fails the build if a source
   exists and is never registered.
3. Use the shared pieces: `sbk_look_*` for the Look group, `sbk_text*` for type,
   `sbk_stage` + `sbk_anim` if it animates in, `sbk_card`/`sbk_fill`/`sbk_dot`
   for boxes, `sbk_surface_*` if it has a variant list, `sbk_glyph_*` if it needs
   a mark. Every length is `u × n`.
4. Anything that reaches the network goes through `sbk-net.c`, never a blocking
   call on the graphics thread.
5. Describe it in `site/content.mjs` so the documentation site picks it up, and
   run `node scripts/skill-sync.mjs` so the agent skill learns its settings.
6. `./build.command`, add it in OBS, open its Properties and **move every
   slider**. Then run the self-test (`docs/INSTALL.md`) and look at the pictures.

### Before you open the pull request

```bash
make            # the list of every job in this repository
make install    # build and install the plugin — quit OBS first
make check      # exactly what CI runs, in the order it runs it
make selftest   # arm the clean self-test, then start OBS
make shots      # pull that run into docs/screens/
```

Then the self-test, and **look at the screenshots it produces**. Almost every
bug found in this repository was found by looking at a rendered frame, not by
reading the code that drew it.

Use `.sbk-selftest-clean` for anything you might publish — it hides the camera
and the display capture for the walk. A screenshot with a person or a desktop in
it is not going in a pull request.

### Style

Tabs, libobs-flavoured C, `snake_case`. Comments explain **why**, not what.
Names in the UI are sentences a streamer would say, not field names: "Lit only
when on air", not "dot_mode_live".

Commit messages say what changed and why in prose. If a fix was hard to find,
the message is where the symptom goes, so the next person recognises it.

## The two rules that are not negotiable

These are what keep the kit redistributable, and a pull request that breaks
either cannot be merged whatever else it does.

1. **No code from a commercial theme or design system.** Not markup, not class
   names, not variable names, not file structure. The kit speaks the visual
   language of the [Im Design System](https://design.imswarnil.com) — a neutral
   ramp, Geist, one accent, the recording-light dot — and shares no code with
   it. That system is all-rights-reserved and sold; this is MIT and public.
2. **No trademarked logos.** The marks in `data/effects/glyph.effect` are
   generic on purpose: a play triangle, a camera, an at-sign. A platform is
   identified by its colour and its name in type. This is not caution for its
   own sake — it is what lets anybody ship this inside something they charge
   for, and it does not go stale the week a company redraws its mark.

## Things that have already bitten

Read **"OBS facts the code relies on"** in `CLAUDE.md` before you spend an
afternoon on one of them again. Each is written down with the symptom that gave
it away: the wrapped-text measurement, the `GS_R8` row alignment that made a QR
code unscannable, the UI thread and queued screenshots, the scene-rebuild double
free, the scene-and-source namespace collision that makes an item silently never
appear, and a brand colour too dark to see on a dark overlay.

## Licence

By contributing you agree your work is released under the MIT licence in
`LICENSE`. A compiled plugin links libobs and is distributed under the GPL's
terms; see `deps/README.md`.
