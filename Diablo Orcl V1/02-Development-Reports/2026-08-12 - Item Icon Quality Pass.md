---
title: 2026-08-12 - Item Icon Quality Pass
date: 2026-08-12
tags: [dev-report]
summary: The six worn-type icons looked torn and muddy. The background removal was fine by this point - the damage came after the downscale, where the 50% alpha cut deleted every thin feature. Soft edges now composite onto the slot tone, a final-resolution island sweep removes downscale crumbs, and a half-strength Floyd-Steinberg dither replaces the flat nearest-match quantise.
---

# Item Icon Quality Pass

User: "improve the quality of the already introduced assets... they even look a bit like parts of them are gone due to bad background removal."

The diagnosis mattered here, because the obvious suspect was wrong. The background removal (the flood fill from the previous pass) was already correct - zooming the current output to 5x showed the damage happening **after** it, in two places downstream of the downscale.

## The three defects, at 5x

1. **Torn edges.** The bicubic downscale leaves every silhouette edge partially transparent, and the quantiser dropped everything under 50% alpha. Any feature thinner than about two scaled pixels - boot rims, cuff edges, straps - came out nibbled or vanished. This is what read as "bad background removal": the background was gone, and so were the item's own thin parts.
2. **Floating crumbs.** Dropping those edge pixels also severed thin connections, orphaning fragments beside the gloves, shoulders and legs. The existing island filter runs at source resolution and cannot see them - they only *become* islands after the downscale.
3. **Mud.** The art's tonal range collapsed into two or three palette entries per icon under plain nearest-match quantisation - flat red-brown patches with no gradient.

## The three fixes

1. **Composite, don't delete.** Binary transparency cannot fade, but it can darken: pixels with meaningful partial alpha (>= 56) are blended over a fixed dark tone chosen to sit near the inventory slot backdrop, then kept as opaque. The silhouette edge becomes a clean anti-aliased outline on the surface it actually sits on.
2. **Second island sweep at final resolution**, threshold five cell pixels. Source-resolution islands and downscale-orphans are different populations; each needs its own pass.
3. **Half-strength Floyd-Steinberg dither** against the shared palette half, replacing nearest-match. The original game's own item icons are dithered - it is the texture this palette was drawn for. Error never diffuses into transparent pixels, so the outline stays crisp. Full strength shimmered at 56px; 0.5 keeps the ramps without the noise.

## The iteration that overshot

The first attempt also stretched each icon's tonal percentiles onto [22, 205] - and every brown turned bright copper. The lesson worth keeping: **on this palette, the stretch ceiling controls hue, not just brightness.** Brightened warm tones land on the orange ramp (208-215) instead of the red-brown ramps; the palette has nowhere brighter to put a brown except orange. Pulled back to [20, 168] with the gain capped at 1.5 - the dither carries the mid-tone gradients now, so the stretch only needs to lift the art out of the bottom two ramp entries.

The two small flecks beside the glove are in the source painting (a detached wrist-strap), confirmed against the sheet - they stay.

## Verification

Before/after strips at 5x for all six icons, three iterations. Installed via `tools/build_item_icons.cmd`, `oracool.mpq` repacked (13 files, 563,278 bytes). Debug build clean at `ORACOOL_VERSION` **1.1.19**, tests **349/351**, the same two pre-existing failures.

Play-test: the paperdoll and backpack at 1x - especially the belt and boots, the two worst offenders before - and one item on the dungeon floor, where the composite tone is least at home (it was chosen for the panel backdrop, and the ground behind a dropped icon is whatever the level draws).

## Related

- [[2026-08-12 - Six New Equipment Slots]]
