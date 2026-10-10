# v1.12.423 - visual techniques audit, round 1 (2026-10-10)

Two read-only reviewers over v1.12.422 (the nine visual techniques). No P1. Fixed:

## P2
- **Ray-traced shadows erased the Caves' lava light.** The shadow plane started dark and was lit only by the live lights and baked flames; PlaceCaveLights bakes every lava piece's light into dPreLight, which is no source. Shadows are now a DARKENING on dLight: every source worked out twice (through walls and with the rays), and the floor pass adds the difference to dLight. Baked lava light survives, and the two falloffs' rounding cancels (also removes the one-level speckle where no wall was in the way).
- **Catacombs wall torches shadowed themselves into a wedge.** AddL2Torches puts them on the wall pieces; a ray from a light standing in a wall now ignores the walls touching its own tile.
- **The vignette sat off-centre when zoomed.** The post passes ran over the full-width pre-zoom region, of which ZoomScale shows only width/zoom columns: at 2x the vignette's flat middle reached the right edge. Bloom and mood/vignette now get `VisibleWorldRegion` (ZoomScale's own sums, the side panel included).
- **Lava haze and water light almost never reached the liquid.** Lava and water pieces are Solid (they cannot be walked on) and the classifier skipped Solid; it now skips only walls (pieces with micros above the floor pair).
- **Bloom's add-back paid the full width for any glowing row.** Each quarter row's glow now records the columns it spans.

## P3, fixed
- 26 light sheets could never glow (MonsterOwned is a file attribute set on the succubi's, mages' and Diablo's own sheets) - check removed; creature bolts and chargers are caught by their None flags.
- Lightning segments and overlapping fire walls stacked to white - those add at half strength.
- Portals' dark hearts showed the floor through them when drawn as light - portals no longer glow.
- Fire walls and infernos flooded sparks (MonsterMHit every tick) - one burst per tile per 150 ms; ForgetParticles resets it.
- Wall shadows: cast by BlockLight, not Solid (no dark rim round lava); the level key hashes every piece (every 7th sampled only one row in seven); the shadows' wall map is rebuilt only when that key moves.

## Accepted (P3)
- Particles are not occluded (an ember behind a wall draws over it); they animate while the Esc menu is open (only PauseMode stops them).
- At 1.1x the sharp-bilinear zoom is effectively a soft blend; crisp near 2x.
- Haze and water work from each pixel's ground position, so a wall in front of lava shimmers too.
- BuildShadows re-casts every baked flame of the level each light change (est. up to 2-4 ms worst case); caching their part is a later refinement.
- Bloom in town (full bright ground) to be judged by eye.

Build Debug, ctest 988/988.
