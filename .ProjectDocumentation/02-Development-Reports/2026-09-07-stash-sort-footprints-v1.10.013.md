# Stash SORT: the consumables layout walks footprints, not cells (v1.10.013)

**Date:** 2026-09-07
**Request:** "issues still occur. redo the logic again to avoid current issues. overlapping adjacent books and when second row of adjacent family occurs."

## The cause

The consumables layout on the materials page walked CELLS and placed every stack through `PlaceMaterialAt`, which claims one grid cell and records the item's position as that cell. Both were written for the 1x1 materials and both were wrong for a 2x2 book: the sprite (anchored at its bottom-left) spilled over the cells above and beside, the book's unclaimed cells were handed to the next stack, and the family after the books started on the row the books' lower half occupied.

## The redo

`PlaceStashItemAt(page, topLeft, item, size)` claims the whole footprint and records the bottom-left cell, exactly as the ordinary deposit does; `PlaceMaterialAt` is now the 1x1 case of it. The layout then walks FOOTPRINTS: each item takes the first free rectangle of its size, scanning row by row from its family's top row, and a family's top row is the first row below everything the previous family placed. A family is a block; the next block starts under the whole of it, books included.

## Tests

The test deposits three books and three potions mixed and, after SORT, checks that every item owns exactly its footprint on the grid, that no cell under an item belongs to another, that the books form one two-row block and that every potion sits above it. Suite 691/691. Committed locally (no push, per the user's 2026-09-07 rule); the RTM exe refreshed.
