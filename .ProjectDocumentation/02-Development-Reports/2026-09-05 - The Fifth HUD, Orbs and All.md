# The Fifth HUD, Orbs and All (v1.9.213)

**Date:** 2026-09-05 · **Request:** "sweep oracool.mpq. take and use 03-transparent-slot-visual-draft" (then: "or maybe use this one - 03-transparent-belt-and-orbs")

## The sweep

`diablo-bottom-hud-2400x384-v1/` (GPT's pack from the prompt of 2026-09-04: seven designs + its own assembly script) moved from the drop-zone root to `02-source-art/delivered-packs/diablo-bottom-hud-v1/`, with a README. OneDrive refused the move; copied and removed instead.

## Which file

- `03-transparent-slot-visual-draft.png` is **24-bit with the checkerboard painted in** - no alpha. Not cuttable.
- `04-raised-stone-wells-true-alpha-belt.png` is the same design with real transparency: belt cells are holes, spheres painted. **This is the one cut.**
- `transparent-orbs/03-transparent-belt-and-orbs.png` is 04 with the sphere interiors at alpha 0 (RGB kept). The game fills orbs by dimming a painted sphere, so an empty glass would need the liquid drawn by code - a real mechanic, not done today.

## The cut

`tools/CutHudPlate.ps1` rewritten: one master, resampled once at **0.288**, cut at two vertical lines into `health_orb.png` (101x109), `middle_hud.png` (353x109) and `mana_orb.png` (102x109). The six belt holes are found by alpha (25x27 at 38-39px pitch); the wells' openings and the spheres (r 33) were measured by hand on a 3x overlay. It generates `Source/oracool/hud_plate_skin.h`, and `hud_layout.cpp` now takes every plate and orb number from it with `ScalePlate` at 1:1.

**Why 0.288 and not 0.32.** 0.32 made the holes exactly 28px, and the cradles 122 tall - their spheres' crowns 11px above `SidePanelContentBottom` (624), which the stash's 17 saved rows and both Abilities pages cannot yield (three static_asserts said so). At 0.288 the crowns sit at 625 and only the arches' tips cross the line; `scrollrt.cpp` clips both orbs to that line while a side panel is open. Drawing the orbs under the panels instead was rejected: the panels are full-height canvases and would hide the orbs outright.

## Also

- `HUD Plate Art` on by default and in the Debug ini (it is the HUD now).
- The +3 belt-item nudge (third plate) removed: the holes are the cells.
- `DrawHealthOrb`/`DrawManaOrb` take a `yOffset` for the clipped sub-surface.

## Look at

The belt holes are 25px against 28px potion sprites; the flask art is narrower than its cell so it should sit inside, but that is a screenshot call. Well openings are 49x48 around the 46px net. The orbs are r 33, down from 44.
