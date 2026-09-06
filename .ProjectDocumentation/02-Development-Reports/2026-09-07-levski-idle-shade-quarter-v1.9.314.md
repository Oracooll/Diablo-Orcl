# Levski's Roar: the idle plates shaded by a quarter, not a half (v1.9.314)

**Date:** 2026-09-07
**Request:** "i find the buttons too dark in idle state. reduce the darkening by half."

The idle shade was one pass of `DrawHalfTransparentRectTo` - the black-blend table, about 50%. No engine table gives 25% directly, but two it already has compose to it: `paletteTransparencyLookup[0][c]` is c at half brightness and `paletteTransparencyLookup[c][that]` is the midpoint between c and it, three quarters of c. `DrawQuarterDarkenRect` (levski_roar.cpp) does that per pixel over the idle plate's rect; exact in the palette's own arithmetic, no dither. Hovered and pressed plates are untouched, as before.

Suite 683/683.
