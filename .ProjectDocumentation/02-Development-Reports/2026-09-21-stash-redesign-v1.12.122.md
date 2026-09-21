---
version: v1.12.122
date: 2026-09-21
area: UI / stash
tests: 832/832
---

# The stash gets its storeroom, and its furniture moves off the grid frame

## The asks

Eight, and six went in as written. The two that did not are below, with why.

## Done as asked

1. **The canvas** - `ui\stash_canvas.png`, a storeroom with the 10x16 grid's frame painted in. Its own file rather
   than a recut of `panel_bg_grid.png`, which Adria, Pepin and Wirt also wear. Measured: the ornate band runs
   y 159..628, the same place the other framed canvases put theirs, so this is a painting swap and not a geometry
   change. Code bezel skipped, frame left untinted (the same 288x470 hole the inventory's canvas gets).
2. **The gold** is Griswold's pair now - the pile with a bare number beneath it, no "GOLD:" - under the grid,
   four pixels below the frame's foot.
4. **The nav row** sits four pixels above the frame: its last pixel is 154, so a 16px row starts at 139.
5. **The counter is centred** on the grid, in a rect its own measured width gives it.
6. **Six pixels** between the counter and the nearest button.
7. **The buttons are 16 tall**, width unchanged at 34.

### The nav rects became a function

`StashButtonRect[]` was a fixed table. "6px from the counter" cannot be a constant - "Page 1 / 2" and "Page 100 /
100" are different widths, and a table would honour the gap on one of them and nothing else. `StashNavButtonRectAt`
measures the string being drawn with `GetLineWidth` and places the four from it, so the gap is six whatever the
page number does. One helper builds the string for the rect, the buttons and the draw.

## 3. There is no 16px font in this engine

The ask was "reduce the page navigation font to font16". The tables are **8, 9, 10, 11, 12, 22
(FontSizeDialog), 24, 30, 42, 46** - there is no 16.

**FontSize12** is used: the nearest real face below the 24 it replaces. NOT 22, which is nearer in number and
unusable here - its ink sits outside the range the colour .trn tables remap, so the ColorGold this string is drawn
in would be a no-op on it. That was learned from a waypoint-list screenshot on 2026-08-30 and is why 24 was
picked over 22 in the first place.

Ask 7 was phrased **conditionally** - "if the font is 16px high then make the nav buttons also 16px high" - on a
font that does not exist. The explicit number was kept, so the buttons are 16; with a 12px glyph inside the legacy
box's 3px bevel the usable face is 10, so the arrows will sit tight. 18 would give the glyph its full height.
Flagged for the user's eye.

## 8. SORT could not join the row above the frame

The arithmetic: the grid is 280 wide. Four 34px buttons (136) + two 6px gaps (12) + a centred "Page 100 / 100"
already spend about 260 of it. Ten pixels a side is not a button; SORT needs about forty.

So it is **under the grid on the gold's line, flush right**. The gold took the left end, so the pairing with the
inventory's SORT-left/gold-right header survives, mirrored. Four pixels below the frame rather than above it.

If it must go above, something else gives: a shorter string ("100 / 100"), narrower buttons, or two buttons
instead of four.

## A patch that silently did nothing

The edit that removed the old fixed table also dropped the helper *definitions* it was meant to replace them with -
only the forward declaration landed. The compiler caught it. Worth recording: a multi-part perl patch reports the
substitutions it MADE, and says nothing about the block it never inserted.

## Build

Debug, clean. 832/832. `ui\stash_canvas.png` packed at 326178 bytes.

## Not verified

Not seen on screen. The two to look at: whether 12px arrows in 16px buttons read as cramped, and whether SORT
under the grid opposite the gold looks deliberate.
