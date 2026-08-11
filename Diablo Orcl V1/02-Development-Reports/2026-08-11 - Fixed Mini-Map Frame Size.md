---
title: 2026-08-11 - Fixed Mini-Map Frame Size
date: 2026-08-11
tags: [dev-report]
summary: The mini-map frame resized on almost every zoom step (232x118 up to 306x175, non-monotonically). Pinned it to 306x175 - the largest bounding box across all 25 zoom levels - so a textured border can be built against constant dimensions.
---

# Fixed Mini-Map Frame Size

## Context

The user noticed the mini-map box changing size with nearly every zoom step, and asked for a single size that fits all zoom levels, fixed permanently - they intend to add a textured border, which needs constant dimensions to draw against.

## Root cause

`CalculateMiniMapDiamondSize()` (`Source/automap.cpp`) sized the box to whatever the isometric diamond actually measured at the current zoom. That was deliberate when it was written - it avoids dark dead space around the diamond - but the size falls out of a chain of integer divisions (`AmLine`, cells-per-view, tile pitch), so it lands somewhere different at almost every scale, and not monotonically.

Tabulating all 25 zoom levels (scales 6-30, `scratchpad/minimap_sizes.ps1`):

| scale | box | | scale | box |
|---|---|---|---|---|
| 6 | 232x119 | | 24 | 255x148 |
| 14 | 252x118 | | 25 | 256x140 |
| 20 | 286x141 | | 26 | 306x156 |
| 21 | 286x164 | | 27 | 306x175 |
| 23 | 270x137 | | 28 | 270x148 |

Width swings 232-306, height 118-175, and consecutive steps can jump either way (25 -> 26 -> 27 -> 28 goes 256x140 -> 306x156 -> 306x175 -> 270x148). Exactly the "changes with almost every step" the user described.

## Fix

`CalculateMiniMapFrameSize()` walks the whole zoom range once and takes the per-axis maximum, giving **306x175** - large enough that no zoom level ever clips, identical at all of them. Cached in a function-local static since it depends only on compile-time constants. `CalculateMiniMapScreenRect()` now returns that fixed size, so the backing, the crop, and the dashed outline all use it.

It is computed rather than hardcoded so it stays correct if `MiniMapSize`, the scale limits, or `AmLine`'s math ever change - though since border art will be sized to the result, any such change would mean redrawing that art. Below the widest zoom the diamond now sits centred with dark backing around it, which is the intended trade for a stable border.

## Knock-on benefit

Four other corner widgets anchor themselves off `GetMiniMapScreenRect()` - the event log window, the game clock, the XP counter, and the XP gain indicator. All of them were being nudged around by the same zoom-dependent geometry; they are now stable too.

## Verification

Debug build clean, `ORACOOL_VERSION` 1.0.65. Needs an in-game zoom sweep to confirm the frame holds still and nothing clips at maximum zoom.

## Related

- [[2026-08-09 - Mini-Map Waypoint and Portal Markers]]
