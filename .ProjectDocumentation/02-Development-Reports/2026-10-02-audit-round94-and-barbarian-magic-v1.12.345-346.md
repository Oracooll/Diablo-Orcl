# 2026-10-02 - Audit round 94 and Barbarian Magic (v1.12.345-346)

**Date:** 2026-10-02. Debug only. 904 tests pass at both builds; the Debug `diablo.ini` was unchanged by ctest.

| Version | Commit |
|---|---|
| v1.12.345 | 7eb6085e |
| v1.12.346 | 85f8ea37 |

## Round 94 (v1.12.345)
- **Unpaid melee skill swings** no longer count for Mythic Rhythm or Combination Strike. The paid-armed getters (`PaidArmedClassMeleeSkill`, `PaidArmedRfa12Melee`) gate them.
- **Melee companions** take melee passives: `OwnerBlow`'s pool is melee.
- **Blood Star and Bone Spirit:** their life price is taken directly, with a floor of 1, and no longer counts as a blow.
- **Barbarian:**
  - Find Potion finds no mana potions for him.
  - Spell books show as unreadable to him (`_iStatFlag`).

## User request (v1.12.346)
"remove barb -/+ buttons in the Magic Stats distribution. No stat points to be able to be assigned to magic. Shrines which redistribute stat point should not be able to move points into Magic."

- **`AttributeTakesStatPoints`** (player.h/.cpp) is false for Magic on a Rage user.
  - `StatPointsToSpend` returns 0 for it.
  - Neither sheet layout draws the Magic -/+ buttons, and the press and keyboard handlers skip them.
- **Mysterious Shrine:** the attribute it boosts becomes Vitality when Magic is the pick.
- **Stat-moving fountain:** it picks another attribute instead of Magic.
- **Not checked on screen yet.**

## Auditing stopped
Auditing stopped after round 94, at the user's word ("stop auditing"). The open list is as in [[2026-10-02-audit-rounds92-93-v1.12.341-344]]. The Barbarian Magic button item is now closed. Still open for the Barbarian: mana lines in gear tooltips and the Cost of Wisdom shrine's mana text.
