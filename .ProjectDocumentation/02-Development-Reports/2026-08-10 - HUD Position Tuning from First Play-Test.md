---
title: 2026-08-10 - HUD Position Tuning from First Play-Test
date: 2026-08-10
tags: [dev-report]
summary: First play-test screenshot review of the Phase 1 HUD - user-specified position corrections (everything 55px down, orbs flush into their corners, middle cluster re-centered, belt dropped toward the XP bar) plus three visual fixes spotted in the screenshot (ghost belt cells, ugly LMB placeholder block, frameless RMB icon).
---

# HUD Position Tuning from First Play-Test

## Context

The user took the first in-game screenshot of the Phase 1 HUD ([[2026-08-10 - HUD Overhaul Phase 1 - Structural Skeleton]]), converted with the project's own PCX tool (`tools/oracool_pcx_to_png.ps1`). The concept was confirmed working - corner orbs, centered belt row, Menu popup with all 10 entries, cursor tooltip live on "Adria the Witch" - and the review produced a combined correction list: five user-specified position adjustments plus three visual defects I flagged from the screenshot.

## User's position corrections

1. All bottom-HUD elements ~55px further down (toward the screen edge).
2. Health orb 110px left - flush into the bottom-left corner. (The flask sprites carry a baked-in ~96px x-offset from their anchor, which is why the number is 110 rather than the visually apparent ~14.)
3. Mana orb 70px right - flush into the bottom-right corner.
4. Middle cluster 26px right, to true-center it.
5. Belt 22px closer to the XP bar - which lands the belt cells' bottom edge exactly flush with the skill buttons' bottom edge.

All five live as named constants in `Source/oracool/hud_layout.cpp` (`HealthOrbTune`, `ManaOrbTune`, `MiddleHudTune`, `BeltExtraDrop`) - future tuning is a one-file constant change.

Because the belt now sits offset from the panel origin, a new `GetBeltSlotsAnchor()` replaced `GetMainPanel().position` as the origin for the belt's InvRect coordinates in every draw call and hit-test (`DrawInvBelt`, `FindTargetSlotUnderItemCursor`, `CheckInvCut`'s scan, `CheckInvHLight`, `CheckInvScrn`'s gate, `CheckPanelInfo`'s hover gate, and the Menu popup's slot rects) - one source of truth, so the visual and clickable areas can't drift apart. `InvRect` itself stays untouched.

## Visual fixes from the screenshot

- **Ghost belt cells**: the belt still blitted the old 8-cell background strip (232px), leaving two dead cells after the Town Portal slot. Trimmed to the 6 visible cells (174px).
- **LMB placeholder**: the engine's "no spell" icon rendered as a gray noise block. Replaced with a plain translucent well plus the spell list's own inset border.
- **RMB icon frameless**: the readied-spell icon floated with no backdrop, reading as an item hanging off the belt. Same translucent well + border treatment, border drawn after the icon so it isn't painted over.

## Verification

Debug build clean, `ORACOOL_VERSION` 1.0.56. Awaiting the user's next screenshot/play-test pass to confirm the numbers land where intended.

## Related

- [[2026-08-10 - HUD Overhaul Phase 1 - Structural Skeleton]]
