# The stash's cast shadow comes off — v1.12.124

**Date:** 2026-09-21
**Version:** v1.12.124
**Branch:** renderer-32bit

## The request

> is there a shadow added to stash grid? if so - remove it. it is overlapping the frame.

Yes, there was one, added two builds earlier on the user's own instruction
([[2026-09-21-stash-redesign-v1.12.122]]): *"draw a 2px black shadow 2px down and 2px left behind
the grid/frame combo in the stash."* Removed.

## Why it overlapped — it was measured against a frame that isn't there

The shadow was placed from the **procedural** bezel's footprint, not from the **painted** one:

```cpp
const Rectangle combo { grid.position - Displacement { oracool::OrnateBorderWidth, oracool::OrnateBorderWidth }, ... };
```

`OrnateBorderWidth` is 3, so with the grid at (30,170) 280×448 the combo came out at **x 27..312,
y 167..620**. The frame the user is actually looking at is painted into `ui\stash_canvas.png` and
runs **x 26..313, y 159..628** — measured off the file when the canvas was applied, and the number
the rest of this window's furniture is placed from.

So the bottom strip landed at y 620..622, which is nine pixels *inside* the painted band rather than
below it: a black rule drawn across the frame's own bottom moulding. The left strip at x 25..26 sat
on the band's outer edge for the same reason.

The two rects were never the same thing. `OrnateBorderWidth` describes the bezel this window draws
when there is **no** canvas; the canvas's band is 6px taller at the top and 8 at the foot. The code
asked the fallback where the frame was and then drew on the painting.

## Not re-measured, removed

The obvious repair is to expand the combo to the painted band instead. It was not done, because
there is nowhere for the shadow to fall: the band already reaches x 26 in a 340-wide panel, so two
pixels to its left is x 24 — inside the panel's own edge bezel, with seven pixels of stone between
them. A cast shadow needs a surface to land on and this one has none. The canvas paints its own
relief.

The user asked for removal, not relocation, and the geometry agrees with them.

## What remains

`DrawDropShadow(out, gridRect, GridBezelInset)` is still in `DrawStash`, and is *not* this shadow —
it is the per-slot recess shadow from 2026-09-05, and it lives inside the
`if (!HasStashCanvasArt() && !HasSidePanelGridArt())` fallback block, so it does not run while a
canvas is up. Left alone deliberately: the spell book carries a comment from a misread "remove
shadows" that cost that shadow and had to be put back.

## Files

- `Source/qol/stash.cpp` — the `FillRectRgb` shadow L deleted from `DrawStash`.

No asset changes, so no MPQ repack. Built clean.
