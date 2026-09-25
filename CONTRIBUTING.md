# Contributing

The kit is small on purpose: one C file per source, one shader for every box,
one header for the shared look. Keep it that way.

## Adding a source

1. Copy the closest `src/source-*.c`. Give it a new `id` (`sbk_<thing>`), a
   locale key (`SBK.<Thing>` in `data/locale/*.ini`) and an `obs_source_info`.
2. Add the file to `CMakeLists.txt`, register the info in `src/plugin-main.c`
   and add its id to the self-test list there. CI fails the build if a source
   exists and is never registered.
3. Use the shared pieces: `sbk_look_*` for the Look group, `sbk_text*` for type,
   `sbk_stage` + `sbk_anim` if it animates in, `sbk_card`/`sbk_fill`/`sbk_dot`
   for boxes, `sbk_surface_*` if it has a variant list. Every length is `u × n`.
4. Anything that reaches the network goes through `sbk-net.c`, never a blocking
   call on the graphics thread.
5. Describe it in `site/content.mjs` so the documentation site picks it up.
6. `./build.command`, add it in OBS, open its Properties and move every slider.
   Then run the self-test (`docs/INSTALL.md`) and look at the screenshots.

## Style

Tabs, libobs-flavoured C, `snake_case`. Comments explain why, not what. Names in
the UI are sentences a streamer would say, not field names.

## Things that have already bitten

Read the "OBS facts the code relies on" section of `CLAUDE.md` before you spend
an afternoon on one of them again: the wrapped-text measurement, the R8 texture
row alignment, the UI thread and queued screenshots, and the scene-rebuild
double free are all written down there with the symptom that gave them away.

## Licence

By contributing you agree your work is released under the MIT licence in
`LICENSE`. A compiled plugin links libobs and is distributed under the GPL's
terms; see `deps/README.md`.
