# The canvas dim blended with dark grey (v1.9.286)

**Date:** 2026-09-06
**Request:** "blend it with dark grey instead of black."

`DrawSidePanelDim` now uses the five-argument `DrawHalfTransparentRectTo` with `PAL16_GRAY + 11` (0x3d3d3d) as the blend colour: each pixel becomes the palette's nearest match to the average of itself and that grey, a little lighter than the black blend, which halves every channel. The test is unchanged and holds.

Suite 631/632, the standing dungeon-generation failure only.
