---
version: v1.12.117
date: 2026-09-21
area: UI / stash
tests: 832/832
---

# A 2px shadow behind the stash's grid and frame

## The ask

> draw a 2px black shadow 2px down and 2px left behind the grid/frame combo in the stash.

## Two things decided the implementation

**It is drawn AFTER the canvas, not before.** "Behind" is where it looks like it comes from, not the order it is
drawn in: the frame is painted INTO the canvas now (v1.12.116) and the canvas is opaque, so a shadow laid down
first would be covered by the very thing casting it. This one falls on the stone beside the frame.

**Only the uncovered L is filled.** The shadow is the combo's rect offset (-2, +2), and most of that rect sits
UNDER the combo. Filling all of it would have blacked out the grid's interior. What is drawn is the part the
offset leaves showing: a 2 px strip down the left, and one along the bottom reaching back to meet it so the
corner is not a notch.

## The footprint is derived, not typed in

Measured on the canvas: the painted frame runs x 27..312, y 167..620 against a grid of 280x448 at (30,170) -
exactly `OrnateBorderWidth` (3) outside it on every side. So the combo is the grid rect grown by that constant,
and the shadow follows the grid if either ever moves.

## Scope

The stash only, as asked. Two things deliberately untouched:

* **The vendors' shelves** wear the same canvas and have no such shadow.
* **The fallback path** - when `panel_bg_grid.png` is absent, the old half-transparent `DrawDropShadow` at
  (-3, +3) still runs. The new shadow draws only where the painted frame is.

## Build

Debug, clean. 832/832.

## Not verified

Not seen on screen, and this is a shadow two pixels wide - exactly the kind of thing only a screenshot settles.
Worth checking that it reads as depth rather than as a dark line, and that its corner meets cleanly.
