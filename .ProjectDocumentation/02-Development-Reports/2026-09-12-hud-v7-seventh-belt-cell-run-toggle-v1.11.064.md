# The seventh HUD, and a Walk/Run toggle on the belt

**Version:** 1.11.064
**Date:** 2026-09-12
**Branch:** renderer-32bit

## What was asked

> "there is now hud-v7.png file. we need to introduce it in the game and fill the new 7th belt slot
> with Walk/Run Togggle which you will find delivered by ChatGPT in Resources."

and, on the row's order:

> "belt slots order is 1-4 (potions), Portal, Menu, Run/Walk toggle."

## Deriving v7 rather than guessing it

The pack shipped `hud-v7.png` **without updating `layout-manifest.json`**, so the geometry had to come
from somewhere. Re-measuring was the wrong tool - the pack's own README says alpha thresholding finds
the wrong edges, because the wells' inward shadows partially occupy the openings.

So the numbers were derived, and the derivation was *verified* rather than assumed: a column-by-column
diff of v6 against v7 gives **1089 identical columns, a 105-column insertion, then 651 identical
columns** (1089 + 651 = 1740, v6's exact width). That is one 35-native belt cell inserted at the sixth
cell's x, with everything to its right shifted by 105 and nothing else touched.

Every manifest rect at or past master x=1089 therefore gains 105:

| | v6 | v7 |
|---|---|---|
| master | 1740x324 | 1845x324 |
| composite (native) | 580x108 | **615x108** |
| plate | 353x108 | **388x108** |
| belt cells | 6 | **7** (72…282 at a 35px pitch) |
| RMB well (plate-local x) | 291 | 326 |
| mana orb centre (master x) | 1548 | 1653 |

`hud-v7-liquid.png` did not exist either; it was generated from `hud-v6-liquid.png` by the same
insertion at the same column, which is provably correct here because the left disc ends at master 296
and the right one begins at 1446 - both entirely on one side of the cut. The generated file's discs
measure exactly where the derived orb centres say they should.

`BeltCellX[6]` in the generated header is now `BeltCellX[BeltCellCount]` with the count derived from
the manifest array, so an eighth cell would need no edit here at all.

## The toggle is slot 6, drawn in cell 6

The belt's "buttons" are not a separate kind of control: they are `Player::SpdList` indices
repurposed, with `BeltCellOfSlot` mapping slot to painted cell. Menu is slot 0, Town Portal slot 5,
and **slots 6 and 7 were already hidden and migrated empty**. So the toggle took slot 6 - every belt
path that addresses a cell by slot (rect, flash, hover) works on it unchanged, and **no save data
moves**: `MaxBeltItems` is still 8 and still the save format.

The mapping now reads: potions 1-4 in cells 0-3, Portal in 4, Menu in 5, toggle in 6. The new cell is
at the END of the row, so nothing that was already on the belt moved.

`BeltVisibleSlotCount` 6 -> 7. Its five other callers are all row geometry - the backing frames, the
plateless row's width, and the "rightmost cell" the XP bar and the menu popup anchor to - so they all
wanted the new number.

## The glyphs say what you are, not what you would become

batch-16 delivered two strips in the belt-glyph format already in use (90x30, three 30x30 cells: idle,
hover, click). `DrawRunToggleButton` picks the strip from `IsRunEnabled()` - the running traveller
while running, the walking one while walking.

That is the deliberate choice: the belt is read at a glance, and a button showing the mode a click
would *select* is ambiguous the moment you stop to think about it. The press is a momentary blink, as
on the Portal beside it, because the persistent state is already carried by which glyph is drawn.

Clicking calls the same `ToggleRun()` the R key does, so the event-log line and the mode are identical
either way. The hover hint names the current mode and mentions the key.

## Verification

- Debug and Release build clean; **718/718** tests pass.
- The cutter reports composite 615x108, plate 388 wide, seven cells at 72…282.
- A composite mock of the plate with the cell rects overlaid and the three button glyphs pasted in
  confirms the cells line up with the painted openings and the row reads potions / Portal / Menu /
  toggle. This is a HUD change and constants can be perfectly plausible while the thing is visibly
  wrong, so it was looked at rather than reasoned about.
- `oracool.mpq` repacked for both trees; RTM updated with the Release exe and the archive.

## Still to check in play

Whether the walking and running silhouettes are distinguishable at 30px on the real plate - they are
the same traveller and differ only in gait, which is exactly right for identity and the one thing a
mock at 2x cannot honestly answer.
