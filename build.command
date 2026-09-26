#!/bin/bash
# Builds Swarnil Broadcast Kit and installs it for the current user: the plugin
# bundle, the Geist fonts it sets type in, and the profile if there is none yet.
# Quit OBS first — a loaded plugin cannot be replaced underneath it.
cd "$(dirname "$0")"
set -e

if pgrep -x OBS > /dev/null; then
    echo "Quit OBS first, then run this again."
    exit 1
fi

cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo > /dev/null
cmake --build build

DEST="$HOME/Library/Application Support/obs-studio/plugins"
mkdir -p "$DEST"
rm -rf "$DEST/sbk.plugin"
cp -R build/sbk.plugin "$DEST/"
echo "Installed -> $DEST/sbk.plugin"

# the fonts: only the ones not already there
FONTS="$HOME/Library/Fonts"
mkdir -p "$FONTS"
added=0
for f in fonts/*.ttf; do
    if [ ! -e "$FONTS/$(basename "$f")" ] && [ ! -e "/Library/Fonts/$(basename "$f")" ]; then
        cp "$f" "$FONTS/"; added=$((added + 1))
    fi
done
[ "$added" -gt 0 ] && echo "Installed $added font file(s) -> $FONTS"

./profile/install.command

echo
echo "Open OBS, then: Sources -> + -> SBK …   or   Tools -> Broadcast Kit: create the scene collection"
