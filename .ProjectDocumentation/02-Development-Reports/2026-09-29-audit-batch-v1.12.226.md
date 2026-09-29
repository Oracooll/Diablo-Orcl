# 2026-09-29 - Audit batch over v1.12.200-225 (v1.12.226)

**Date:** 2026-09-29. Debug only. The user: "Now you can run a big batch of audits." Six read-only tracks ran in
parallel over everything written since the last audit pass (v1.12.200):
1. the Barbarian's mechanics;
2. effects, missiles and the Paladin;
3. companions and Battle Command;
4. state lifetime and saves;
5. the UI;
6. the skill-sound pipeline.

Every finding was checked against the code before a change. None of them corrupts a save. One crashes the Debug build.

## Fixed

### P1

- **Seismic Slam could assert (Debug).** Its flames ride the 16-way art-bolt carrier on Fire Wall's sheet, which has
  only two rows, so most aims indexed past the sheet. The frame-length table was read out of range too: length 0.
  - `SetMissAnim` now falls back to row 0 for any facing past a sheet's rows. It leaves `_mimfnum` alone, because some
    missiles steer by it.
  - `ScaleMissile` does the same.
  - Test: `OracoolAudit.SetMissAnimKeepsTheFacingInsideTheSheet`.

### P2

- **Whirlwind's 5-Rage start was never charged:** it was checked, then not taken. `StartWhirlwind` settles it.
- **Spin blows skipped a swing's on-hit layer.** `PlayerStrikesMonster` now adds the weapon's fire and lightning,
  `OnPassiveHit`, `OnRfa12Hit`, Vengeance's cold, curses, and `NoteRageCombat`. It does not add the skill latches or
  weapon wear, which at four blows a second would eat a weapon.
- **Right-clicks passed through Advanced Stats.** That window covers the left edge of the inventory and the Abilities
  window, and a right-click there refunded tree points, readied hidden rows or equipped hidden items. The F-key hover
  saw through it too. `RightMouseDown` and `DrawHoverFeedback` now stop at it.
- **Right-only skills could be bound to a left F-key**, which was then a dead key. `BindAbilityHotkey` sends such a
  binding to the right button. `EnsureValidReadiedSpell`, which every load path reaches, clears a right-only skill from
  the left button and the left F-keys: a hero saved before v1.12.224 could still cast one with a left click.
  - Test: `OracoolAudit.RightButtonOnlySkillsAreClearedFromTheLeft`.
- **Talic (the companion spin).** Each of these is fixed:
  - He was drawn spinning through his death.
  - His spin ignored the Passive stance and his leash; `StopCompanionSpin` now covers both.
  - The spin and a pending Double Throw carried into the next level. They are now reset wherever a body is let go.
- **The aura Stop cues never played:** none of the 51, the Paladin's 36 among them. The lit aura was only remembered
  when a Loop row existed, and there are none. `ResumeClassAuraLoop` now records it either way.
- **The sound page's picks that never played:** Blessed Hammer's impact (on each monster struck), Vengeance's impact
  (on the blow its cold rides) and Conversion's impact (when one turns).
- ~~**Vengeance's cold hit cold-immune monsters.**~~ *Corrected in v1.12.227:* the finding was spurious. The game has no
  cold immunity at all (`monstdat.h`: cold has resistance only, the 8-bit field is full), so the check could never fire.
  It was removed again.
- **Grown items over empty cells.** `ReseatOutgrownItems` and the stash check only flagged a footprint that overlapped
  another item. A shield grown to 2x3 over an empty row drew over cells the grid still called free. Every cell of the
  footprint must now be the item's own.
  - Test: `InvTest.ReseatOutgrownItems_ClaimsTheEmptyCellsAnItemGrewInto`.

### P3

- **Whirlwind:** it stops when another skill is readied on the right mid-spin.
- **Madawc:**
  - Aggressive stance threw from 8 tiles; it is 6 now.
  - Double Throw's second hammer aimed a tile aside and flew past a lone target. It now leaves from beside him, at the
    target.
  - Double Throw hits at the ability power, a quarter harder from level 10, as its tooltip says.
- **The magic cast sheet** is loaded only for a whirling companion. It was decoded for every companion.
- **Hover (Abilities window).**
  - The headline is the total the badges show: "Current Skill Level: 8 (5 invested, +2 from items, +1 from Battle
    Command)".
  - The level-up stat line is at the invested points, which is what the game applies.
  - A passive's effect lines include Battle Command's rank.
- **Wording.**
  - The LMB menu says "click to ready it there" for right-only skills; "light it" was the aura wording.
  - Double Swing and Frenzy now say "two swings in one attack's time", and their facts line says "Swings: 2".
- **Charge's cues follow its blow (`IsChargeBlowArmed`), not the mana check.** A real Charge that spent the last mana
  was silent, and a cooled-down plain swing voiced Charge.
- **Hammer of Faith:** its burst and impact sound play on every blow it lands, including a lone target's. The mana is
  still only spent on a splash.
- **`ScaleMissile`:**
  - It clamps to the scaler's 25-400%, so a floor lift matches the drawn size.
  - A one-row sheet is cached once, not once per facing.
- **Skill sounds:**
  - `FreeSkillSounds` drops all cues, beside vanilla's, in `effects_cleanup_sfx`. After a sample-rate change, the cues
    had played at the old rate.
  - Stale comments fixed: "306 rows", and `GenSkillSounds.ps1`.

## Not changed (open)

- **Latches armed mid-swing.** A skill armed at the click is read at the hit frame of the swing already under way, so
  a plain swing in progress takes the next skill's bonus, and now Double Swing/Frenzy's second swing. This is the
  latches' old shape, across Paladin, class-melee and RfA-12. Fixing it means capturing the skill at `StartAttack`, a
  refactor across three modules; not done without a play test.
- **Fist of the Heavens' impact:** `IS_ISHIEL` is hard-coded (an earlier explicit request), so the sound page's
  `nova.wav` pick loses. The user's call.
- **Ground Stomp** plays `holybolt.wav` twice, Cast and then Impact, both the user's picks. The user's call.
- **Madawc's hammer** rolls ranged to-hit, and gets half the damage modifier as a non-Rogue, like an arrow. His hits
  make no sound. Balance, left as is.
- **Latent rank readers:** Slip, Pierce, Counterstroke, Reed in the Wind, the cold masteries, curses and summons read
  raw points. Only Barbarian passives are ranked by Battle Command today.
- **`BlessedHammer.Cast`** is heard through the missile's own launch sound, the same file as the pick. A re-pick would
  not be heard.
- **Skill sound cache:** one sample per row, not per path. It is memory only.
- **Earlier open items stand:** the event log under the orb, monster lights on revisit, and the five performance
  refactors.

Debug build and ctest: 885/885 (three new tests). An idle leftover game process was stopped before the build. Nothing
seen in play.
