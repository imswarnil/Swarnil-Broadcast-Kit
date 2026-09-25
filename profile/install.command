#!/bin/bash
# Installs the Swarnil Broadcast Kit profile (1080p60, Apple hardware H.264,
# 6000 kbps) for the current user. OBS lists it under Profile once it restarts.
# Existing profiles are never touched; a profile of the same name is left alone.
cd "$(dirname "$0")"
set -e
NAME="Swarnil Broadcast Kit"
DEST="$HOME/Library/Application Support/obs-studio/basic/profiles/$NAME"
if [ -d "$DEST" ]; then
    echo "A profile called $NAME already exists at $DEST — leaving it."
    exit 0
fi
mkdir -p "$DEST"
sed "s|^FilePath=\$|FilePath=$HOME/Movies|" "$NAME/basic.ini" > "$DEST/basic.ini"
echo "Installed -> $DEST"
echo "In OBS: Profile -> $NAME, or Tools -> Broadcast Kit: use the Broadcast Kit profile."
