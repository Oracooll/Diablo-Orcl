# The Fourth HUD Plate (v1.9.206)

**Date:** 2026-09-04 · **Request:** "there is a new hud.png file in oracool.mpq. can you use it to replace current hud?"

## What arrived

`hud new.png`, 947x263 RGBA on transparent ground: two square wells flanking one long bar, in the gold pinstripe of the second Levski painting. Nothing baked in - no LMB/RMB/Menu/Portal labels, no belt cell frames, no icons. Filed as `01-in-use/bottom-hud/hud-plate-v4-947x263.png`.

## What changed

- **`tools/CutHudPlate.ps1`** rewritten for the new master: crop the 897x195 band at (22,30), assert the alpha box sits one pixel inside it, scale to 356x77 (the width is unchanged; the plate is 13px taller than the 64 it was).
- **`hud_layout.cpp`** - `PlateSrcSize` 897x195; well openings measured from the pinstripe (LMB 24,20 160x155; RMB 723,20 159x155); the bar opening (219,96 458x79) divided into six belt cells of 76x79 source, ~30x31 on screen. The `CentreInWell` pins re-derived (22,20 / 299,20). `NetRectNudge` zeroed - it corrected the third plate's bezel. The `LevelUpIconSize` static_assert dropped: that art has been the fallback behind the points frame since 2026-08-27 and no longer matches any well.
- What draws on it is unchanged: `DrawBeltBacking` frames the six cells, burger and portal icons land in cells 0 and 5, items in 1-4, skill icons centred in the 46x46 net of each well.

## Look at

The wells are ~63px openings now with the same 46px content square inside, so there is more stone around each icon than before. The belt backings scale to 30x31 cells (they were 33x35). Both are screenshot calls; the numbers to move are `PlateScreenWidth` and `SkillWellNetSize`.

## Addenda

**v1.9.207** - the plate switch (`HUD Plate Art`) was off in the Debug ini and defaulted off, so the first build showed the plateless row unchanged. Default flipped to on; the ini flipped. The archive was verified against the source tree with the packer's `--verify` first - it was right all along.

**v1.9.208** - "i also see the old one overlapping. hide the old one." The points-frame backings - the 64x64 frame behind each well and the six scaled copies across the belt - were the plateless row's bezels and kept drawing over the plate. `PlateIsDrawn()` (switch on and art loaded) now short-circuits `DrawSkillWellBacking` and `DrawBeltBacking`; with the plate off or missing, the frames return, so the plateless look is still the fallback.
