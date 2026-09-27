# 2026-09-27 - Dev notes batch and the obsidian slab (v1.12.204)

**Date:** 2026-09-27. Debug only. The user said "check dev notes and process". There were nine notes; the two about Salvation were a question and its retraction. Mid-turn the user added:
- "take this backing ...obsidian-stone-slab-background-340x720.png and place it one layer behind the hero stats canvas and simulate each frame as a punctured hole through the canvas, revealing whatever part of this layer lays behind it."

Each note's outcome is in `development-archive.md` under "Batch of 2026-09-27 (third)". The engineering follows.

## Radiance and Sanctuary: holy damage

- **The cause:** `monstdat.cpp` gives nearly every undead `IMMUNE_MAGIC`: all four zombies, all the skeletons. Radiance struck through `Strike(..., DamageType::Magic, ...)`, which honours immunity, so it hurt none of them. Sanctuary's burn went through `AuraStrike` with Magic and had the same bug.
- **The fix:** both now skip the immunity and resistance tests, the way `Monster::isImmune` has always let Holy Bolt through to the undead.
  - Radiance: `StrikeHoly` in `rfa12_effects.cpp`.
  - Sanctuary: a `holy` flag on `AuraStrike` in `aura_field.cpp`.
- The damage is still booked as Magic for the floating numbers.

## Vengeance's cold

- **Why a strike of its own:** `ItemBonusTotals` has fire and lightning weapon channels but no cold one, so the cold can't ride the blow.
- **How it works:** `ApplyVengeanceCold` (`warcries.cpp`) runs after each landed melee blow while the buff is up.
  - Damage: 1+rank to 5+2*rank cold, scaled by `Rfa12ColdDamagePercent`.
  - A quarter against monsters that resist cold.
  - Chills the target for 1 second and plays the frost flash.
- **Hooked in:** `player.cpp`, beside `OnRfa12Hit`.
- **Also fixed there:** the Life Tap call under it was indented as if it were inside `if (didhit)` but ran on every swing. It is harmless on a miss, since it bails on zero damage, but it now has its own `if`.

## Shift/Ctrl-click in the skill tree

- **New functions:** `InvestClassTreePoints` and `RefundClassTreePoints(player, skill, count)`. The single-point functions now call them with a count of 1.
- **The click:** `CheckSBook` reads `SDL_GetModState`:
  - Ctrl: `MaxTreeInvestment`;
  - Shift: 5 (`ShiftClickPoints`);
  - otherwise: 1.

## Redemption's corpse column

- **The art:** `tools/BuildRedemptionRise.ps1` takes vanilla's `ressur1` strip (16 x 96x160) from the Blizzard export.
  - It resamples each frame to 60%.
  - It re-dithers the result against a 2x2 ordered threshold, because the missile loader's alpha is binary and a resampled stipple would come out blotchy.
  - It colours each kept pixel red or blue by diagonal band, keeping its lightness.
- **In the game:** `MissileGraphicID::RedemptionRise` (PngOnly, 58x96, animWidth2 -3). Redemption's consume plays it through `AddArtEffect`, with `LS_RESUR`.

## Sunk buttons' shadows

- **The helpers:** `DrawDropShadow` and `DrawHoverShadow` take `sunk`. It shrinks the footprint by `SunkShadowInset` (1) on every side.
- **Callers that pass it:** the waypoint Act buttons and the Abilities window's icons (`IsIconPressed`).
- **Callers that don't need it:** no other sinking button draws a code shadow.

## Front-end button sounds

- **Hover:** `diabloui.cpp` tracks the focusable `UiArtTextButton` under the mouse once a frame (`TrackButtonHover`, from `UiPollAndRender`). It plays titlemov on entry.
  - `UiInitList` seeds the tracker, so a screen that opens under the pointer is silent.
- **Keyboard:** Down onto the row, and Left/Right along it, sound too.

## The obsidian slab behind the hero sheet

- **Where it lives:** the delivery (`batch-61-stat-field/obsidian-stone-slab-background-340x720.png`) is installed as `ui\hero_sheet_slab.png`.
- **How the holes work:**
  - `DrawGroupedSheet` pins the slab to the panel's top-left (`SetSheetSlabOrigin`) for its own draw and clears it after.
  - Every `DrawSheetBox` in between becomes a hole:
    - The frame's gold rows and end caps stay as the cut edge.
    - The interior shows the slab pixels behind that spot.
    - The canvas's lip casts the sheet's 3px shadow into the hole, along the inside of its top and right edges.
    - There is no drop shadow outside, since a hole casts none.
- **Scrolling:** the slab is fixed to the panel, so a frame that scrolls slides across it.
- **Fallback:** without the slab file, the frames draw solid as before.

## Tests

v1.12.204 builds clean; 873 of 873 pass. oracool.mpq repacked (943 files). `OracoolPreview.DISABLED_HeroSheet` rendered the sheet: every frame shows the slab through it, the veins running on from hole to hole, with the gold lip and the inner shadow along the top and right.

## v1.12.205: one Shift/Ctrl rule, and the Redemption preview

- **The request:** the user asked "align behaviour of shift/ctrl+clicks among all abilities/hero stats screen buttons ... double check".
- **What disagreed, three ways:**
  - The grouped hero sheet's - and + moved 10 with Shift and 5 with Ctrl, the user's rule from earlier the same day.
  - v1.12.204's tree cells moved 5 with Shift and "all" with Ctrl.
  - The list sheet's + still spent every point on Shift.
- **The fix:** `PointsPerClick(shift, ctrl)` in `control.h`, which returns 1, Ctrl 5, Shift 10, with Shift winning when both are held. Every point button now goes through it: the hero sheet's - and + on both layouts, and the tree cells, left and right.
- **Checked:** every path that spends or refunds a stat or skill point. Gamepad and touch release with plain 1-point clicks. A modified click reaches both the sheet and the tree before any Shift-attack handling.
- **Preview:** `OracoolPreview.DISABLED_RedemptionRise` renders the column the way the game does. It loads the sheet through `MissileFileData::LoadGFX`, so the palette quantisation matches play, and draws it with a real Zombie corpse frame through `ClxDraw` on the caves palette. It writes `redemption_frames.png` and `redemption_scene.png`.

v1.12.205 builds clean; 873 of 873 pass.
