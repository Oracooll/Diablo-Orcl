# The portal ring centred, the burger in blue, the shadow cast like text (v1.9.288)

**Date:** 2026-09-06
**Requests:** (a zoomed belt cutout) "align TP icon better. Also - maybe we need to recolor the burger menu icon to stand out more from the background." and, mid-build: "i want potoin shadows in belt to be same cast as text shadows - a bit to the left, a bit downwards, pitchblack."

## The ring

Measured on the sheet: the idle ring's opaque box is x 9..24 in a 31px frame, 1.5px right of the frame's centre; the hover and click rings are drawn larger (x 7..23 and 5..23). Centring the FRAME therefore put the ring off by a pixel or two, differently per state. `CellOpaqueBounds` / `CentreOpaqueIn` (hud_art.cpp) now centre each state's opaque box in the cell; the burger uses the same rule.

## The burger

Its bars are dark in the PNG (mean luminance 55/255) and were quantised half toward the yellow ramp - a darker gold on a gold plate. `RepaintOntoBlueInverted` runs after the quantise and repaints every opaque pixel onto the blue mini ramp with dark source = bright blue, so the bars land on the portal ring's own 5757ff and the two buttons read as a pair. Bevel highlights come out as the darker blues.

## The shadow

`DrawBeltItemShadow` draws the silhouette SOLID black (`ClxDrawTRN`, not blended) two left and two down - the ornate borders' cast. It had been a half-transparent blend two right and two down for one build.

## Tests

- `OracoolAudit.TheBeltButtonsAreBlueAndTheRingIsCentred` - plate-only vs plate-and-button diffs on a 960x720 surface: every burger pixel on the blue ramp; the ring's box centred within a pixel in all three states.
- `TheBeltItemShadowIsTheSpriteSilhouetteTwoLeftTwoDown` - solid black at (-2,+2), nothing in the top two rows or the right two columns.

Suite 632/633, the standing dungeon-generation failure only.
