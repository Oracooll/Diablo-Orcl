# Item backings: vanilla's tint, back (v1.12.048)

**Date:** 2026-09-19 - Debug only - **819 of 819 tests** (second run; the first run failed one test that
asserted the old plate, rewritten below). User: "can we apply vanilla methodology of backing to orcl backings?"
then "build it without borders as the second layer". Nothing seen in play yet.

## What vanilla does, and why it could not be copied

DevilutionX 1.5.5's `InvDrawSlotBack` (checked against the fork's baseline commit and the upstream tag) covers
nothing. It reads the slot art back from the frame and moves every pixel in the grey ramp (240..255) one shade
deeper into the item class's ramp, so the stone stays stone, tinted. That is a test on palette INDICES. The fork's
panels are true-colour PNGs on a 32-bit screen: no indices, and nothing in the grey ramp anyway - which is why the
backing has been an opaque plate with a two-pixel border since 2026-08-16.

## What was built

- **`TintRectRgb`** (engine/render/primitive_render): vanilla's idea on colour values. Each pixel keeps its
  luminance (Rec. 601, the weights `RampIndexFromLuminance` uses) and takes the given hue, normalised so its
  brightest channel is full; a floor (30%) is the share of the hue a black pixel still shows, so a dark well reads
  as colour rather than as a darker well; a depth (90% = vanilla's one shade deeper; 60% for the socket family's
  "dark" backings; 100% for the lighter Set green) is applied last. On an indexed surface (tests) it is vanilla's
  shift verbatim, into the fallback ramp.
- **`InvDrawSlotBack`** rewritten: the same precedence ladder (sockets > ethereal > Rare > Buffed Unique > Primal >
  Set > Magic > Unique > nothing; inspecting = orange), each rung now a hue + depth + fallback ramp, one
  `TintRectRgb` over the footprint. **No border anywhere** - the user's choice over "borders as a second layer".
  The socket family, which was identified by its border, is a dark tint now: socketed = white hue at 60% (the well
  pushed down to the dark grey asked for on 2026-08-20), runeword = the fork's green at 60%, ethereal = the old
  outline's violet at 90%. A plain item and gold still get nothing; vanilla's beige is deliberately not restored.
  One rectangle per item, not vanilla's per-cell tinting (user, 2026-08-16).
- The **set-border finding** of the same morning (border painted by index 152 = orange on the 32-bit screen) is
  closed by there being no border.
- The stash, the shop grid and the belt share the function, so all grids change together.

## Tests

- `InvTest.EtherealItemsWearAPurpleTintAndNoBorder` replaces `...BackingAndOutline`: the surface is filled with a
  grey-ramp shade, and the test pins the one-deeper shift into the fallback ramp, corner == centre (no border), a
  non-grey pixel untouched, the socket tint for a socketed ethereal base, and nothing for a plain item.
- The first run of this build (build 15) failed only that test - it asserted the plate's indices on a zeroed
  surface, which vanilla's shift leaves alone. Rewritten, build 16: 819 of 819. The inv.cpp comment received a
  one-word wording fix after build 16 started (comment only; the exe is the same code).

## To look at in play

The three numbers at the top of `InvDrawSlotBack` (`TintDepthPercent` 90, `DarkTintDepthPercent` 60,
`TintFloorPercent` 30) are first guesses. A screenshot of a full backpack with a magic, a rare, a set and a socketed
item is the verification; the floor is the number most likely to move (too low and a tint on the dark well reads as
a darker well, too high and it flattens the stone).

## Related

- The Item Backings page (https://claude.ai/artifact/22q4axpNMnBN3VEEHHcrFN), version 3: samples and numbers now
  describe the tint; comment boxes remain for amendments.
- [[2026-09-19-eleven-monster-variants-v1.12.047]] - the previous build.
