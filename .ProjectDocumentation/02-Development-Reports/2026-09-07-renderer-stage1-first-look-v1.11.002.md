# Renderer stage 1, the first look: stale-palette frames and three raw index writes (v1.11.002)

**Date:** 2026-09-07. **Branch:** renderer-32bit.
**Report (user, with three screenshots):** "game looks 99,9% same, no problems. there is a problem with the color of the inv grid inactive tab icons [...] loading screens are the ones with issues [...] there is some transition going on before entering load screen [...] every time an area loads, for a fraction of a second it looks incorrectly colored but then it goes ok."

## Stale palette frames (the loading screen, the transition, the first frame of a level)

One cause for all three. `LoadPalette` fills the palette arrays but never called `palette_update`; vanilla could wait for the fade-in's first update, because an 8-bit frame drawn meanwhile was recoloured by that update. A 32-bit frame is not: the loading screen was drawn through the PREVIOUS level's colour table, presented black, and then revealed by the fade as garbage; the first frame of every level likewise, until the next game tick redrew it. `LoadPalette` now applies gamma into the logical palette and rebuilds `PaletteRGB` at once, so a frame drawn between the load and the fade is drawn in the right colours.

## Raw index writes

Three sites wrote an index byte straight into the surface, which on the 32-bit screen lands as the low byte of a colour: the inventory tabs' closed chest (the blue tint in the screenshot), Levski's grid overlay and the socket rings. All three go through `SetPixelUnchecked`, which resolves by format. A tree-wide grep finds no other raw write outside the render kernels.

Suite 694/694. RTM refreshed to 1.11.002.
