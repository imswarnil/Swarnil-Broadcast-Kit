# Placing things on the canvas

## The canvas

1920 × 1080. Every number in a spec is in those units, whatever the person
actually streams at — OBS scales the whole canvas, so a scene built at 1080p is
correct at 1440p and at 720p.

**Keep a 120 px margin.** Nothing that has to be read goes closer than that to an
edge. Broadcast calls it the title-safe area and every television has had one for
seventy years; a phone's rounded corners and a platform's own chrome are the
modern version of the same problem.

## Anchors

`anchor` decides what `x` and `y` mean. It is the point on the item that lands on
the coordinate.

| anchor | `x`, `y` is the item's… |
| --- | --- |
| `top-left` *(default)* | top-left corner |
| `top-centre` | middle of its top edge |
| `top-right` | top-right corner |
| `left` / `centre` / `right` | middle of its left edge / its middle / its right edge |
| `bottom-left` | bottom-left corner |
| `bottom-centre` | middle of its bottom edge |
| `bottom-right` | bottom-right corner |

Anchor to the edge an item belongs to. A chip in the top-right corner is
`{"x": 1800, "y": 120, "anchor": "top-right"}`, and it stays in the corner when
its text gets longer. The same chip at `top-left` with computed coordinates
drifts the moment anybody edits the label.

## The aspect table

`sbk_frame` and `sbk_plate` share one list of shapes. At `size: 1.0`:

| aspect | box |
| --- | --- |
| `16x9` | 640 × 360 |
| `9x16` | 360 × 640 |
| `1x1` | 480 × 480 |
| `4x5` | 432 × 540 |
| `4x3` | 560 × 420 |
| `21x9` | 756 × 324 |
| `custom` | whatever `width` and `height` say |

At `size: k` the box is `w × k` by `h × k`, rounded down. **Those are the numbers
a camera's `box` must use**, or the frame will not sit on the picture.

```json
{ "id": "camera",    "x": 1800, "y": 560, "anchor": "top-right", "box": [640, 360] },
{ "id": "sbk_frame", "x": 1800, "y": 560, "anchor": "top-right",
  "settings": { "aspect": "16x9", "size": 1.0, "style": "ring" } }
```

Same coordinates, same anchor, and the box equals the frame's shape. Change
`size` and you must change `box` with it.

## A plate goes behind, centred

`sbk_plate` reports itself **bigger than its shape** — the shadow needs room to
fall outside it without being clipped. So anchoring a plate by a corner puts its
*shape* a shadow's width in from where you meant, and the drop shadow ends up
beside the thing it belongs to rather than under it.

The padding is symmetric, so the shape's centre is the source's centre. **Place a
plate with `anchor: "centre"`, on the centre of the box it sits behind.**

For a frame at `(x, y)` with anchor `a` and shape `w × h`, the centre is:

```
cx = a has left  ? x + w/2 : a has right  ? x - w/2 : x
cy = a has top   ? y + h/2 : a has bottom ? y - h/2 : y
```

So a 16:9 camera at `size: 0.66` (422 × 238) anchored `bottom-right` at
`(1800, 960)` has its centre at `(1589, 841)`, and the plate goes:

```json
{ "id": "sbk_plate", "x": 1589, "y": 841, "anchor": "centre",
  "settings": { "aspect": "16x9", "size": 0.66, "radius": 16.0, "fill_glass": true } }
```

Order matters too: the plate must come **before** the camera in the items list,
because OBS draws a scene's items in order and the first one is at the back.

`fill_glass: true` gives the plate the Look's glass colour, so it reads as a card
the camera sits on and is still there when the feed drops. Without it the plate
is a pure shadow, which is invisible on a black background and correct over a
backdrop.

## Drawing order

Items are drawn in the order they appear. Back to front:

1. the backdrop
2. plates
3. cameras and captures
4. frames
5. everything else — chips, lower thirds, meters, comments
6. the tally light, last, so nothing covers it

## Sizes that work

These are the kit's own, from a show that has been looked at on a television.

| | |
| --- | --- |
| Margin from any edge | 120 |
| A corner camera | `16x9` at `0.52`–`0.66` |
| A main camera | `16x9` at `1.0`, or `1.31` for a two-up |
| A full-frame camera | box `1664 × 936`, centred at the top with a 120 margin |
| A vertical camera | `9x16` at `1.22` (439 × 781) |
| A card's width | 900–1500, `align: "centre"` for a title screen |
| A ticker | `width: 1920`, anchored `bottom-left` at `(0, 1080)` |
| A visualizer along the foot | `height: 240`, anchored `bottom-left` at `(0, 1080)` |
| A comments panel | `width: 560`, `show_count: 3` |

## Colours

Every colour setting is an **ABGR integer**, which is how OBS stores them — not a
hex string, and not RGB. To convert `#RRGGBB` to what goes in a spec:

```
value = 0xFF000000 + (BB << 16) + (GG << 8) + RR
```

The kit's accent `#f5273f` is `0xFF3F27F5`, which is `4282329077` in decimal.
Write the decimal; JSON has no hex.
