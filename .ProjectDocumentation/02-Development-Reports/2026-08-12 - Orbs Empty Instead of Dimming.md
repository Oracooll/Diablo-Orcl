---
title: 2026-08-12 - Orbs Empty Instead of Dimming
date: 2026-08-12
tags: [dev-report]
summary: The health and mana orbs now go translucent where they are empty, so the dungeon shows through the glass, instead of painting an opaque dark sphere.
---

# Orbs Empty Instead of Dimming

User idea: "can you make orbs transparent as they deplete?"

## What was there

The orbs were built as two 8-bit surfaces: `bright` (the art as painted) and `dark` (the same art with the glass circle's RGB scaled to 40%). Drawing was two blits - the dark one whole, then the bright one over it from the fill line down. A near-dead character therefore had a solid dark ball sitting in the corner of the screen. Correct, and readable, but the glass never actually looked empty.

## Translucency in an 8-bit renderer

There is no alpha channel to reach for. What the engine has instead is `paletteTransparencyLookup`, a 256x256 table whose `[a][b]` entry is the palette index closest to the average of colours `a` and `b` - the same mechanism behind `DrawHalfTransparentRectTo` and the blended CLX draws. So a translucent blit is `dst = lookup[dst][src]` per pixel, skipping index 0.

Half is the only strength that table gives in a single pass. Applying it twice does not help: the second pass averages the already-blended result *with the source again*, landing at 25% destination and 75% source - less transparent, not more. Stronger transparency would need a dither pattern, which at this size would read as noise on curved glass. Half turns out to look right anyway.

## Why the surfaces had to be split

The blend has to hit the glass and nothing else. Blending the existing whole-image `dark` surface would have made the ornament, the rim and the mount translucent too, which is wrong - those are metal.

So quantisation now splits the composition along the sphere circle instead of dimming inside it:

- `frame` - everything **outside** the glass, circle left transparent.
- `sphereDim` - the glass **alone**, dimmed, everything else transparent.

and drawing is three steps: `frame` opaque, then the empty rows of `sphereDim` blended into whatever the world drew behind, then `bright` opaque from the fill line down. Step three still blits full rows; outside the sphere those pixels are identical to what step one drew, so overwriting them is invisible - the same property the old two-blit version relied on.

## Verification

Debug build clean at `ORACOOL_VERSION` **1.1.8**, version string confirmed in the exe. Tests **347/349**, the two pre-existing failures only.

Not yet play-tested, and this one is worth looking at carefully: the effect depends entirely on what is *behind* the orbs. Against the dark dungeon floor the empty glass should read as genuinely hollow; against a bright town street, or with a lot of missile effects at the screen's bottom corners, half-transparency may be too weak to read as empty, or too strong to keep the orb's shape legible. If it needs tuning, the dim factor in `QuantizeAsset` and the blend itself are the two knobs.

## Related

- [[2026-08-12 - Six Play-Test Fixes]]
