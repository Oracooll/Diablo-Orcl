# The open tesseract — v1.12.168

2026-09-22

> replace levski's cube with this new asset - "Levski Cube - open tesseract hologram grid
> 340x720.png"
> dont apply grid texture to levskis hologram grid. leave it as it is. as transmute button we will
> use the diamond who is 72 pixels bellow the cube grid. dont use this asset for the recipe tab.

A man facing the opened cube, the item grid floating above it as a violet hologram and the
transmute diamond glowing in the tesseract below.

## Measured, not estimated

**The hologram grid.** Its rules stand at x 123/124, 152, 180, 207/208 and y 103/104, 132, 160,
188, 215/216 — three columns by four rows of 28px cells on a 28px pitch, origin **(124, 104)**.

That is the Roar's own cell and the Roar's own pitch, so not one line of the transmute logic moves:
`CellRect`, `FitsAt`, `PlaceInGrid`, the hit test and the footprint drawing all carry over.

**The diamond.** Its vertices sit at x 143 and 190, y 288 and 322, so the button is the bounding box
**{ {143, 288}, {48, 35} }** — wider than it is tall because the shape is, and a square over it
would take in the tesseract's ribs on either side.

Its **top lands at exactly 216 + 72**, which is the user's "72 pixels bellow the cube grid" to the
pixel. Two independent measurements agreeing is what says the grid reading is right as well: the
grid's bottom rule and the diamond's apex were found by separate scans and the arithmetic between
them was the user's.

## Two switches on the page, not two special cases in the draw

**`slotArt`.** The item grids' slot texture goes in every cell of every grid in the game, because
those cells are painted wells. These are light. The page now carries `slotArt = false` and the draw
asks it, rather than the loop testing for this particular canvas — which is the shape that has
broken repeatedly in this codebase whenever a third thing appeared.

**`paintedTransmute`.** The tabbed pages wear Griswold's plate and glyph over their transmute rect,
which is right where the rect is a recess the art left empty for it. Here the button IS the painted
diamond, so a plate would cover the very thing being pressed.

It answers with **light** rather than with the mod's standing two-pixel sink: a sink moves a painted
object off the art it is part of, and the tesseract's ribs would show a diamond-shaped hole beside
it. 115% under the cursor, 140% while held.

That branch is checked **before** the tabbed-page branch, which this page also satisfies — the
tesseract is a tabbed page, and the plate is exactly what it must not get.

## What did not change

The **Recipes tab** keeps `ui\cube_recipes_canvas.png` and every number it had ("dont use this asset
for the recipe tab").

**Ogden's Cube page** keeps `CubePageTransmuteRect` — Griswold's 34px plate under his painted well.
The two pages shared that rect until today; Levski's now has its own, and Ogden's is untouched.

## Files

- `Packaging/resources/oracool_assets/ui/cube_page_canvas.png` (replaced)
- `Source/oracool/levski_roar.cpp` — the geometry, the two new page flags, the painted-button branch

Built clean, 832/832. `oracool.mpq` repacked.

## What to look at in play

1. The Cube tab: items should land in the hologram's cells, with no stone slot behind them.
2. A 2x3 item in it — the footprint should cover six hologram cells squarely.
3. The diamond: brighter under the cursor, brighter still while held, and it transmutes.
4. The Recipes tab: unchanged.
5. Ogden's Cube tab: still his well and still Griswold's plate under it.

## Known

The hover brighten uses the diamond's **bounding box**, so the four corners outside the rhombus —
tesseract ribs and glow — brighten with it. Diffuse enough there that it should read as the gem
lighting up, but if it reads as a box, that is why.
