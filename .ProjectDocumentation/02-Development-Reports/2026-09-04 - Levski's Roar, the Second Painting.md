# Levski's Roar, the Second Painting (v1.9.205)

**Date:** 2026-09-04 · **Request:** "there is a newer version of levskis roar in oracool.mpq. introduce it into the game."

## What arrived

`Levski_s_Roar.png`, 447x559, in the drop-zone root. Same furniture as the first painting - title, lion, SALVAGE column of seven, TRANSMUTE, Recipe Book, a red X - but an **8x10 grid** instead of 3x4, and painted very nearly at game size: cells measure 29.4 px across and 28.1 px down. No hover/pressed sheets came with it.

Filed as `02-source-art/delivered-packs/oracool-levski-roar-skin/levski-roar-v2-447x559.png`, README updated.

## What changed

- **`tools/CutLevskiRoarSkin.ps1`** now reads the second painting and scales the axes separately (28/29.4 across, 28/28.1 down) so both land on the 28px cell. Window 426x557, grid origin (38,166). Emits `GridColumns`/`GridRows` into the generated header; deletes the stale state PNGs from the first painting.
- **The grid is 8x10** - `LevskiGridColumns`/`Rows` in `levski_roar.h` now come from the skin header, so the grid IS the painting's grid. Eighty cells, indexed in the same `int8_t` occupancy map (anchor+1 ≤ 80). Not saved; it empties with the game as before.
- **State without sheets** - `DrawLevskiRoar` checks for the state file; if absent, hover is the theme outline and press is the outline over a half-transparent shade.
- **Test** `oracool_audit_test.cpp` "thirteen cells in a twelve-cell grid" generalised to `LevskiGridSlots` + 1 cells.

## Measured rects (source px)

close 386,46 27x28 · transmute 50,474 196x38 · recipes 270,473 140x38 · salvage 301,188+36i 106x31. Checked with a 2x overlay before cutting.

## Left alone

The first painting and its ten state sheets stay in the pack folder; the cutter's git history has the cut for them. Recipes still operate on the item array with no notion of the larger grid beyond room checks, which now pass more often.
