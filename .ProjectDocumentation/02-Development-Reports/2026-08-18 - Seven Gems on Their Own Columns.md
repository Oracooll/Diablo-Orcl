# Seven Gems on Their Own Columns

**Version:** 1.7.70
**Date:** 2026-08-18
**Request:** "I think gems are missing affixes. Apply the D2 affixes to them, so they are useful."

They were missing most of their identities. The old table gave each gem one or two modest numbers
of this fork's own invention - Amethyst had no strength, Diamond no armor attack-rating, Topaz no
magic find, Ruby's armor roll was resist instead of life, and the Skull had no drain at all. The
table now follows Diablo II's own three-column grid (weapon / helm-armor / shield), with Normal
rows chosen so Perfect (x2 on this fork's quality ladder) lands on D2's own Perfect values.

## The grid

| Gem | Weapon | Helm / Armor | Shield |
|---|---|---|---|
| Amethyst | +8% to hit | +5 strength *(P: +10 = D2)* | +15 armor *(P: +30 = D2)* |
| Diamond | +3 damage ¹ | +5% to hit | +10 all resists *(P: +20 ≈ D2's 19)* |
| Emerald | +4 damage ² | +5 dexterity *(P: +10 = D2)* | +20% magic resist ² *(P: 40 = D2)* |
| Ruby | 8-12 fire *(D2's Normal exactly)* | +19 life *(P: +38 = D2)* | +20% fire resist *(P: 40 = D2)* |
| Sapphire | +10 mana ³ | +19 mana *(P: +38 = D2)* | +12% magic resist, +10 mana ³ |
| Topaz | 1-20 lightning *(P: 2-40 ≈ D2)* | +12% magic find *(P: 24 = D2)* | +20% lightning resist *(P: 40 = D2)* |
| Skull | +2 life per kill ⁴ | +8 life, +8 mana ⁴ | thorns, +4 armor ⁴ |

**The four substitutions**, where D2 leans on channels this engine lacks - documented in the
table's own comments rather than fudged silently:

1. Diamond's "+% damage vs undead" has no channel → flat damage.
2. Emerald is D2's POISON gem and D1 has no poison → flat damage in weapons, poison resist →
   magic resist.
3. Sapphire is D2's COLD gem and D1 has no cold → it becomes **the mana gem**, its D2 armor
   identity extended to every host; cold resist → magic resist.
4. Skull's steal percentages ride D1 flags that exist only at fixed 3%/5% and cannot
   quality-scale → the weapon gets **+life per kill** (same drain fantasy, scales Chipped 1 →
   Perfect 4), the armor gets life+mana for "replenish life / regenerate mana", and the shield
   keeps D2's attacker-takes-damage via the engine's own `Thorns` flag (the same
   `totals.flags` route the class tree's thorns already take).

## Mechanics that came with it

- **Life per kill** is an event, not a stat: `GemLifePerKill` mirrors Tir's `RuneManaPerKill`,
  granted in `MonsterDeath` beside it. One deliberate difference, worth its comment: the summer
  resolves through `ResolveGem` (index → Normal row + quality percent), because `FindGemRow` only
  knows Normal rows - it would have silently answered 0 for every Flawless and Perfect skull.
  Weapon hosts only, D2's own placement.
- **Magic find on Topaz** feeds the existing `magicFind` channel (the unseeded drop-tail upgrade
  chance the charms already use) - the first item source of it.
- **The tooltip prints everything now.** `GemSocketLine` gained mana, life-per-kill, strength,
  dexterity, to-hit, magic find, and the thorns line - and in passing this fixed an existing gap:
  armor dexterity was applied but never printed.
- **The table stopped being positional.** The old rows were 19 bare numbers each, and the struct
  carried a warning that inserting a field would silently re-read every trailing number as
  something else. Designated initializers now: a row names what it sets, everything else defaults
  to 0, and adding a field cannot shift anyone.

The runes are untouched (they were already on D2's sheet, from the 2026-08-16 directive) - just
re-expressed in the designated-initializer style.

## Tests

Three updated to the new anchors, one added:
- `RubyFollowsItsDiabloTwoColumn` (was `...FireResistInArmor` - the armor column is LIFE now).
- `QualityScalesEffectsAndNormalIsTheTunedRow` - ruby anchor 8-12, amethyst armor strength.
- `QualityScalesEffectsOnceNotTwice` - the armor half moved from resist to the life field, which
  also pins that the <<6 fixed-point conversion happens AFTER the quality application.
- **New: `EveryGemCarriesItsDiabloTwoIdentity`** - one signature stat per type per host, all
  seven, including the skull's kill-leech through a real socketed weapon (Perfect 4, Chipped
  floors at 1, helm host 0).

## Verification

Debug build clean at 1.7.70; suite **445 of 447** - the two failures are the standing baseline
pair. Existing socketed items pick the new numbers up automatically: sockets store the gem's item
index and the effects resolve from the table on every recalc, so nothing in any save needed
migrating.

**To see it in game:** socket a Perfect Ruby in armor (+38 life, tooltip agrees), a Topaz in a
helm (+magic find line), a Skull in a weapon (life ticks up on kills) and in a shield (melee
attackers bleed), a Sapphire anywhere (mana).
