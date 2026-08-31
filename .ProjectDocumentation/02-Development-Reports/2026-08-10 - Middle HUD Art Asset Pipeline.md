---
title: 2026-08-10 - Middle HUD Art Asset Pipeline
date: 2026-08-10
tags: [dev-report]
summary: First real art asset lands - the user-supplied middle HUD plate (LMB/RMB wells, 6 labeled belt cells, XP groove) replaces all the placeholder wells/borders/text labels, establishing the PNG-to-palettized-surface pipeline the remaining HUD art (orbs next) will reuse.
---

# Middle HUD Art Asset Pipeline

## Context

With the Phase 1 structural skeleton tested and position-tuned ([[2026-08-10 - HUD Position Tuning from First Play-Test]]), the user supplied the first real art asset: a single plate image for the middle HUD (`Middle_HUD.png`, 679x367 source) carrying the LMB/RMB skill wells, the six belt cells with baked-in labels (Menu, 1-4, Portal), and an XP groove with its own "XP" label. The user confirmed rendering it at ~460px wide on the 960x720 canvas (vs. the old 302px placeholder row) so the baked-in labels stay legible. Orbs are the agreed next asset.

## The pipeline (reusable for future assets)

1. **Measure** - scratchpad PowerShell scans located the plate bounds (665x165 within the source canvas) and each cell/well/groove rect to the pixel.
2. **Offline prep** (`scratchpad/prep_hud_art.ps1`) - crop the plate, HighQualityBicubic-downscale to 460x114, knock out near-white background remnants to transparent, save to **both** `Packaging/resources/assets/ui/middle_hud.png` (source of truth, gets packaged) and `build/x64-Debug/assets/ui/middle_hud.png` (the dev build's loose-asset search path, confirmed live - the engine finds `assets/` next to the exe).
3. **Runtime** - new `Source/oracool/hud_art.h/.cpp`: loads the PNG via the engine's existing `LoadPNG` (utils/png.h), and quantizes the 32-bit RGBA pixels to the engine's 8-bit palettized surface on first draw. **Key simplification**: quantization only uses palette entries 128-255, which are global/identical across town and every dungeon type (per engine/palette.h's own documentation) - so one quantization pass works everywhere, with no per-level requantize and no palette-cycling risk (the animated ranges live in the level-specific 0-127 half). The plate's dark browns/golds/grays map almost entirely into the global beige/yellow/orange/gray ramps. Transparent pixels map to index 0 and are skipped at blit time (`BlitFromSkipColorIndexZero`).

## Geometry becomes art-derived

`hud_layout.cpp` now stores the plate's source-pixel measurements plus one scale constant (460/665); every rect - `GetMiddleHudRect`, `GetLmbSkillButtonRect`/`GetRmbSkillButtonRect`, `GetBeltSlotRect(i)`, `GetXpBarFillRect` - derives from them. The old placeholder-era constants (`MiddleHudTune`, `BeltExtraDrop`, `GetBeltSlotsAnchor`, InvRect-offset belt math) are gone; the plate centers itself horizontally and pins 4px off the bottom edge. Orb tuning from the play-test round is untouched.

Belt hit-tests (`FindTargetSlotUnderItemCursor`, `CheckInvCut`'s scan, `CheckInvHLight`, `CheckInvScrn`, `CheckPanelInfo`'s hover gate, the Menu/TP click checks) all moved from InvRect-plus-anchor math to `GetBeltSlotRect`/`GetMiddleHudRect` - visuals and clickable areas keep sharing one definition. `InvRect` itself remains untouched (it still serves the inventory grid).

## Placeholder code retired

- `DrawInvBelt` no longer draws the old strip background, slot backs, or hotkey numbers - just the item sprites, centered in the art's cells (28px vanilla sprites in ~40px cells; scaling them up is a possible later nicety).
- The Menu/TP cells draw nothing at all now - the art carries their frames and labels; only the click handlers remain.
- `DrawSpell`'s translucent well + border (added one round ago) is gone; the icon draws bare, centered in the art's RMB well.
- The LMB placeholder well is gone entirely - the art's empty well IS the placeholder until the assign-and-cast mechanic lands.
- `qol/xpbar.cpp` no longer loads or draws the vanilla `xpbar.clx` backdrop; the fill bar (silver/gold gradient logic unchanged) stretches across the plate's groove width (419px vs. the old fixed 307px), and the hover breakdown hit-tests the groove rect.

## Verification

Debug build clean, `ORACOOL_VERSION` 1.0.57. The processed 460px asset was visually verified crisp with legible labels before wiring. Awaiting the user's in-game screenshot: expected tuning candidates are the item-sprite vertical centering within cells, the RMB icon fit inside its well (56px icon in a ~65px well), the XP fill's vertical placement in the groove, and how the quantized browns read against the game's palette in town vs. dungeon.

## Related

- [[2026-08-10 - HUD Position Tuning from First Play-Test]]
- [[2026-08-10 - HUD Overhaul Phase 1 - Structural Skeleton]]
