#!/bin/bash
# Serves the remote on your own network so a phone can open it.
#
# Why this and not the website: a page loaded over https cannot open a plain
# ws:// connection — browsers block it — and obs-websocket only speaks ws://.
# So the remote has to come from an http:// address, which is what this is.
# Nothing leaves your network, and the script only serves this one folder.
cd "$(dirname "$0")"
PORT="${1:-8910}"
IP=$(ipconfig getifaddr en0 2>/dev/null || ipconfig getifaddr en1 2>/dev/null)

echo
echo "  SBK Remote"
echo "  ──────────"
if [ -n "$IP" ]; then
  echo "  On your phone, on the same wifi:   http://$IP:$PORT/"
  echo "  The address to type into it:       $IP"
else
  echo "  Could not work out this Mac's address — check System Settings → Network."
fi
echo "  On this Mac:                       http://localhost:$PORT/"
echo
echo "  In OBS first: Tools → WebSocket Server Settings → Enable, then Show Connect Info"
echo "  for the port and password."
echo
echo "  Ctrl-C to stop."
echo
exec python3 -m http.server "$PORT" --bind 0.0.0.0
