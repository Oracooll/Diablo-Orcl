---
title: 2026-08-12 - Fixing Torn Edges and Floating Debris in Item Icons
date: 2026-08-12
tags: [dev-report]
summary: Follow-up to the icon quality pass. Viewed against a white background rather than the dark game panel, two of the six icons showed genuine defects the dark preview had been hiding - free-floating fragments severed from the main shape. Traced to the source art downscaling roughly 4x into a 56px cell, not to the background-removal step the user suspected. Fixed by measuring real connected-component sizes rather than guessing a debris threshold.
---

# Fixing Torn Edges and Floating Debris in Item Icons

Direct follow-up to the same-day icon quality pass. The user asked to see the six icons on a plain white background before launching the game - a good instinct, because it exposed something the dark preview panel had been quietly absorbing.

## What white revealed

Two real defects, at 8x-14x zoom:

1. **The belt looked hollow.** Investigated first since it looked most alarming - and it turned out to be correct. The source "Leather Belt" render is a closed loop with the buckle visible and daylight showing through the middle, the same way a real belt looks laid flat. Confirmed directly against the source sheet. Not a bug.
2. **The gloves had free-floating fragments.** A small blob and a smaller cross-shaped speck sat detached in empty space near the hand, with no visible connection. Zooming the source art at 3x showed a single, fully connected hand with a wrist strap - no loose pieces anywhere in the painting.

## Chasing the wrong cause first

The instinct was to blame the background-removal flood fill from the previous pass - maybe it was leaking through shadow creases in the leather and severing small decorative bits from the interior. Implemented a fix for exactly that (erode the backdrop set by one layer, reclaiming thin leaked channels while leaving the true background alone) and re-tested.

It did nothing to the fragments. Measuring the actual gap in the downscaled icon (printing the alpha mask as a grid of `#`/`.` characters) showed 5-7 fully empty columns between the hand and the fragment - a real gap, not a hairline leak the erosion pass could have closed. The true cause: the source art's ~225px content is downscaled roughly 4x into a 56px cell, and small connecting details - a dangling buckle-strap, a stud - that are several pixels wide in the source can thin below the surviving-alpha threshold during that reduction and simply separate.

A blind alley was investigated too: switching the whole pipeline to crop from the sheets' own small "ground icon" thumbnails instead of shrinking the large hero render. The thumbnails are demonstrably clean (confirmed for gloves - solid, no floating bits, evidently simplified by the artist for exactly this scale), which would sidestep the problem at the source. But automatically locating that thumbnail's box proved unreliable across sheets with different column counts and layouts; a generic search window that worked for gloves and bracers grabbed the wrong region entirely on the belt sheet. Recalibrating by hand for all six sheets individually was a larger job than the fix in hand justified, and is filed as a follow-up rather than pursued further here.

## The actual fix: measure, don't guess

Connected-component analysis of all six icons, at the actual output resolution:

| Icon | Component sizes (px) |
|---|---|
| shoulders | 1234 |
| bracers | 1156, **14** |
| gloves | 1099, **57**, **16** |
| belt | 887 |
| legs | 1611, **22** |
| boots | 344, 257, 255, 171 |

Two things fall out of this table directly. First, debris tops out at 57px and real content starts at 171px - a wide, unambiguous gap to set a threshold in. Second, boots legitimately renders as **four** separate pieces (a left boot and a right boot, each apparently split the same way the glove's strap was), so "one icon = one connected island" was never a safe assumption; any fix based on "keep only the largest piece" would have destroyed one of the two boots.

`MinCellIsland` (the final-resolution debris filter, previously 5, briefly 14) is now **90** - comfortably clear of 57 on one side and 171 on the other, confirmed against the measured data rather than picked by eye.

## Verification

Before/after strips on both a white background (to see real defects) and the dark panel tone (the actual in-game context). All six icons now read as single clean shapes - no floating debris on gloves, bracers or legs; belt and boots unchanged, since they were already correct. Re-cut via `tools/build_item_icons.cmd`, `oracool.mpq` repacked (13 files, 563,232 bytes). Debug build clean at `ORACOOL_VERSION` **1.1.20**, tests **349/351**, the same two pre-existing failures.

One thing knowingly deferred rather than fixed: the dark, speckled edge treatment from the previous pass (composited toward a tone near the inventory slot backdrop) reads as rough black fringing on white paper. That's expected - the tone was chosen for the dark game panel, not for print - and the honest verification for it is in-game, not against white. Flagged rather than silently tuned for a background the icons will never actually sit on.

## Related

- [[2026-08-12 - Item Icon Quality Pass]]
- [[2026-08-12 - Six New Equipment Slots]]
