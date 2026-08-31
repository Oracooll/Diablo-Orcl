---
date: 2026-08-19
version: 1.8.15
area: Levski's Roar - the monument, its window, and socket extraction
---

# Levski's Roar, in Borrowed Clothes

The user cut through the blocker: placeholders are fine, build it. So it is built, wearing borrowed
art, and it takes points 5 and 8 of the socket directive with it.

## The monument

`OBJ_STAND` - `objects\rockstan.cel`, the rock stand the Anvil of Fury sits on. The user asked for
Griswold's anvil, and it turns out **Griswold's anvil is painted into the town tileset rather than
being an object**, so it cannot be placed at all. The rock stand is the nearest anvil-shaped thing
the object table has, and it is honest about being a stand-in.

Placed one tile from the stash at {56,67}, with four fallback tiles tried in order. Town furniture
has moved before, and a silent overlap would put two operable objects on one square - the stash's
own placement only warns about that, which is a bug waiting for the day the tiles shift.

Operating it toggles the window, gated on `currlevel == 0`: in the Caves, OBJ_STAND is still the
vanilla stand the Anvil of Fury quest uses, and that must keep working.

## The window

Kanai's Cube's shape with the Horadric Cube's count. The user asked for Kanai's grid size, which is
**3x3**; they then asked for **3x4** - twelve slots, which is D2's own cube. The window sizes
itself from the grid constants, so that change was one line.

Kanai's three legendary-power slots are deliberately absent. Legendary powers are not built, and a
row of empty slots promising a system that does not exist is worse than no row.

A Transmute button that lights gold when a recipe is ready, and a Recipes button opening the book
**beside** the window rather than over it - the book is a reference while you work.

## Items in the grid are never saved

Closing the window returns everything to the backpack. That is what keeps a crafting station
entirely out of the save format: no state to write, no format to break, nothing to recover if the
game exits with the window open.

The one case that needed a real decision is a **full backpack**. There is no exported "drop this
item here" call - and I nearly wrote one. Adding an engine function to solve a UI problem is how a
stone ends up on a floor the player has already left, so the close is **refused** instead, with a
line in the log: "Your pack is full - Levski's Roar keeps what it holds." Nothing is lost.

## Point 5: Free the Sockets

A fourth recipe. One socketed item goes in; the item comes back with its sockets empty and every
stone beside it. It restores durability from `_iMaxDur` - which is precisely why Zod was written
three builds ago to stamp `_iDurability` and leave the maximum intact - and clears the item's name,
because with the runes gone a completed runeword is a base item again.

## Point 8: crafting moves here

The three existing recipes now run against the monument's grid. They are a second set of walks
rather than the backpack ones parameterised: the backpack versions respect InvList's compaction
rules, the twelve slots have none, and serving both from one function is how a compaction rule gets
applied to an array that does not compact.

## Four self-inflicted build failures, recorded because the pattern is the lesson

1. The grid recipes were appended with `>>`, landing past `} // namespace devilution::oracool` -
   twenty errors from one misplacement.
2. `AddLevskiRoarObject` moved into objects.cpp wrapped in `namespace oracool {`, which that file
   already had open: the symbol became `devilution::oracool::oracool::` and did not link.
3. `DropItemBeforeTrig` and `CopyUtf8` - two functions I called that are not visible there. The
   first does not exist at all.
4. Two hung test processes from earlier ctest runs held their own executables open, failing the
   link with LNK1168. Twice a ctest run then reported "452 passed" against stale binaries, which is
   the same trap this session hit at 1.8.8 and is worth naming again: **a test result from a failed
   build is not a result.**

## Verified

**454 tests, the usual two** (`Drlg_l1.CreateL5Dungeon_diablo_3_844660068`,
`Timedemo.WarriorLevel1to2`). Two new tests: one that the grid recipes consume AND produce, and one
that freeing sockets returns the stones, the sockets, the durability and the base name.

No new art was added, so the MPQ needs no repack.

## Still open

The real monument art and the real window art, whenever they arrive. Neither swap is constrained by
anything here.
