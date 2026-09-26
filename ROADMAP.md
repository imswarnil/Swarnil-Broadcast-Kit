# Roadmap

What is next, and what is deliberately not. Nothing here is a promise; it is the
order things would happen in if nobody asks for something else first.

Open an issue to argue with any of it. The fastest way to move something up this
list is to say what you would put on screen and why the existing sources cannot.

## Next

- **A universal bundle.** The release is Apple Silicon only, because the runner
  is arm64 and so are the OBS frameworks and the Homebrew jansson it links.
  Fixing it means sourcing universal dependencies and cross-compiling, not
  flipping a CMake variable. Until then, Intel means building from source.
- **Linux and Windows.** The C is portable and nothing in it is Mac-specific
  except the device pickers in `scenes.c` and the Qt shim in `CMakeLists.txt`.
  Neither has been tried. A working CI job for either would be a large,
  welcome contribution.
- **A scene-collection importer in the plugin.** The Tools menu can build the
  show; it cannot yet read one of the JSON specs the builder and the agent skill
  produce. Doing that would close the loop without a trip through OBS's import
  dialog.
- **Twitch and Kick in `SBK Counter`.** YouTube, Ghost and any JSON endpoint
  already work; these two are the same shape with different auth.

## Wanted, unscheduled

- **A source that reads OBS's own audio track routing**, so a meter can say
  which track a guest is actually going out on.
- **Per-scene Look overrides**, so a break screen can be a different accent
  without touching every source in it.
- **A second mark set** for people who want something other than the kit's
  neutral drawing. Generic, as ever — no logos.
- **Spec round-tripping**: read a collection back into a spec, so an arrangement
  made by dragging in OBS can be saved and shared.

## Not planned

- **A browser-source fallback.** The whole argument for this kit is that a page
  cannot know you are live, hear the program mix, see dropped frames, be a
  transition, or filter the picture. Shipping one would be shipping the thing it
  replaces.
- **Bundling anybody's logos.** The marks are generic on purpose. It is what
  keeps the kit MIT and free to give away, and it does not go stale the week a
  company redraws its mark.
- **A hosted service of any kind.** Nothing the plugin draws is fetched from
  the internet at render time, and that is a feature.
- **Re-using anything from a commercial theme or design system.** Not code, not
  markup, not class names, not file structure.

## How a change actually lands

```bash
make            # the list of everything below
make install    # build and install the plugin (quit OBS first)
make check      # exactly what CI runs
make selftest   # arm the clean run, then start OBS
make shots      # pull the run into docs/screens/
```

Then look at the pictures. That is not a formality — it is how nearly every bug
in this repository was found.
