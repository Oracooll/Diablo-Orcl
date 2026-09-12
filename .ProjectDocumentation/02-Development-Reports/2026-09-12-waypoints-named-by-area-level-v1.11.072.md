# Waypoints are named by their area level, and sorted by it

**Version:** 1.11.072
**Date:** 2026-09-12
**Branch:** renderer-32bit

## What was asked

> "nest waypoints to be renamed to Level 9-12 and placed between Hell and Caves waypoints
> Crypt to be renamed to Level 13-16 and be placed after Hell waypoints.
> Actually - make a formula for naming waypoints - the level number in the name to be a variable
> equal to its Area level."

## The formula

The twenty-five hand-written names are gone. A name is now built from two derived parts:

```cpp
StrCat(WaypointRegionName(level), " Level ", AreaLevel(level, DIFF_NORMAL))
```

- the region word comes from `AreaNameOfFloor` in `oracool/area_level.cpp`, which already holds the
  six names in floor order - not a second copy here, which would be the thing that went stale if an
  area were renamed. Only "Tristram" is this file's, because level 0 is not a dungeon floor and
  `AreaNameOfFloor` clamps it up to the Cathedral.
- the number is `AreaLevel`, so it is the ladder rung rather than the dungeon floor.

For floors 1-16 the two agree, which is why the Catacombs still start at 5. For Hellfire's two they
do not, and that is the requested change: **the Nest reads Level 9-12 and the Crypt Level 13-16**,
because since v1.11.060 those floors side-step onto the Caves' and Hell's rungs.

`DIFF_NORMAL` deliberately: the ladder adds a flat block per difficulty, so Normal's value IS the
rung. A name that climbed with difficulty would describe the run rather than the place, and rows
would rename themselves between games.

So "Caves Level 9" and "Nest Level 9" both exist. That is correct - they are the same depth in two
different places, and the region word is what tells them apart.

## The order

Sorted by depth, which interleaves Hellfire's regions with Diablo's:

| rows | |
|---|---|
| 0 | Tristram |
| 1-4 | Cathedral Level 1-4 |
| 5-8 | Catacombs Level 5-8 |
| 9-12 | Caves Level 9-12 |
| 13-16 | **Nest Level 9-12** (dungeon levels 17-20) |
| 17-20 | Hell Level 13-16 |
| 21-24 | **Crypt Level 13-16** (dungeon levels 21-24) |

## The part that needed care

**The row index used to BE the destination dungeon level.** `CheckWaypointMenuClick` passed it
straight to `StartNewLvl`, and the draw loop passed it straight to `IsWaypointUnlocked`. Reordering
the rows breaks that identity, so a `LevelOfRow` table is now the single mapping and both call sites
go through it. Row 13 is the Nest's first floor, which is dungeon level 17.

What deliberately did NOT change: `Player::_pWaypointUnlocked` and `OperateWaypoint`'s `_oVar1` are
still indexed by dungeon level, and `IsWaypointUnlocked`/`UnlockWaypoint` still take one - they are
called from `objects.cpp` with `_oVar1` and `currlevel` and from `debug.cpp` with a level. Saved
unlock state is therefore untouched, and a character keeps every waypoint it had.

The non-Hellfire order is written out separately rather than sliced off the front of the Hellfire
one: with the Nest interleaved at rows 13-16, the first seventeen rows are no longer levels 0-16.

## Verification

Debug and Release build clean; **718/718** tests pass. RTM updated.

The rendered list was computed from the same arithmetic outside the build and checked row by row -
all twenty-five rows, name and destination level - which is what confirms the reorder did not
silently point a row at the wrong floor.
