# v1.12.424 - visual techniques audit, round 2 (2026-10-10)

One fresh read-only reviewer over v1.12.423. No P1. Fixed:

- **P2 - ray-traced shadows still darkened floor lit by the Caves' lava** wherever the hero's light was blocked (a shore behind a wall went dark as the hero came near). A tile dLight lights clearly brighter than every known source, unblocked, would leave it is lit by a light that is no source (the lava baked into dPreLight) and keeps that light: the darkening applies only where `dLight + 1 >= OpenLevel`.
- **Flicker multiplied flame by flame** (three overlapping flames could dip a spot by 40%): combined per tile into one multiplier, capped at +/-13%.
- **At zoom > 1 the floor, colour, particle and haze passes walked the whole pre-zoom width** (half never shown at 2x): all world passes now get `VisibleWorldRegion` and a start point shifted with its left edge.
- "portal" dropped from the glow word list (portals are not additive since round 1).

## Accepted (P3)
- An in-wall torch's +/-1 skip lets it light the far side of a 1-2 tile wall (no worse than vanilla, which has no occlusion).
- Shadows refresh with the light list: a door opened while nothing moves keeps its shadow until a light moves.
- Hover outlines drawn in the tile pass take the colour, flicker, mood and vignette.
- A see-through walkable piece with trapezoid floor micros still repaints its floor half at the tile's light (unconfirmed whether any exists).
- Liquid classification in Hell (palette 1-31) to be checked by eye; lava pieces at a wall's foot get no haze.
- Frame cost estimated ~10 ms in a typical dungeon, 15-20 ms worst case in the Caves (not measured): the frame-time line under Show FPS is the measure; every pass has its own switch.

Build Debug, ctest 988/988.
