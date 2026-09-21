---
version: v1.12.116
date: 2026-09-21
area: UI / canvases
tests: 832/832
---

# The 10x16 grid windows get a canvas with the frame painted in

## The ask

> take this canvas "Updated Canvas 340x720 for Grid Application.png" and apply it to all windows which use the
> 10x16 grid. It has new grid frame embedded in it.

## It is a precise fit, and that was measured

The canvas's dark opening runs **x 29..310, y 169..618** - the grid's 280x448 at (30,170) with exactly one pixel
of black around it - and the gold bezel line sits immediately outside, at x=28 (`#DDC47E`) and y=168 (`#CCB775`).
It was drawn against the real geometry.

Worth recording how that was found: the first measurement took the first and last dark pixel on the centre row
and column, which swallowed the whole panel, because the OUTER bezel has near-black lines of its own. The answer
is the longest contiguous dark run, not the extremes.

## Which windows, and the one that was asked about

Exactly two windows use the 10x16 grid: the shop shelves and the stash. (The inventory's is 10x7 and sits
lower.) But Griswold's shelves wear his own forge painting as of v1.12.109, so the literal reading of the ask
would have replaced it.

**Asked rather than guessed**, because the cost of being wrong ran in both directions - either undoing a redesign
finished hours earlier, or not doing what was asked. The user's answer: **keep his forge.** So:

| Window | Canvas |
| --- | --- |
| Stash | the new grid canvas |
| Adria, Pepin, Wirt - grid tabs | the new grid canvas |
| Griswold - grid tabs | his forge, unchanged |
| Griswold - Salvage page | his forge, unchanged (no 10x16 grid anyway) |

## A SECOND file, not a replacement

It ships as `ui\panel_bg_grid.png` beside `ui\panel_bg.png` rather than overwriting it. The shared canvas is worn
by **nine** windows and six of them have no 10x16 grid - the character sheet, the spell book, the quest log, the
waypoint list, the inventory and the artisan pages would all have carried a gold frame drawn around an opening
with nothing in it.

Loaded through the by-path PNG cache, so it needs no registration in hud_art's declared-asset list, and it takes
the same `DrawSidePanelDim` pass every other canvas wears.

## The code bezel steps aside

Where the new canvas is up, `DrawGridBezel` / `DrawOrnateBorderOutside` are skipped - drawing one would put a
second frame inside the painted one. Griswold still draws his, because his painting has no frame in it. Both
windows fall back to the old canvas and the old bezel when the file is absent.

**The fill and the cell rules are still code.** The canvas's opening is pure black, and letting that show would
have made the dark 1px cell rules invisible against it. So the art brings the FRAME and nothing else, and the
grid's interior reads the same on this canvas as on a painting. Flagged to the user as the one reversible call
here.

## Build

Debug, clean. 832/832. `ui\panel_bg_grid.png` packed at 126311 bytes.

## Not verified

Not seen on screen. What to look at: that the painted frame lands exactly on the grid with no doubled or missing
line at its edges, and whether the cell rules read well against the canvas's opening.
