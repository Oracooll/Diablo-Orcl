# Item backings: vanilla's stone under the tint (v1.12.050)

**Date:** 2026-09-19 - Debug only - **819 of 819 tests**. User, with a vanilla and an Orcl screenshot side by
side: "i like the backing of vanilla diablo more. can we reach its level of transparency with all orcl backing
colours?" - then "go, build the stone underlay". Nothing seen in play yet.

## The diagnosis

v1.12.048's tint keeps each pixel's luminance and gives it the tier's hue - vanilla's method. In the Orcl
screenshot it still looked like the old opaque plate, and the reason was under the tint, not in it: vanilla's
wells are mid-grey textured stone, so its shift leaves a wash with cracks in it; the fork's panel art paints every
well as a near-black hole, so a luminance-keeping tint of it is a flat colour at the floor.

## What was built (inv.cpp)

- **`DrawSlotStoneUnderlay`**: vanilla's own slot stone under the item's footprint before the tint. The stone is
  the first backpack cell of `data\inv\inv.cel` (1.5.5 InvRect {17, 222} 29x29; the 28x28 well at 17, 223), which
  `InitInv` still loads for every class. `EnsureSlotStoneTile` decodes it ONCE with ClxDraw into a 28x28 owned
  surface and keeps the palette indices; the underlay copies them across the footprint, tiled every 28 px -
  indices on an 8-bit surface, `PaletteRGB[index]` on the 32-bit screen. No panel loaded (tests) = no underlay;
  a plain item never reaches it (bare slot still means Basic).
- `TintFloorPercent` 30 -> **10**: the floor was all there was to see on the black well; with stone under it the
  luminance carries the colour. Depth stays 90 (60 for the socket family, 100 for Set).
- The stash, shop grid and belt share `InvDrawSlotBack`, so all grids change together.

## Verification

- 819 of 819; the ethereal test (8-bit surface, no panel loaded) is unaffected by design.
- Not verified: the look. The two numbers to judge from a screenshot are the floor (10) and, if the stone reads
  too bright against the Orcl frame, a darkening of the tile (none is applied - the stone is drawn as vanilla
  draws it, and the tint's 90% depth is the only shade taken off).

## Not done

- Vanilla's rose-beige wash on plain items is still not restored (user rule of 2026-08-16); offered once on
  2026-09-19, not asked for.

## Related

- [[2026-09-19-vanilla-tint-backings-v1.12.048]] - the tint this sits under.
- [[2026-09-19-single-item-debug-commands-v1.12.049]] - the previous build.
