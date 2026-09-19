# Item level requirements and Diablo II's experience table (v1.12.054)

**Date:** 2026-09-20 - Debug only - **821 of 821 tests** (build 24; build 23 failed four tests that pinned the old
table and one that held a level-2 starting sword on a level-1 hero - see below). The user's answers on the Level
Requirements page (https://claude.ai/artifact/DhXkqmeGB3sQAx5rQwf7bF), every decision as recommended. Phases
L1-L4 built; L5 (audit) and L6 (play) open. Not yet seen in play.

## The rule (oracool/level_requirement)

required level = max( base tier level, ceil(3/4 x highest affix level), unique's UIMinLvl, set piece's level,
highest socketed rune/gem ) - 3 per Shard of Ease, floor 1. DERIVED from the item every time it is asked; no
field, no format change.

- **Base**: the nine material tiers' floors (leather 1, iron 4, steel 7, crusader 10, bone 1, royal 16, obsidian
  19, infernal 22, diamond 25 - recognised by the first word of the name, the givebset rule) plus 12 per base
  tier (`_iOracoolBaseTier`: Normal/Nightmare/Hell/Torment); every other base asks its own `iMinMLvl`; a base the
  dungeon never drops (IDROP_NEVER: the class kits, quest rows) asks 1 - the Warrior's Short Sword carries a drop
  level of 2, and a fresh hero must hold what the game handed him (build 23's failure).
- **Affixes**: the lowest-level table row with the affix's power whose range covers the roll (the rolled row is not
  recorded), or the lowest row of that power; prefix, suffix and the pool rows (`OracoolPoolAffixMinLevel`, new in
  items.cpp); a vanilla-shaped magic item's `_iPrePower`/`_iSufPower` the same way. Three quarters, rounded up.
- **Runes**: Diablo II's ladder by position, El 11 ... Zod 69; **gems** Chipped 1, Flawed 5, Normal 12, Flawless 15,
  Perfect 18; a jewel its drop level. A runeword therefore asks its highest rune.
- **Charms** count as gear for this (their affixes); potions, scrolls, books, gold, stones ask nothing.

## Where it acts

`Player::CanUseItem` (the equip gate), `CalcSelfItems` (the worn-item loop that sets `_iStatFlag`, so an unmet
level paints red and disables exactly as unmet strength does) and `PrintItemInfo` ("Required Level: N", its own
line, red when unmet - decision D7). Nothing else learned anything.

## Diablo II's experience table (decision D3: raw, with the two brakes)

`playerdat.cpp`'s `ExpLvlsTbl` is the Arreat Summit's table verbatim: 500 to level 2, 47,116,709 to 50,
3,520,485,254 to 99. The fork asked 28x that at 50 and 9.5x at 99, and its monsters pay Diablo I experience, so
`KillExperienceFor` carries Diablo II's brakes: a monster more than five levels below the hero pays mlvl / clvl
of its experience; above level 70 the gain is cut five points a level to a tenth at 88, then a twentieth from 95.
Vanilla's +/-10% per level step stays for monsters above the hero. Experience is stored as a total and
re-levelled on load, so no save changes; an existing hero simply finds himself higher on the new curve.

## Tests

`ItemLevelRequirementIsDiabloTwosRuleOnTheForksTables` (the ratio, the rune ladder, gems, a base at Normal and
Torment, a potion, a Zod host at 69, Ease to 66 and the floor of 1, an affix-driven number bounded by 45) and
`ExperienceTableIsDiabloTwosWithItsBrakes` (the table's anchors, monotonic, the two brakes). Three old tests
updated to the new table (`Player.CreatePlayer`, the table prefix test, the hero-file test's level-51 threshold);
`CalcPlrInv.BrokenItemContributesNoStatBonus` passes through the IDROP_NEVER rule.

## Related

- [[2026-09-19-audit-pass-2-v1.12.053]] - the previous build.
