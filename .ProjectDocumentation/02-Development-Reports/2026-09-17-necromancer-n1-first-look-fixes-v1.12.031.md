# The Necromancer, N1: the first look in play (v1.12.031)

**Date:** 2026-09-17 - Debug only - 799 of 799 tests. Three reports from the user's first look at v1.12.030.

1. **Hero-select showed a red Sorcerer.** That screen draws palette indices and showed an owned colour through
   its fallback index. Right for the Barbarian (his fallbacks are blues), wrong for the Necromancer, whose colours
   fall back to themselves. `hero_preview` now matches each owned colour to the FRONT END palette directly
   (`NearestUiIndex`, shared with the level-to-UI table).
2. **"Necromancer" fitted neither the class list nor the name box.** The list column is a quarter of the button
   row and ellipsised him; the name box stopped at 10 characters although `_uiheroinfo::name` holds 15.
   `ShrinkToFit` in `diabloui.cpp`: a 30-point label too wide for its box is drawn at 24 points (lists and edit
   boxes); the name limit is 15.
3. **The recolour was flat.** The cause was a wrong reading of the palette: THE SHARED RAMPS COME IN PAIRS -
   224-239 is one red of sixteen, 160-175 one skin of sixteen, 208-223 one orange of sixteen. v030 took the
   halves for separate materials and sent the robe's lit folds (229-231) to near-black. Now: the whole red ramp
   and the pure reds go to dark green at 62% of each entry's own brightness, so the folds keep their place; the
   skin ramp AND 204-207 (where his face actually sits - found by a histogram of the head rows) go to ash,
   brighter than the brown they replace; the orange leather (boots, sash) becomes dried blood. Proofed on a
   contact sheet first. `CacheVersion` 6, because a cached mixed sheet stores its dye.

Note for sprite_mix: its `RampOf` still treats the shared half as eight-entry ramps. It works (it only asks
"same material?"), but paired ramps would be the truer model if that code is revisited.
