# 2026-10-01 - Round 58, Absolute Zero's vortex and redesign, Ice Needle, the red + (v1.12.297-302)

**Date:** 2026-10-01. Debug only. 901 tests pass; the Debug `diablo.ini` was unchanged by ctest; oracool.mpq was repacked at v1.12.298, 299 and 300.

## v1.12.297 - audit round 58 (regression review of v1.12.294-296)
- **A level save that fails at publish is caught.** `pfile_save_level` scopes its writer: the header, tables and rename happen in the destructor, so a failure there now un-marks the floor (built anew on return) and says SAVE FAILED.
- **Shares multiply the pooled blow.** `ClassMeleeSkillSharePercent`: Whirlwind's spin and a chain's extra blow multiply the whole pool instead of adding a negative term (a rank 1 spin had hit 93% of a blow on +400% gear). The pool's doc says so.
- **The sheet's Whirlwind line** shows its share (`ClassMeleeSkillSharePercentFor`); its negative bonus had read as "not a swing" since v1.12.229.
- **Physical-arrow slot quotes** include Glass Cannon.
- **`PooledWeaponDamage` clamps at INT_MAX/2048**, leaving room for crit, class, Triple Demon, Devastation and <<6.
- **Left as is:** elemental arrows still multiply the passives (they are not weapon blows).

## v1.12.298-299 - Absolute Zero is the user's vortex
- **Source:** the user's eight painted frames (`Resources/02. Oracooll Assets/Sorc Skills/Absolute Zero/Absolute Zero.png`, green screen, 2 x 4).
- **`tools/BuildAbsoluteZero.py`** (Pillow + numpy):
  - keys the green, centres each frame on its eye and fits it to one ellipse;
  - adds an even counter-clockwise spin, since the frames are variations rather than a clean turn (the arms trail it);
  - dithers alpha and builds one 250-colour palette, as the true-colour CLX loader needs.
- **The sheet:** 44 frames - 10 growing (0.5 s), a 24-frame loop whose end blends into its start, and 10 shrinking (0.5 s). Pure colours, at the user's word: no frost wave, no eye flare.
- **The engine** (`ProcessCensusEffect`) holds the loop while more than the shrink's ticks are left: the AnimLen_44 row, the floor anchor on the eye.

## v1.12.300 - Absolute Zero redesigned (user's spec)
- **The cast:** freezes what is within 4 tiles for 2 s (uniques chilled), as before. Its damage moved into the vortex.
- **The vortex:** an Absolute Zero field follows the Sorcerer like Bone Storm for 7 s (10 + 120 + 10 ticks).
  - Every 5 ticks it strikes everything within 4 tiles for 5 ticks' worth: 0.35r - 0.35(r+1) a tick, so 112r - 112(r+1) in 1/64 points (`AbsoluteZeroPulse64`).
  - It goes down stairs with him, ends in town, and ends at death (`EndAbsoluteZeroArt`).
- **The cooldown:** 30 s, a generic per-player list in rfa12's `PlayerState` (`StartCooldown`, `Rfa12CooldownTicksLeft`, `Rfa12CooldownProgress`).
  - `CheckSpell` refuses a cooling skill before any price is paid and logs how many seconds are left.
  - The skill well darkens as Charge's does.
- **The art:** the sheet is 512x256 for the 4-tile reach.
- **Texts:** the tooltip and the tree description say all of it.
- **Test:** `OracoolCooldowns.AFreshHeroHasNoCooldown`.

## v1.12.301 - Ice Needle
The Sorcerer Skill Cards page note ("use 50% size arrow instead of this asset. tinted cold."): it now flies the vanilla arrow at 50%, tinted ice blue, as Ice Lance flies it at 200%.

## v1.12.302 - the red +
The hero sheet's + is drawn in the vanilla button's red while stat points wait and that attribute is under 999:
- `RedStatButtonPixels` carries the stone face's brightness onto a dark-to-bright red ramp, so the grain and bevel stay.
- The - stays grey.
- Checked in the `DISABLED_HeroSheet` render.

## Sorcerer Skill Cards page (Version 7)
- **Absolute Zero:** shows the vortex (strip `fx_absolute_zero_vortex`, 44 frames, half size, drawn at 200%) and its new text.
- **Ice Needle:** shows the ice-blue arrow at 50%.
- **The applied snapshot** is now `cards/db300/sorcerer`; no comment threads were open.
