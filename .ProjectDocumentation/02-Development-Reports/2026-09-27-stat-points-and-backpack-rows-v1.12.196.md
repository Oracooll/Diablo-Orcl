# Stat points below zero, and 1x1s on the fourth row

2026-09-27 — v1.12.196

## Why

Two dev notes the user logged while playing v1.12.195.

## The + spent points the hero did not have

- **Note:** "+ button on stats can increase stats even if no stat points are available and stat points just increase negativly".
- **Cause:** since v1.12.195 the grouped sheet's + is always pressable. `ReleaseChrBtns` asked whether there were points for ctrl (5) and shift (10), but a plain click started from `statPointsToAdd = 1` and never asked. The list sheet was safe only because its + cannot be pressed at 0 points.
- **Fix:** every + click goes through `StatPointsToSpend(player, attribute, requested)` (player.cpp). It gives the smallest of what was asked, the unspent points and the room below 255, and never less than 0. `CapStatPointsToAdd` in control.cpp is gone.
- **A hero already below zero:** the bad spends were recorded in `_pStatPtsSpent*` like real ones, so clicking - on that stat refunds them and brings the pool back to 0.
- **Test:** `Player.StatPointsToSpend_NeverMoreThanUnspentNorPastTheCap`.

## 1x1 items landed on row 4

- **Note:** "when 1 grid slot items are picked up they alnd on row 4 in my inv grid. that is some remnant of vanilla inv grid. make them land on bottom row - 7th."
- **Cause:** `AutoPlaceItemInInventory` still used vanilla's 10x4 backpack as literals. A 1x1 tried cells 30-39 first, which is row 4 of the 10x7 grid. Worse, no shape could reach rows 5-7 at all:
  - 1x1s only tried rows 1-3 after that;
  - 1x2 and 2x2 items only tried rows 1-3 as a top row;
  - 1x3 items tried cells 0-19;
  - 2x3 items tried rows 1-2.

  So a pickup went to page 2 while three rows of page 1 were empty. This is the same slip `AddGoldToInventory` had until 2026-08-19.
- **Fix:** the ranges now come from `InventorySizeInSlots`, and vanilla's order is kept:
  - 1x1s fill the bottom row (row 7) from left to right, then the columns from the right, bottom up.
  - Taller items fill from the top and can use every row.
  - The starting gold goes in the bottom-left cell (60), not 30.
- **Tests:**
  - `InvTest.AutoPlaceItemInInventory_OneByOneLandsOnTheBottomRow`.
  - `InvTest.AutoPlaceItemInInventory_EveryShapeReachesAllSevenRows`: each shape fills page 1 completely before anything spills over (70 1x1s, 30 1x2s, 15 2x2s, 20 1x3s and 10 2x3s).

## Tests

- v1.12.196 Debug builds clean, and 853 of 853 tests pass, including the shuffled runs.
- One test failed on the first run because my own test case was wrong: a dagger is 1x2, not 1x3. I made the dagger the 1x2 case, used a short sword for 1x3, and reran inv_test: 69 of 69 pass, plus 3 shuffled rounds.
