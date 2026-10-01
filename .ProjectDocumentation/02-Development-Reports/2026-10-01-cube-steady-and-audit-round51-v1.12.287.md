# 2026-10-01 - Levski's Cube stands still, and audit round 51 (v1.12.286-287)

**Date:** 2026-10-01. Debug only. 899 tests pass; the Debug `diablo.ini` was unchanged by ctest.

## v1.12.286 - the cube's foundation (user, 2026-10-01)
- **Cause:** the opening and opened frames sat 4-7 px higher than the closed-idle loop and swayed up to 2 px sideways.
- **Fix (`tools/LevskiCubeSteady.cs`):**
  - lines every frame up on idle frame 1's pedestal;
  - pastes the idle foundation below the user's cut line: row 139 at the sides, with a V down to row 150 under the open cube's front corner.
- **Build chain:** the tool now runs as part of `build_levski_cube_sheet.cmd`.
- **Approval:** previewed on the temporary "Levski's Cube Frames" page and approved there.

## v1.12.287 - audit round 51 (regression review of v1.12.285-286)
- **Weaken and Decrepify** no longer lift a 0 monster-vs-monster blow to 1.
- **Torment picker:**
  - its OK / Cancel sit where the difficulty screen has them (`DifficultyButtonRowBottomMargin`, shared);
  - its column hit areas are clamped above the button row on short windows;
  - Cancel returns to the Torment row.
- **Test comments** now name v1.12.285.

## Waiting on the user
- **`tools/LevskiCubeRepair.cs` is committed but not applied:**
  - the cut left sub-cubes in opening frames 5-8 are rebuilt from their mirrored right-hand twins;
  - the opened loop holds its one whole frame to end the tear.
  - It is shown on the page as "Proposed".
- **Balance questions from round 51:**
  - Unarmed non-Monk heroes now get almost no Strength damage (their weapon roll is 1-3).
  - The Barbarian's Vitality term is 1% a point.
  - Skill +% and Glass Cannon do not multiply the Strength part (D2 pools them).
- **Not changed:**
  - On the 8-bit UI surface the picker's columns have no art.
  - Up and Down do nothing on the picker; Left/Right and Enter/Esc work.
