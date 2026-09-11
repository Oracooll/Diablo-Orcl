# v1.11.052 - spells never miss (Diablo II rule, on trial)

2026-09-11. The user: "Let's switch to D2 style for a while. I want to see how overpower this would make sorcerers."

## Before

Diablo 1 rolls to hit for every player spell against a monster (`MonsterMHit`):

> Magic + 50 (+20 Sorcerer, +10 Bard) - 2 x monster level - distance, kept to 5-95%

Even a maxed Sorcerer missed 1 cast in 20. A low-Magic hero against deep monsters sat at the 5% floor. Diablo II keeps a hit roll only for weapon attacks, and Diablo III has none.

## Now

`SpellsNeverMiss` (player.h, beside `BaseHitChance`) is **true**. In `MonsterMHit`, a missile without the Arrow flag skips the roll, the same way Guided Arrow and a stone-cursed monster already did.

**What changes:**
- Every player spell that reaches a monster lands. That includes the Paladin's Blessed Hammer, Blessed Shield and its splash, and Fist of the Heavens.
- The hero sheet's "Spell to hit" row reads **Always** instead of a percentage.

**What doesn't:**
- Arrows and other weapon missiles keep their roll against armour.
- Immunities and resistances still apply.
- Monster spells against the hero, and trap missiles, are untouched. The trial is about the hero's own casting.

To go back, set `SpellsNeverMiss = false`. That one line restores Diablo's roll and the percentage on the sheet.

## What to watch

- How much faster a Sorcerer clears, especially deep in the dungeon, where the old formula punished the monster level hardest.
- Whether Magic still feels worth raising now that it no longer buys accuracy. It still gives mana and spell damage.
- Whether low-Magic hybrids (Paladin, Warrior with a staff) start leaning on spells more.

## Verification

Debug and Release built, ctest **710/710**, RTM refreshed with exe 1.11.052. There is no dedicated test: the change is one guarded line beside Guided Arrow's, and a `MonsterMHit` test needs a live monster and level. **Not seen in play.**
