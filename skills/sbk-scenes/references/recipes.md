# Worked scenes

Six shapes that cover most of what anybody asks for. Copy one, change the words,
check it, ship it. Every coordinate here obeys the 120 px margin and the plate
rule in `layout.md`.

Colours are ABGR decimals. The accent is `4282329077`.

## A starting-soon screen

The one people sit on for ten minutes, so it has to survive being stared at.
A drifting ground, something to read, and a clock that proves the stream is live.

```json
{
  "name": "Starting soon",
  "items": [
    { "id": "sbk_backdrop", "name": "Backdrop", "x": 0, "y": 0,
      "settings": { "mode": "grid", "drift": 6.0, "pitch": 72.0 } },
    { "id": "sbk_visualizer", "name": "Spectrum", "x": 0, "y": 1080, "anchor": "bottom-left",
      "settings": { "style": "bars", "height": 240 } },
    { "id": "sbk_card", "name": "Title card", "x": 120, "y": 300,
      "settings": { "eyebrow": "Starting soon", "title": "Building a Salesforce app live",
                    "body": "Grab a coffee. We begin at the top of the hour.",
                    "variant": "card", "align": "left", "width": 1100 } },
    { "id": "sbk_countdown", "name": "Countdown", "x": 1800, "y": 300, "anchor": "top-right",
      "settings": { "mode": "duration", "minutes": 15, "style": "ring",
                    "ring_size": 300, "label": "We begin in" } },
    { "id": "sbk_social", "name": "Handles", "x": 120, "y": 820, "anchor": "bottom-left",
      "settings": { "mode": "stack", "variant": "card", "brand": true } },
    { "id": "sbk_clock", "name": "Clock", "x": 1800, "y": 120, "anchor": "top-right" },
    { "id": "sbk_onair", "name": "Light", "x": 120, "y": 120 }
  ]
}
```

## A talking head

The camera is the picture, so the chrome shrinks to almost nothing. A full-frame
camera is a `box` and no frame shape — the brackets are a `corner` frame sized to
the same box.

```json
{
  "name": "Talking head",
  "items": [
    { "id": "camera", "x": 960, "y": 120, "anchor": "top-centre", "box": [1664, 936] },
    { "id": "sbk_frame", "name": "Cam frame", "x": 960, "y": 120, "anchor": "top-centre",
      "settings": { "aspect": "16x9", "size": 2.6, "style": "corner",
                    "bracket": 96.0, "line": "accent", "weight": 4.0, "label": "" } },
    { "id": "sbk_logo", "name": "Bug", "x": 120, "y": 100,
      "settings": { "mark": "ring", "loop": "orbit", "mark_size": 52, "variant": "none" } },
    { "id": "sbk_lower_third", "name": "Name", "x": 160, "y": 980, "anchor": "bottom-left",
      "settings": { "variant": "minimal", "bar": true } },
    { "id": "sbk_onair", "name": "Light", "x": 1800, "y": 120, "anchor": "top-right",
      "settings": { "shape": "dot" } }
  ]
}
```

## A screen share with the camera in a corner

The plate is centred on the camera's centre, and comes first so it is behind.
Camera at 16:9 × 0.66 is 422 × 238, anchored bottom-right at `(1800, 960)`, so
its centre is `(1800 − 211, 960 − 119)` = `(1589, 841)`.

```json
{
  "name": "Screen share",
  "items": [
    { "id": "screen", "x": 0, "y": 0, "box": [1920, 1080] },
    { "id": "sbk_plate", "name": "Cam plate", "x": 1589, "y": 841, "anchor": "centre",
      "settings": { "aspect": "16x9", "size": 0.66, "radius": 16.0, "fill_glass": true } },
    { "id": "camera", "x": 1800, "y": 960, "anchor": "bottom-right", "box": [422, 238] },
    { "id": "sbk_frame", "name": "Cam frame", "x": 1800, "y": 960, "anchor": "bottom-right",
      "settings": { "aspect": "16x9", "size": 0.66, "style": "ring", "radius": 16.0,
                    "label": "@imswarnil", "chip_at": "bottom-left" } },
    { "id": "sbk_chip", "name": "Topic", "x": 120, "y": 120,
      "settings": { "label": "Chapter 1 — setting up", "variant": "card", "dot": "accent" } },
    { "id": "sbk_meter", "name": "Mic", "x": 120, "y": 960, "anchor": "bottom-left",
      "settings": { "source": "@mic", "label": "Mic", "width": 300,
                    "style": "segments", "segment_count": 20, "show_db": false } },
    { "id": "sbk_onair", "name": "Light", "x": 1800, "y": 120, "anchor": "top-right",
      "settings": { "shape": "badge" } }
  ]
}
```

## Two people over a shared screen

Both cameras on one edge so the screen keeps the middle, and a meter under each,
because the thing you cannot tell from a picture is who is talking.

