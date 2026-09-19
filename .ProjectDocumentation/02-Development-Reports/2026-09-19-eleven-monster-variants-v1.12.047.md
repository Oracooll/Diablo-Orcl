# Eleven more monster variants (v1.12.047)

**Date:** 2026-09-19 - Debug only - **819 of 819 tests**. The user's verdicts on the Monster Variants page
(https://claude.ai/artifact/AXjwm7fbJAGaYPRRZrnnSU): all eleven approved, plus one rule of his own - "I want all of
these monster variants to be meet-able in all zones except cathedral." Nothing seen in play yet.

## The kinds

Appended to `MonsterVariant` after Feral (append only: a seed maps to a roster position), each one trait, each a
recolour by the existing ramp shift (parity alternates lighter/darker), all derived from the monster's seed - no
field, no save change:

| Kind | Trait | Where it lives in the code |
|---|---|---|
| Veiled | resists magic (never immune) | `ApplyMonsterVariant`: `resistance \|= RESIST_MAGIC` |
| Ironhide | armour class x1.5 | field at spawn |
| Brutal | special attack x1.5 | field at spawn; `VariantOf` declines it for a type without a special, so the draw becomes an ordinary monster rather than a recolour that does nothing |
| Frenzied | attack animation one tick per frame faster | `NewMonsterAnim` - the one place every monster animation starts - through `VariantAnimTickDelta`, floored at one tick |
| Fleet | walk animation one tick faster | the same hook |
| Searing / Voltaic | a third of the blow as fire / lightning, against the player's resistance (clamped 0-75) | `MonsterAttackPlayer`, through `VariantHitElement`; the total at zero resistance is the same blow |
| Venomous | the blow poisons: as much again as the hit, over five seconds | `MonsterAttackPlayer` -> **`oracool/venom`** (new): a per-player bleed, refreshed not stacked, resisted as MAGIC (the fork's poison rule), ticking from `ProcessPlayers` beside the gradual healing, never the killing blow (floor 1), cleared in `InitPlayer` beside the cold armour |
| Unyielding | cannot be knocked back | `IsKnockbackImmune` beside Relentless and Implacable |
| Gilded | one more item on death, from the equipment pool two rungs deeper, good-item bias on (uper 15) | `TrySpawnGildedDrop` (items.cpp) at the death-drop seam beside the Necromancer bases, through `RndEquipmentForMonsterLevel` so the kill is always worth an item |
| Luminous | carries a light (radius 5) | field at spawn: `AddLight`; the walk code moves it; the corpse stays lit for the rest of the level, as a unique's does (corrected 2026-09-19: the death path does NOT free it - vanilla frees a monster light only on a petrified unique's death) |

Numbers (`IronhideArmorPercent` 150, `BrutalSpecialPercent` 150, `LuminousRadius` 5, the third-of-the-blow split,
the five-second bleed) are first guesses beside Hollow's and Feral's, to be corrected from play.

## Rosters

The user's rule, implemented literally: the Cathedral keeps Hollow and Feral only; every other dungeon keeps its old
subset (so the Caves still lack Hollow and the Crypt still lacks Ashen) and adds all eleven. Hell offers all fifteen.
Density is unchanged (15 / 19 / 23 / 28% by difficulty), so each kind is rarer where the roster is longer. The
roster test now pins: Cathedral = Hollow and Feral and nothing else; every other dungeon offers every new kind; every
kind has a name of its own.

## Verification

- 819 of 819. The first run of this build failed only at the two archive packs because the game was running and held
  them (a live session and a 36 KB ghost, both gone before the rerun; nothing was terminated).
- Exe 1.12.047. Not verified: nothing in play - Venomous's bleed feel, the Frenzied/Fleet tempo, the Gilded drop rate
  and the Luminous light are the things to look at first.

## Not done / left alone

- No new art: the recolour is the ramp shift the first four use. A per-kind roster weight (Gilded rarer than the rest)
  is not built; the page noted it as a small change if wanted.
- The player has no poison-cure item; the bleed is short by design (five seconds) and ends on its own.

## Related

- [[2026-09-19-imbuement-shards-v1.12.044]] - the same day's other build.
