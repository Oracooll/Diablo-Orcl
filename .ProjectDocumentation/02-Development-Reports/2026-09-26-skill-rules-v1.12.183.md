# The two skill rules — v1.12.183

2026-09-26

> 1. all skills must provide main skill + a level up increase of some sort.
> 2. all skills pop-up tooltip must declare the main stat they provide and the level up buff they provide.
> — "simulate a virtual hero of each class and build up each of their skill to its maximum level and make
> sure each level of each skill actually levels up the level up stat ... i dont want any missalignmens or
> discrepancies."

## Audit

A read-only agent walked all 506 tree rows and 46 book spells: 117 passed both rules. Most tooltips
printed only a cost line: the 114 RfA-12 actives, all 54 Necromancer rows, the 128 Passive Skills rows
(early return), the 17 book rows (early return), and every rule-passive and off-sheet aura.

## Decisions (user)

- Passive Skills page: exempt from growing, but must print its numbers.
- Book spells: a cheaper mana cost counts as the level-up increase.
- Rows that grow nothing or stop early: add a level-up stat (not raise caps).
- Monk capstones: 5 ranks like the ladder.
- Mine: the hidden Bard, retired and inert rows are exempt; prose does not count as "declaring" — the
  Current/Next block must show the number; a main effect that itself grows each rank satisfies rule 1.

## What changed

- `oracool/skill_facts` routes a spell to its module's facts, now player-aware; `SpellLevelLines` is
  the one block the Spells sheet and the tree share; `BookSpellFactsAt` gives the book spells' hidden
  per-level terms (bolts, leap radius, durations, width, charges, golem).
- Per module, beside its formula (five agents, disjoint files): `Rfa12ActiveFactsAt` +
  `CompanionFactsAt`, `CurseFactsAt` / `NecroSummoningFactsAt` / `NecroPassiveFactsAt`,
  `PassiveFactsAt` (90 rule rows), `Rfa12PassiveFactsAt` + aura facts (Static Field, Thunder Storm,
  Redemption, Healing Mantra...), `ColdPassiveFactsAt`. Inline literals lifted into named helpers the
  rules read too — no gameplay number changed except the Ice Bolt fix below.
- `class_tree.cpp`: `ClassTreeRankBlock` (exported for the test); Passive Skills and book rows print
  their numbers; `ApplyPassive(..., assumeCondition)` so a mastery shows its numbers with a condition
  line; Prayer/Meditation/Endurance/Warmth/Weapons Master/Archery/Finery/Animosity/Unforgiving/Swift
  Harvesting/Overwhelming Essence lines; level-up line labelled "Level-up bonus:".
- Level-up stats added: Weapon Throw, Increased Speed, Leap, Taunt, Find Item, Guided Arrow, Lightning
  Bolt, Multiple Shot, Strafe, Dodge, Avoid, Evade, Pierce, Conviction, Cleansing, Static Field, Sweeping
  Reed, Seven Reeds, Hundred Fists, Breaking Current, Flowing Step, Gather the Dead, Bone Spirit
  (Necromancer), Wide Malice, Summon Resist, Unholy Offering, Corpse Explosion. Radiance's light radius
  (+1 per 5 ranks) became +2% magic resist, +1% a rank.
- Monk capstones (Master of the Long Staff, Perfect Vessel, Enlightenment): 5 ranks, +2% a rank.
- `ItemBonusTotals::armorPercent`: "+N% armour" was landing in the FLAT bonusArmor; now a real share
  (CalcPlrItemVals). `DescribeBonusTotals` prints life/mana in whole points (was 1/64) and the speed flags.
- Skill picker: a tree row uses the tree block, so the level-up line shows.
- Fixes found on the way: Golem damage line (was fixed 11-17), Lightning Fury and Frozen Orb damage
  lines, Ice Bolt casts now take Cold Mastery as the tooltip promised, Taunt's tooltip radius, ~30
  descriptions corrected (Static Field/Thunder Storm pulse every 3 s, Redemption, Doom, Decrepify,
  Lower Resist, Frenzy, Army of the Dead, Unity, Throwing Mastery, Cold Mastery, Chill Touch, Aegis
  Slam, Furnace Mouth, Immolate, Inner Sight...).

## The simulation

`test/oracool_skill_rules_test.cpp`: a level-99 hero of each visible class buys every row to its cap
through `InvestClassTreePoint`. At every rank: the level-up stat grows in every field; the hero's
`ApplyClassTreeToTotals` carries it; the tooltip line quotes the same numbers; the Current Skill Level
block holds this rank's lines and Next Level the next's; a main-effect line exists; without a level-up
stat the main effect changes rank to rank. Book rows are checked level by level against the Spells
sheet; Passive Skills rows must print an effect.

First run: 41 discrepancies (Sharpen/Inner Fire printing the same line twice, Seven Reeds/Hundred
Fists stepping, five silent Passive Skills rows) — all fixed. Final: **432 rows, 9,230 ranks,
0 discrepancies.** Full table: `2026-09-26-skill-rules-simulation-report-v1.12.183.md`.

Warnings only (by the user's rule): nine book spells have levels where nothing changes once the
mana cost reaches its floor (Telekinesis, Teleport, Inferno, Energy Shield...).

## Build

v1.12.183, Debug. ctest: 834/837 on the first run — the 3 were old tests pinning the 1-rank capstones and
"Conviction/Cleansing add nothing to the sheet"; updated, and oracool_audit_test then passed plain and
shuffled ×3. Device Guard blocked the MPQ packer again (code 4551; no asset changed). Commit f588195d.

## Left alone

- Fire Wall / Lightning Wall / Ring of Fire / Rune of Light damage lines are vanilla DevilutionX
  display formulas, not the per-tick roll.
- Guided Arrow and the other bow skills show "Damage: 0 - 0" without a bow in hand (true, but bare).
