# Levski's Roar, the Fourth Layout (v1.9.268)

**Date:** 2026-09-05 · **Request:** "i made another levski UI window. it is in oracool.mpq. grab it and apply it. also apply the necessary icons for hover and click functions."

## The painting

`Oracool.MPQ\levskis roar.png`, 320x352, with everything painted in: the frame, LEVSKI'S ROAR, the SALVAGE stone plate, the 3x4 grid (29px pitch, 26px interiors, exactly the earlier painting's cells), the seven salvage gems and the recipe book on 36x36 plates in two columns (x 170 and 229, rows 138/178/218/258), and Transmute on a 60x60 plate under the grid at (57,251).

## What changed

- **1:1.** The painting ships unscaled. The grid's painted pitch is now its own header constant (`GridPitch` 29) and the game's 28px item cell sits inside each painted square; before, the painting was squeezed by 28/29 to make the two agree. Crisp icons, and the overlays land exactly.
- **Overlays only.** No default state file: the painting is the rest state. Hover and pressed are cut from the icon-controls-v3 pack's 96px art down to the painted plate sizes (36 and 60) and drawn over the plate while the cursor is on it or the press flash runs. The window's loop draws nothing at rest.
- **SALVAGE** is painted, so the code-drawn title is skipped when the skin gives it a zero rect.
- The close button stays the game's red X at the frame's top-right.

Measured by matching the pack's default icons against the painting (plate positions) and by the gold/luma profiles (grid), then the plate rims for exact sizes. A 2x mock with three overlays and the cell grid confirmed alignment before the build.

## Filed alongside

Six new GPT packs from the root: the limestone TRANSMUTE / RECIPE BOOK plates, the LEVSKI'S ROAR and SALVAGE stone titles (sources of what the painting carries), the salvage plate, and two stone-navigation control packs (not consumed).

## Verification

Debug build clean; 625/626 with the standing `Drlg_l1` failure. In the game: open the stand, hover each gem and the transmute plate for the overlay, click for the pressed frame, put a 2x3 armour in the grid.
