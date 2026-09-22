# One slot texture, every item grid — v1.12.163

2026-09-22

> apply this texture to all 28x28px inv/stash grids game-wide. also apply it to all vendors grids
> which might be a big bigger like odgen and gillian's grids in their screens

then, after the first build:

> do the belt too and also if lecski is 29x29 scale it to 29x29

## What the art is

A single 28x28 slot with its own bevel — light along the top and left, dark along the bottom and
right, fully opaque, no alpha. **Not a seamless tile.** So it is drawn once per CELL rather than
tiled across a grid: tiling it would repeat the highlight through the middle of every cell.

Installed as `ui\slot_background.png`, drawn by one function, `oracool::DrawSlotBackground(out,
cell)` — 1:1 where the cell is the art's own size, scaled where it is not.

## Where it went

| Grid | Cell | Path |
|---|---|---|
| Backpack and its nine extra tabs | 28x28 | 1:1 |
| Stash, all sixteen rows | 28x28 | 1:1 |
| The four vendor grids | 28x28 | 1:1 |
| Ogden's craft well, Gillian's | 28x28 | 1:1 |
| The belt's four item slots | 28x28 | 1:1, centred in the plate's recess |
| Levski's Cube | 28x28 | 1:1 — see below |
| Ogden's gem and rune boards | 30x30 | scaled |
| Ogden's jewel board | 42x50 | scaled |

**Every cell, occupied or not.** The backings that were already there are drawn per ITEM, not per
cell, so they could never be the thing that gives an empty slot its face.

**Ogden's boards** are the only grids in the game that are not 28px. The art is scaled to whichever
cell it is handed rather than tiled, for the reason above.

**The belt** goes in the plate's recess rather than over the plate: the plate is that row's frame,
and its recess is 28x28 by construction — "sized so the backing's recess comes out at 28x28 - the
sprite's own size", as the nudge note beside it already recorded. Slots 0 and 5 are the menu button
and the town portal, not slots.

## Levski is not 29

The user asked for 29x29 if that is what the Cube's cells are. They are not, and the skins say so:
`levski_skin::GridPitch` and `CellSize` are both **28**, and `cube_skin`'s own measurement note
reads *"brass rims at x 25-26 / 53-54, interiors of 26 px on a 28 px pitch - the Roar's grid
exactly"*. The 29 is a stale line in `CellRect`'s comment, left from a painting the window has not
worn since 2026-09-05.

It first went in the **well**, one pixel inside the cell at 26x26, to leave the painted brass rims
showing — painting over moulding being the one thing every canvas this week has been laid out to
avoid. That was caution rather than a request, it was offered as a one-line change, and the user
asked for the cover: *"i would like that."*

So the art fills the whole 28 cell, rims included — the same square the item sprite occupies.

## A stack overflow found on the way

`BlitLayerScaled` — the existing scaled blit, and the one the non-28px boards need — called
**itself** in its fallback branch:

```cpp
if (!out.isIndexed() && !asset.argb.empty()) { BlitArgbScaled(...); return; }
BlitLayerScaled(out, asset, srcCell, dest, halfTransparent);   // <- itself
```

Infinite recursion the moment a scaled draw reached an indexed target or an asset with no
true-colour layer. Nothing had ever triggered it because the 32-bit path always returned first;
Ogden's boards are the first caller that could plausibly land there, since the golden tests draw
into an indexed surface.

The fallback returns now. Nothing drawn is the honest answer: there is no indexed scaler to fall
back to, and the sibling `BlitLayer`'s fallback is an unscaled blit, which would put a 28px cell in
a 42px hole.

## The backings take the same texture

> make backings of item sprites use the same texture under the tinting layer.

`InvDrawSlotBack` laid `DrawSlotStoneUnderlay` under the tint — vanilla's stone, cut into four
swatches and eight orientations and hashed across the footprint so the grain never repeats. That
was the right answer while the grids had no art of their own: the backing had to invent a surface
for the tint to sit on. They have art now, and an item's backing showing a different stone from
the cell beside it is the seam the tint was meant to hide.

Per cell, not stretched: a 2x3 sword covers six slots exactly as it covers six cells. The stone
stays as the fallback — without the file there would otherwise be nothing under the tint at all,
and a tint over bare panel art is not a backing.

## One pixel, wrong since the grid existed

> make sure when a sprite is placed on the inv grid or on the stash grid the sprite backing
> texture and the grids texture align down to the last pixel.

They did not, in the backpack. A grid item is drawn from its **foot**, and the foot of a cell
whose top row is T is row T+27 — the inventory passed T+**28**. So every item in the backpack,
its sprite, its backing, its hover outline and its socket overlay, sat one row below the cell it
belongs to and spilled into the top row of the cell beneath.

The **stash has always had it right**: its offset is `INV_SLOT_SIZE_PX - 1`, and the two windows
are otherwise the same code.

The decisive evidence that the cell is the truth and the item was out of place: the inventory's
own 1px cell rules are drawn at `GridOrigin + r * CellPx - 1` — the same boundaries `InvRect` uses
for hit-testing, and the same the stash uses. Invisible until now, because the cell had no face to
be out of step with.

Fixed by one named constant, `GridItemFoot`, used by both the backing and the sprite call so they
cannot drift apart again.

## Brighter tints, and a gradient

> make tints a nudge brighter. also can you make the tinting gradient - darker at the top,
> brighter at the bottom?

**Brighter** through one constant, `TintBrightnessNudge = 15`, added to all three depths — plain
90, dark 60, Set 100 — rather than three retuned numbers, so the order those depths were chosen in
survives the lift instead of having to be re-decided. Set's "the stone's own brightness" is now a
named `FullTintDepthPercent` rather than a bare 100 sitting where the nudge would miss it.

**The gradient** is drawn row by row, from `depth - 15` at the top row to `depth + 15` at the
bottom. Over the whole **footprint**, not per cell: the backing has been one piece per item since
2026-08-16, and a gradient restarting at every cell boundary would put six light bands down that
same 2x3 sword.

Spelled out at the call site rather than added to `TintRectRgb`: nothing else in the game wants a
graded tint, and a rect tinter that secretly ramps would surprise every other caller.

## Files

- `Packaging/resources/oracool_assets/ui/slot_background.png` (new)
- `Source/oracool/hud_art.cpp` / `hud_art.h` — `DrawSlotBackground`, `HasSlotBackgroundArt`, and the
  `BlitLayerScaled` fix
- `Source/inv.cpp` — the backpack grid and the belt
- `Source/qol/stash.cpp` — the stash grid
- `Source/oracool/shop_grid.cpp` — the vendor grids
- `Source/oracool/workshop.cpp` — Ogden's boards and both craft wells
- `Source/oracool/levski_roar.cpp` — the Cube's wells

Built clean, 832/832. `oracool.mpq` repacked.

## What to look at in play

1. An empty backpack, an empty stash page, an empty vendor grid — every cell should wear the slot.
2. Ogden's Gems and Jewels tabs: the 30px and 42x50 cells, scaled, including the cells that stand
   for nothing.
3. The belt: the texture inside the gold plate, not over it.
4. Levski's Cube: the art inside the brass rims, not across them.

## Left alone

Gillian's one-item bench on her Reroll and Imbue tabs — an 87x115 painted frame, not a grid cell.
The art would stretch to five times its size there.