```json
{
  "name": "Screen share + two",
  "items": [
    { "id": "screen", "x": 0, "y": 0, "box": [1920, 1080] },
    { "id": "sbk_plate", "name": "Host plate", "x": 1633, "y": 213, "anchor": "centre",
      "settings": { "aspect": "16x9", "size": 0.52, "radius": 14.0, "fill_glass": true } },
    { "id": "camera", "x": 1800, "y": 120, "anchor": "top-right", "box": [332, 187] },
    { "id": "sbk_frame", "name": "Host frame", "x": 1800, "y": 120, "anchor": "top-right",
      "settings": { "aspect": "16x9", "size": 0.52, "style": "ring", "radius": 14.0, "label": "Host" } },
    { "id": "sbk_plate", "name": "Guest plate", "x": 1633, "y": 443, "anchor": "centre",
      "settings": { "aspect": "16x9", "size": 0.52, "radius": 14.0, "fill_glass": true } },
    { "id": "external", "name": "Guest", "source_id": "window_capture",
      "x": 1800, "y": 350, "anchor": "top-right", "box": [332, 187] },
    { "id": "sbk_frame", "name": "Guest frame", "x": 1800, "y": 350, "anchor": "top-right",
      "settings": { "aspect": "16x9", "size": 0.52, "style": "ring", "radius": 14.0,
                    "label": "Guest", "line": "accent" } },
    { "id": "sbk_meter", "name": "Host level", "x": 1800, "y": 580, "anchor": "top-right",
      "settings": { "source": "@mic", "label": "Host", "width": 292, "show_db": false } },
    { "id": "sbk_meter", "name": "Guest level", "x": 1800, "y": 690, "anchor": "top-right",
      "settings": { "source": "@mic2", "label": "Guest", "width": 292, "show_db": false } },
    { "id": "sbk_ticker", "name": "Ticker", "x": 0, "y": 1080, "anchor": "bottom-left",
      "settings": { "width": 1920 } }
  ]
}
```

The guest is an `external` item pointing at whatever carries them — a window
capture of the call, a capture card, an NDI source. Change `source_id` to match.

## A questions panel

For teaching, when the chat gets ahead of you and you want the question on screen
while you answer it.

```json
{
  "name": "Questions",
  "items": [
    { "id": "screen", "x": 0, "y": 0, "box": [1920, 1080] },
    { "id": "sbk_chip", "name": "Q chip", "x": 1800, "y": 120, "anchor": "top-right",
      "settings": { "label": "Questions", "variant": "accent", "dot": "none" } },
    { "id": "sbk_comments", "name": "Questions panel", "x": 1800, "y": 240, "anchor": "top-right",
      "settings": { "width": 560, "title": "Questions", "show_count": 3,
                    "rotate": 12.0, "variant": "card", "enter": "left" } },
    { "id": "sbk_onair", "name": "Light", "x": 120, "y": 120, "settings": { "shape": "badge" } }
  ]
}
```

Set `from: "youtube"` on the panel and give it a `video` id and a `key` to read a
real live chat. A key typed into a setting is stored in the collection **in plain
text** — tell the person they can write `@/Users/them/.youtube-key` instead and
the plugin reads the file.

## An ending screen

The ask, with the thing it is asking for, and something to scan.

```json
{
  "name": "Ending",
  "items": [
    { "id": "sbk_backdrop", "name": "Backdrop", "x": 0, "y": 0,
      "settings": { "mode": "stripes", "drift": 10.0, "pitch": 96.0 } },
    { "id": "sbk_visualizer", "name": "Spectrum", "x": 0, "y": 1080, "anchor": "bottom-left",
      "settings": { "style": "dots", "height": 240 } },
    { "id": "sbk_card", "name": "Sign off", "x": 960, "y": 320, "anchor": "top-centre",
      "settings": { "eyebrow": "That is a wrap", "title": "Thanks for watching",
                    "body": "Subscribe for the next one.", "variant": "none",
                    "align": "centre", "width": 1400 } },
    { "id": "sbk_progress", "name": "Goal", "x": 960, "y": 620, "anchor": "top-centre",
      "settings": { "label": "Subscriber goal", "value": 640, "target": 1000, "width": 720 } },
    { "id": "sbk_social", "name": "Handles", "x": 960, "y": 900, "anchor": "top-centre",
      "settings": { "mode": "bar", "variant": "card", "brand": true } },
    { "id": "sbk_logo", "name": "Mark", "x": 120, "y": 960, "anchor": "bottom-left",
      "settings": { "mark": "ring", "loop": "draw", "mark_size": 120,
                    "variant": "none", "caption": "See you Thursday" } },
    { "id": "sbk_qr", "name": "Channel QR", "x": 1800, "y": 960, "anchor": "bottom-right",
      "settings": { "text": "https://youtube.com/@imswarnil", "caption": "Scan to subscribe",
                    "code_size": 240, "variant": "none", "invert": true } }
  ]
}
```

## Things worth saying when you hand one over

- Import does not disturb the collection they are on. It adds one beside it.
- A `camera` or `screen` item has no device chosen. One visit to Properties each.
- **SBK Wipe** and **SBK Logo Sting** are added from the Scene Transitions
  panel's **+**, not from Sources, and not by this file.
- If they want the whole twenty-two-scene show with devices already wired in,
  that is **Tools → Broadcast Kit: create the scene collection**, not this.
