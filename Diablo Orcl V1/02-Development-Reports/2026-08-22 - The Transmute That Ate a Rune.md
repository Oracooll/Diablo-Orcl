# The Transmute That Ate a Rune

**Version:** v1.9.9 → v1.9.10
**Date:** 2026-08-22
**Tests:** 493/495 (the two standing baseline failures)

## The bug

Levski's Roar's grid is **3 × 4 = twelve cells**, and items sit in it by footprint — `FitsAt`
bounds-checks width and height against real cells. But the recipes operate on a twelve-long *array*
of items and have no idea of footprints at all.

"Free the Sockets" is the one recipe that gives back more than it takes, so it had a room check of
its own. That check was `GridRoomAfter` — **a count of free array entries**.

A 2×3 breastplate holding six stones is **one array entry and six cells**. So the check answered
"eleven free" to a transmute that needed thirteen cells in a twelve-cell grid, approved it, emptied
the host, and handed the result to `RebuildGridOccupancy` — which called `PlaceInGrid` and
**discarded the return value**. `PlaceInGrid` returns false when nothing fits. The item was already
cleared out of `GridItems` by then, so it simply stopped existing. No message, no log line.

## Why this was not a corner case

`MaxItemSockets` is 6. A fully socketed 2×3 host plus its six freed stones is 6 + 6 = **exactly
twelve cells** — the worst case fills the grid with nothing to spare. So *one loose rune sharing the
grid* is already one cell too many. A player emptying a six-socket weapon with anything else on the
monument walks straight into it.

Which is also why the grid is 3×4 rather than Kanai's 3×3: nine cells could never have emptied a
six-socket 2×3 host at all. The fourth row was already sized for this case; the check just wasn't.

## The fix, in three parts

**1. Measure in cells, by actually packing.** New `LevskiGridCanHold(items, count)` in
`levski_roar.cpp` runs the *real* placement — largest-footprint-first, first fit — against the grid
arrays borrowed as scratch and restored afterwards. Borrowing the real arrays is deliberate: a
separate simulation with its own occupancy map is a second implementation of "does it fit", and a
second implementation is exactly how a check comes to disagree with the thing it checks.

`CollectLargestFirst` was factored out so the simulation and the real rebuild sort from one function.

**2. The recipe pre-checks the real result.** `TransmuteLevskiGrid` now builds the exact item list
the grid will hold afterwards — everything currently in it, plus a constructed copy of each stone —
and asks `LevskiGridCanHold`. On refusal it returns `"not enough room to free the stones"` instead
of an empty string, so the player is *told* rather than watching a button do nothing.

**3. The transmute is transactional anyway.** `RebuildGridOccupancy` now returns false when
something could not be placed, and the Transmute button snapshots `GridItems` and `GridCells` before
the recipe runs and restores both if the repack reports failure.

The rollback should be unreachable now that the pre-check exists. It stays because "should be
unreachable" is not a guarantee worth staking a player's stones on, and because the next recipe added
will not remember to ask.

## The test

`OracoolAudit.LevskiGridMeasuresRoomInCellsAndLosesNothing` pins four things:

- 2×3 plus six singles **fits** — otherwise a six-socket item could never be emptied at all;
- one more single **does not** — thirteen cells in twelve, the case a slot count waves through;
- the recipe **refuses** a six-socket host when a loose stone shares the grid, and the host still has
  all six stones afterwards. This is the assertion that fails against the old code;
- with the grid to itself the same host **is** emptied and all six stones come back, so the refusal
  is about room and not about the recipe being broken.

The 2×3 base is found by measuring every item rather than by naming one — a hardcoded index would be
a second source of truth about a size the art owns.

## What to look at in game

Put a six-socket 2×3 item on the monument along with one loose rune and press Transmute. It should
refuse with a message and leave everything exactly where it was. Take the rune out and press again:
the item empties, keeps its sockets, and all six stones land in the grid.
