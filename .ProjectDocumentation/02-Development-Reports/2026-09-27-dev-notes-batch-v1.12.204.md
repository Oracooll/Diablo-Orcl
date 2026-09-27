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

## v1.12.206: Redemption's column redrawn, and the aura rings colour-cycle

**Redemption's column.** Rendered first, then approved:
- **The first cut:** a helix of saturated red and blue. It read as a solid barber pole.
- **The user asked:** "make it sparser and brighter at the core. try following a chesboards pattern maybe 2x2px red, 2x2px blue. use softer coloring", then "reduce the height of the effect to half".
- **`tools/BuildRedemptionRise.ps1` now:**
  - measures each row's span of the column;
  - keeps every pixel on the core and every other pixel off it, in a 1px checkerboard;
  - colours by a 2x2 chessboard of soft rose (226,112,118) and soft periwinkle (112,132,232), lifted toward white at the centre line;
  - resamples to 60% wide and 30% tall, giving 58x48 frames.
- **No code change:** the loader takes the frame height from the sheet.

**Aura ring colour cycle.** The user asked "can we add colorcylcing to all aura rings to animate them a bit?"
- **How it works:** `BlitAura`'s 32-bit path multiplies each pixel's colour and coverage by a wave that travels round the ring.
  - The angle comes from a byte per art pixel, filled at load and measured on the ellipse's own circle.
  - The wave has 2 crests, each doing a full lap per 2400 ms, at ±35%.
- **Scope:** it covers all 59 rings. The slow 4 s pulse is unchanged. The 8-bit dither path does not cycle.
- **A bug the user caught:** the first formula, `cos(2pi(crests*a - phase))`, moved the crests only 1/crests of a lap per cycle. The pattern then repeated only every 2400 ms, while the 8-frame preview covered 1050 ms, so the loop visibly jumped. It is now `cos(2pi*crests*(a - phase))`: a full lap per cycle, with the pattern repeating every 1200 ms.
- **Preview:** `OracoolPreview.DISABLED_AuraRingCycle` renders Might, Holy Freeze and Redemption at 16 moments 75 ms apart, through `DrawAuraRingPreview` and `AuraRingClockOverrideMs` (tests only).

v1.12.206 builds clean; 873 of 873 pass; oracool.mpq repacked.

## v1.12.207: Barbarian and Necromancer re-dyed by ramp

**How it was decided.** The user asked what Infravision is and whether its technique could recolour the two borrowed-body heroes. Infravision is `plrgfx\infra.trn`: one table sending every palette colour to the red ramp by brightness.
- **The answer:** the hero recolour was already a stronger table (any true colour, correctly lit). Taken literally, Infravision would have flattened every material to one hue.
- **The idea worth trying:** its trick applied per material ramp. That is a "ramp dye": each ramp gets a new hue and saturation and keeps its own lightness.
- **How it was chosen:** renders first, all through the game's own hero colour draw.
  - `OracoolPreview.DISABLED_HeroPaletteScan` counted the palette entries each armour tier uses.
  - `DISABLED_HeroRampDyes` drew the plain body, the old ramp dye, RfA-28 and three new dyes per hero, then three plate-tint strengths for the chosen ones.
- **The user's picks:** "barb C, necro B, tint the plate greys", then "go with your picks. necro - plate strong".

**The dyes.** Both live in `tools/GenHeroRampDye.js`, which now writes `Source/oracool/hero_recolour_data.inc` in RfA-28's format, so `hero_look.cpp` reads it unchanged. One dye serves all three tiers.
- **Barbarian, steel and moss** (40 entries):
  - mail and plate 240-255: cool steel (hue 205, saturation 0.12);
  - cloth 184-191: moss green;
  - gloves 168-175: grey fur;
  - leather 216-223: worn brown;
  - skin and hair: left alone.
- **Necromancer, bone and violet** (72 entries):
  - robe 224-239 and pure reds 136-143: violet;
  - skin 160-175: bone;
  - sash 208-223: near-black violet;
  - plate 240-255: violet-grey (saturation 0.22).

**Also changed:**
- The RfA-28 hand recolour is retired. `tools/GenHeroRecolour.js` stays, marked superseded. The render had shown its medium Necromancer tier with a black face under bone-yellow blotches.
- `sprite_mix` CacheVersion is 9, so cached sheets are rebuilt.
- **Tests:**
  - `OracoolHeroLook.TheBarbarianWearsBlueAndTheNecromancerGreen`, which pinned RfA-28's colours, is replaced by `TheBarbarianWearsSteelAndMossAndTheNecromancerBoneAndViolet`.
  - The "touches almost nothing" guard is now at least 32 entries; the Barbarian's dye is 40.

v1.12.207 builds clean; 873 of 873 pass.
