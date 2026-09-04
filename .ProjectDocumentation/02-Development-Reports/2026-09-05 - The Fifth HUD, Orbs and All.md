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

## Addendum, v1.9.214 - the liquid

"use the transparent orbs version and draw the liquid in code."

The cutter now reads `transparent-orbs/03-transparent-belt-and-orbs.png`: the same design with the sphere interiors at alpha 0 and their colour still in the RGB. A resampler works premultiplied and would discard that colour, so the cutter lifts it FIRST - alpha forced to 255 inside each sphere's circle, 0 elsewhere - resamples that on its own and cuts it to the orb pieces as `ui\health_orb_liquid.png` / `ui\mana_orb_liquid.png` (opaque sphere, nothing else). The hole finder is restricted to the plate's span, since the empty spheres are alpha-0 runs too.

`DrawOrb` takes the liquid asset: if present and the same size as the cradle, it blits the liquid from the fill line down and the cradle over it; the world shows through the empty glass above the line. Without a liquid file the painted-sphere path (frame / dimmed sphere / bright) still runs. Two new assets in `hud_art.cpp`'s load, requantise, quantize and reset lists.

A mock at 100/55/15% is in the session's scratchpad (`orb_liquid_mock.png`); the in-game look is the user's screenshot.

## Addendum, v1.9.215 - the whole sphere

"you are not filing the entire orbs. cant you see? there are unfilled areas in the bottom half."

The first lift used a hand-measured circle (r 116 in the master) and the sphere is r 137, so its edge was never lifted. The cutter now finds the sphere itself, in two passes over the master: first the bounding box of the SATURATED pixels under alpha 0 (the master keeps a grey ghost of the cradle under its alpha too, and the anti-aliased fringe is partial alpha with colour - the liquid is the only hued thing at alpha exactly 0), which gives centre and radius; then every alpha-0 pixel inside that circle plus two, whatever its colour, since the rim's darkest red fails any saturation test and lifting by hue alone left a ragged edge. Centre and radius go into the header from the same measurement (60,50 / 42,50, r 39 on screen), so the game's fill line covers exactly what was lifted.

The sphere's crown now sits 2px above `SidePanelContentBottom`; while a side panel is open those two rows are clipped with the arch tips.

## Addendum, v1.9.216 - the visual draft itself

"use 03-transparent-slot-visual-draft.png as hud."

It is not the same render as the true-alpha files (45% of shared pixels differ by more than 30), so the choice is real. It has no alpha, so `CutHudPlate.ps1` now recovers it, in C# inside the script: near-white and light-grey neutral pixels in connected regions of 1500+ px are keyed out (the ground, the six holes); single stray pixels are despeckled; opaque islands under 200 px (checker fragments inside the holes) are dropped; and bright neutral pixels touching transparency (the seams clinging to the frame) are eaten in two passes. Band 1934x382 at (3,214) - 1-3px larger than the alpha sibling's, the kept anti-aliased edge.

The spheres are painted, so each is split: the interior inside r-1 is made transparent in the cradle and everything inside r+2 goes to the liquid file, then both layers are resampled and cut with the same lines. The circles are hand-measured on the script's own 3x overlay (`%TEMP%\CutHudPlate\overlay.png`) after two detectors failed - one took the sphere's reflection on the stone for the sphere, the next the arch's blue highlights. Radius 35 on screen, centres (59,48) and (45,48).

## Addendum, v1.9.217 - design 02, raised stone slots

"use 02-raised-stone-slot-design.png as hud."

Same layout, same 24-bit checkerboard, but the belt cells are painted raised stone, not holes - so the alpha finds nothing there. The cutter now carries the six cell openings as hand-measured band-local rects (design 03's holes, which share the layout; verified on the overlay against 02's painted rims) and falls back to them when it finds no holes; a count of anything but six or none still throws. Everything else - keying, spheres, cut lines - unchanged, and the generated header came out identical to v1.9.216's.

## Addendum, v1.9.218 - ten percent bigger

"scale the hud up 10%." Cutter scale 0.288 → 0.3168: HUD 613x121 (plate 387, cradles 112 and 114), belt cells 28x30 - a potion sprite now fills its cell exactly - wells 54x53, spheres r 39. The side-panel clip line stays at 624, and the spheres' crowns now rise 12px above it: with a side panel open, the orb beside it is cut flat across the top until the panel closes. Noted to the user as the cost of the size.
