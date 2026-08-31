---
title: 2026-08-10 - Compact Middle HUD Redesign
date: 2026-08-10
tags: [dev-report]
summary: Second middle-HUD art iteration - minimal-footprint plate (324x58 vs the first design's 460x114), flush with the bottom edge, XP bar retired entirely in favor of the existing XP Counter, RMB indicator switched to the engine's small spell icon to fit the tighter wells.
---

# Compact Middle HUD Redesign

## Context

The first plate design worked ([[2026-08-10 - Middle HUD Art Asset Pipeline]], plus its palette-timing fix) but the user found 460px wide too big. They supplied a second design ("Middle HUD.png", 1536x1024): two large skill wells flanking six equal unlabeled cells, no XP groove, on a black background this time. Their spec: flush with the bottom, dead center, minimal footprint - "scale to make the belt slots around 30x30 pixels or scale to fit the spell icon in the skills slots, whichever you decide", and "Exp bar is gone. I will use the exp counter under the minimap."

## Sizing decision

The two sizing options conflicted: 30px cells put the big wells at ~43px, under the 56px large spell icon. Resolved in favor of minimal footprint by using the engine's **small (37x38) spell icon** for the RMB readied-spell indicator instead - it fits a 43px well with margin, and is already loaded in-game (InitSpellBook). Result: **plate 324x58 on screen** (down from 460x114), cells 30x28 (the 28px vanilla item sprites fit almost exactly), flush to the bottom edge, dead center.

## What changed

- **Asset prep** (`scratchpad/prep_hud_art2.ps1`): the new source has a black background instead of white, so brightness-keying doesn't work (wells and background are both dark). The plate silhouette is cut with a geometric 3-rect union mask (left square / middle strip / right square, measured from the source), leaving the notches between the tall squares and the shorter strip properly transparent. Verified against a magenta-background preview before install. Crop (30,356,1477x265) → mask → bicubic downscale to 324x58 → alpha hardened (>=128 opaque, else transparent) → installed to both asset locations.
- **`hud_layout.cpp`**: new source measurements (wells ~198-206px, cells 137px at 165px pitch) + `PlateScreenWidth = 324`, `PlateBottomMargin = 0`. `GetRmbSkillButtonDrawPosition` now centers the 37x38 small icon. `GetXpBarFillRect` deleted.
- **`panels/spell_list.cpp` (`DrawSpell`)**: `DrawLargeSpellIcon` → `DrawSmallSpellIcon` throughout, including the Furious Charge cooldown-fill partitions (56 → 38px height) and an inline small-icon-aligned hotkey label (PrintSBookHotkey assumes the large icon's 56px box).
- **`qol/xpbar.cpp`**: gutted to no-ops with an explanatory header - the per-level progress readout lives on in the Oracool XP Counter under the mini-map, which the user prefers. Call sites (scrollrt, CheckPanelInfo, Init/Free) left wired so a future groove-bearing plate design can bring a bar back as a one-file change.
- Item placement, hit-tests, Menu popup anchor, and dirty-rects all follow automatically since everything reads hud_layout's rects.

## Plate v2 (2026-08-11, v1.0.64)

The user supplied a refined plate ("Middle HUD v2.png") and asked for it 10% larger than the shipped 324px version, which read as cramped in play. The new art restores per-cell labels (LMB / Menu / 1-4 / Portal / RMB) sitting in notches above each cell, and bakes in a hamburger glyph for the Menu cell and a glowing portal ring for the Portal cell - so those two buttons need no runtime drawing at all, matching what the code already does.

Rendered at **356x62** (324 + 10%), giving ~33px belt cells and ~49px skill wells. The RMB indicator stays on the engine's small 37x38 spell icon, which still fits comfortably. Geometry re-measured from scratch rather than scaled from v1: the cells are a different size and pitch, and the skill wells now sit higher than the belt row.

Masking initially used a three-rect geometric silhouette (tall left square / shorter middle strip / tall right square), on the assumption that v2 - like v1 - had an opaque background needing to be cut away.

**That was wrong, and it showed in play as a light band between the skill buttons.** v2 ships with a real alpha channel: the plate's silhouette is already authored, and the "black background" is transparent pixels that merely retain warm RGB values (sampled (47,41,33) at A=0 just above the labels). Forcing everything inside the mask rectangles to opaque baked that glow in as a solid band spanning the gap between the LMB and RMB squares, above the label row.

Fixed by dropping the geometric mask entirely and taking the silhouette from the source alpha (>=128 opaque, else transparent), with the crop set to the art's own alpha bounding box - (16,337) 1505x272, slightly larger than the brightness-derived guess, which had also been clipping a few pixels of the plate's soft edges. Transparent pixels keep their RGB through the downscale so the filter blends against plate-coloured neighbours instead of pulling in black, then alpha is hardened afterwards because the engine's blit is binary (skip index 0). Plate is now 356x64. Verified on a magenta backdrop: the gaps between labels are genuinely see-through.

Lesson for future art: check the alpha channel first. Both prior assets (v1 plate, orbs) happened to need manual silhouettes, which made a geometric mask the reflexive choice here.

## RMB icon centring and button feedback (v1.0.67)

Two follow-ups from play-testing the v2 plate.

**The readied-spell icon sat off-centre in the RMB well.** The centring maths assumed the small spell icon was 37x38, but only its *width* is fixed - it comes from `LoadCel("data\\spelli2", 37)`, which takes a width and derives the frame height from the CEL itself. The guessed height was wrong, so the vertical centring was off by that error. Added `GetSmallSpellIconSize()` (`panels/spell_icons.cpp`), which reports the loaded sprite's real dimensions, and `DrawSpell` now centres against those. The hotkey-label alignment and the Furious Charge cooldown partitions were on the same hardcoded number and now follow it too. `GetRmbSkillButtonDrawPosition()` was deleted, since the centring belongs where the sprite is known.

**The Menu and Portal cells had no click feedback**, their frames and icons being baked into the plate art with no state to react with. Added `DrawBeltButtonFeedback()` (`oracool/hud_menu.cpp`): the Menu cell stays lit for as long as its popup is open, so it reads as a toggle rather than a blink, and both cells flash for 140ms when clicked. The Portal cell flashes even when the cast is rejected (in town, or multiplayer) - an unlit button would read as a dead click when the real reason is "not available here". Timing is in ticks rather than frames so the flash is frame-rate independent, and the unsigned tick subtraction means a `SDL_GetTicks` wrap ends a flash early rather than latching it on. Starting a flash marks the belt component dirty, so the effect still animates on the partial-redraw path used at narrow canvas widths.

## Verification

Debug build clean, `ORACOOL_VERSION` 1.0.59. Awaiting the user's screenshot. Watch-fors: the geometric mask edges against varied dungeon backdrops (the frames' outer bevels are cut at the measured rect edges), small-icon legibility in the RMB well, and item-sprite fit in the 30x28 cells (near-zero margin by design).

## Related

- [[2026-08-10 - Middle HUD Art Asset Pipeline]]
- [[2026-08-10 - HUD Overhaul Phase 1 - Structural Skeleton]]
