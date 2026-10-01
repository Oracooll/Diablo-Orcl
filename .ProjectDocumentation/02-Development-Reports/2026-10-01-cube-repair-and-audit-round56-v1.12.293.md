# 2026-10-01 - Audit round 56 and the Cube repair (v1.12.292-293)

**Date:** 2026-10-01. Debug only. 899 tests pass; the Debug `diablo.ini` was unchanged by ctest; oracool.mpq was repacked.

## v1.12.292 - audit round 56
**Tracks:**
- regression review of v1.12.291 (no regressions);
- stores and town (3rd pass).

**Fixed:**
- **Selling from backpack pages 2-10.** A sale there counted the item's own cells as room for the gold, but proceeds only ever go to page 1 and the stash, so the gold could be lost. Only page 1's cells count now, on the list sale, the right-click sale and `ShopSellItemAt`.
- **Gold hardening:**
  - `TakePlrsMoney` and `TakeGold` refuse a cost of zero or less;
  - `WithdrawGold` clamps to the stash's gold at the moment of withdrawal.
- **Withdraw box:** both direct closes of the box call `CloseGoldWithdraw`, so the text input stops with it.
- **Grim Ward and Tranquility in town:** Grim Ward's repel and Tranquility's chill do nothing there. Tranquility's heal and both clocks run on.
- **Comment:** `paladin_ranged.h` names Fist of the Heavens' ring.

**Not changed:**
- **A gambled unique that misses the pack.** It is already marked found: `StoreAutoPlace` sets the flag before it tries placement.
- **Recast and Consecrate do not carry oils.** They change the base, possibly to another kind of item, so the oils stay with the old item.
- **Items saved under v1.12.290.** An item from before that version may have had its oil armour turned into base drift. This cannot be told apart afterwards; noted only.

## v1.12.293 - Levski's Cube repair (user, 2026-10-01)
- **Approval:** approved on the Levski's Cube Frames page. The sheet is byte-identical to the approved preview.
- **Cut left sub-cubes:** in opening frames 5-8, each left sub-cube the stills cut off at x 21 is rebuilt as the mirror of its right-hand twin (`tools/LevskiCubeRepair.cs`). Closing plays the same frames backwards, so it is fixed too.
- **Opened loop:** it holds its one whole frame, so the tear between the six stills is gone.
- **Build chain:** `build_levski_cube_sheet.cmd` runs the repair once, after the steadying. The tool is not idempotent, and its header says so.
- **Order-dependent test:** `LeavingAGameDoesNotLeakLevskisGridToTheNextCharacter` now loads the test archives first. Without them, the Cube's page art was cached as missing for the rest of the run, and a later Cube test in a shuffled order found no tabs (shuffle seed 71480).
