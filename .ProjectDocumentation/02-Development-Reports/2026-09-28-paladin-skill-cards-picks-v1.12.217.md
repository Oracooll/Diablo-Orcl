# 2026-09-28 - The Paladin Skill Cards page's picks (v1.12.217)

**Date:** 2026-09-28. Debug only. The user: "Check my paladin notes in his artefact and apply".

## What was on the page

The Paladin Skill Cards page (db `picks`) held 20 animation picks and no sound picks:
- 15 set by the vanilla / scale / tint pickers;
- 5 by comment, all "remove the warcry ring".

| Skill | Pick | Where in code |
|---|---|---|
| Blessed Hammer | its spin sheet at 75%, Paladin gold | `AddBlessedHammer` |
| Blessed Shield | its spin sheet at 75%, Paladin gold | `AddBlessedShieldThrow` |
| Fist of the Heavens | its bolt at 75%, ice blue, strike kept on the ground | `AddFallingMace` |
| Wrath of the Heavens | its pillar at 75%, foot kept on the floor | rfa12 pillar loop |
| Vengeance | cold hit flash at 50%; no warcry ring | `ApplyVengeanceCold`, `AddWarcry` |
| Conversion | no warcry ring | `AddWarcry` |
| Holy Fire, Holy Freeze, Holy Shock | no warcry ring on the pulse | `ProcessHolyPulse` |
| Crusade | Holy Bolt burst 75% (was 50%), holy blue (was gold) | `SwingArt` |
| Heaven's Descent | burst 125% (was 100%), Vengeance amber (was gold) | `TickLanding` |
| Judgment | burst 25%, Paladin gold (was 50%, blue) | `SwingArt` |
| Oathbrand | burst 25%, spectral lavender (was 50%, blue) | `SwingArt` |
| Votive Strike | burst 25% (was 50%), infrared | `SwingArt` |
| Charge (new) | burst 50%, Paladin gold, on the arriving blow | `player.cpp`, beside the melee hook |
| Sacrifice (new) | burst 25%, infrared | `melee_skills.cpp` |
| Smite (new) | burst 25%, Paladin gold | `ApplyMeleeSkillOnHit` |
| Zeal (new) | burst 25%, pale warm, every blow that lands | `ApplyMeleeSkillOnHit` |
| Redemption | vanilla Resurrect at 25%, infrared, in place of the Redemption Rise sheet | `warcries.cpp` |

## One scale for every missile

`ScaleMissile(missile, percent, floor)` (missiles.h) replaces the per-effect scalers for new work. It relies on three new Missile fields, none of them saved:
- `oracoolScalePercent` and `oracoolScaleFloor` are kept through `SetMissAnim` and `missiles_process_charge`, so a missile that turns keeps its size.
- `oracoolScaleLift` is applied at draw time by `DrawMissilePrivate`.

How it works:
- **Cache:** scaled sheets are cached per graphic, facing and percent, and own their pixels.
- **Anchor:** a scale keeps either the sprite's centre (bursts, flying sprites) or a floor point above its bottom edge (pillar, bolt, rising beam).

Other changes:
- **HolyBurst:** `HolyBurst(player, tile, percent, rgb)` uses it. `DrawHolyBurst` now takes a percent and an rgb.
- **Named hues:** `oracool::hue` (missile_tint.h) holds the named hues the pages offer.
- **Cold hit flash:** `AddColdHitFlash` takes a percent.

The Frost Nova and Blessed Shield impact scalers are unchanged.

## The page

- **Rebuilt data:** the page shows the game as it now is. build.js `SKILL_FX` sets, drops and adds effects per skill.
- **Plain-sheet effects:** a `raw` effect's strip is the plain sheet, which the page draws at the game's size and tint.
- **Old picks:** the picks stay in the db; each now equals the game, so the cards show no change.
- **Visual FX Schedule:** its strips for these effects are still the pre-v1.12.217 renders.

## Test

Debug build and ctest: 878/878 passed.
