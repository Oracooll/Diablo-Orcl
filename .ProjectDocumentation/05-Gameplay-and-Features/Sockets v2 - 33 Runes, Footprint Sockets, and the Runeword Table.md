---
date: 2026-08-19
area: Sockets v2 - design
status: proposed
---

# Sockets v2 - 33 Runes, Footprint Sockets, and the Runeword Table

The user's nine-point socket directive, turned into tables before any of it is built. Points 1-4,
6, 7 and 9 are specified enough to implement; 5 and 8 wait on Levski's Roar.

## 1-3. What can be socketed, and how many

**The rule replaces three separate restrictions with one measurement:** an item's socket cap is the
number of 28x28 inventory cells it occupies.

| Footprint | Cap | Typical hosts |
|---|---|---|
| 1x1 | 1 | rings, amulets |
| 1x2 / 2x1 | 2 | gloves, boots, belts, bracers, small weapons |
| 2x2 | 4 | shields, helms, shoulders, most one-handers |
| 2x3 | 6 | body armour, two-handed weapons, staves |

Consequences:

- `Item::MaxItemSockets` grows **3 to 6**, and the item save extension grows 6 bytes with it
  (`1 + MaxItemSockets * 2`). Old characters lose their sockets rather than misparse - the fixed
  item record changes size, so this is a format break, and it takes the usual version bump.
- The socket **roll** stays where it is: on the drop paths, after `SetupAllItems`, never inside the
  seeded replay. That is the drop-pool lesson and it does not change here.
- The roll picks a count from 1 to the cap, weighted toward the low end so a full six stays an
  event: **60 / 25 / 8 / 4 / 2 / 1** for one through six, truncated and renormalised at the cap.

**All four base tiers may roll sockets.** The old rule excluded anything with a tier, which made
Nightmare/Hell/Torment bases strictly worse raw material than a Normal one - backwards, since the
deeper base is the better host. Tier and sockets are independent axes, as tier and quality already
are.

## 4. Jewelry

Rings and amulets have no basic versions to roll on, so they take sockets on **magic, rare, unique
and primal** rolls instead - one socket, since they are 1x1. Everything else keeps the basic-only
rule: a magic sword is already a rolled item, and letting it socket too would make the white sword
pointless.

| Host | Qualities that can roll sockets |
|---|---|
| Rings, amulets | magic, rare, unique, primal |
| Everything else | basic only |

## 5, 8. Extraction and the crafting move - BLOCKED

Insertion stops being permanent: a Levski's Roar recipe pulls the stones back out, returning both
the stone and the host item. Gem and rune crafting moves out of the current crafting window and
into Levski's Roar with it.

**Neither can be built yet.** Levski's Roar does not exist - no monument, no UI, and its
Kanai's-Cube-inspired recipe table was to be approved before building. Extraction is one recipe on
that table; this design assumes it lands there rather than in the interim crafting window.

## 6. The 33 runes

All of Diablo II's runes, in D2's own order, cut from `runes.png` (11x3 green-screen grid). Runes
have no quality ladder, so each is one row of numbers.

Where D2 leans on a channel this engine lacks - cold, poison, crushing blow, open wounds, deadly
strike, freeze - the effect moves to the nearest channel that exists. The engine's real vocabulary
is `ItemBonusTotals` (resists, life, mana, armour, the four attributes, to-hit, damage, damage
reduction, light radius, magic/gold find, spell levels) plus the `ItemSpecialEffect` flags
(Knockback, the attack-speed and hit-recovery ladders, FastBlock, Thorns, StealLife5, StealMana5,
TripleDemonDamage, HalfTrapDamage).

| # | Rune | In a weapon | In armour | In a shield | Adaptation |
|---|---|---|---|---|---|
| 1 | El | +5% to hit | +15 armour | +15 armour | D2's +50 AR at its own 10:1 convention; +1 light radius everywhere |
| 2 | Eld | +7% damage | +10 life | +7 armour, fast block | Damage vs undead has no channel |
| 3 | Tir | +2 mana per kill | +2 mana per kill | +2 mana per kill | - |
| 4 | Nef | knockback | damage taken -3 | damage taken -3 | D2's -30 missile damage becomes flat DR |
| 5 | Eth | +10% to hit | +10 mana | +10 mana | "-25% target defence" is to-hit here |
| 6 | Ith | +9 damage | +8 mana | +8 mana | - |
| 7 | Tal | +5 damage | +30% magic resist | +35% magic resist | Poison has no channel; poison resist becomes magic |
| 8 | Ral | 5-30 fire damage | +30% fire resist | +35% fire resist | - |
| 9 | Ort | 1-50 lightning damage | +30% lightning resist | +35% lightning resist | - |
| 10 | Thul | +8 damage | +30% magic resist | +35% magic resist | No cold at all in this engine |
| 11 | Amn | life steal | thorns | thorns | D2's 7% steal rides the engine's own StealLife5 flag |
| 12 | Sol | +9 damage | damage taken -7 | damage taken -7 | Min-only damage cannot cross max here |
| 13 | Shael | faster attack | faster hit recovery | fast block | Straight onto the engine's own speed ladders |
| 14 | Dol | +4 life per kill | +10 life | +10 life | "Monster flees" has no channel |
| 15 | Hel | requirements -20% | requirements -20% | requirements -20% | Acts on the item's own requirement fields |
| 16 | Io | +10 vitality | +10 vitality | +10 vitality | - |
| 17 | Lum | +10 magic | +10 magic | +10 magic | D2's energy is magic here |
| 18 | Ko | +10 dexterity | +10 dexterity | +10 dexterity | - |
| 19 | Fal | +10 strength | +10 strength | +10 strength | - |
| 20 | Lem | +75% gold find | +75% gold find | +75% gold find | - |
| 21 | Pul | triple demon damage | +25 armour | +25 armour | The engine's own demon flag carries D2's intent |
| 22 | Um | +10 damage | +15% all resists | +22% all resists | Open wounds has no channel |
| 23 | Mal | +12 damage | damage taken -7 | damage taken -7 | "Prevent monster heal" has no channel |
| 24 | Ist | +30% magic find | +30% magic find | +30% magic find | - |
| 25 | Gul | +20% to hit | +5% magic resist | +5% magic resist | - |
| 26 | Vex | mana steal | +5% fire resist | +5% fire resist | StealMana5 carries D2's 7% |
| 27 | Ohm | +50% damage | +5% magic resist | +5% magic resist | - |
| 28 | Lo | +20% damage | +5% lightning resist | +5% lightning resist | Deadly strike becomes flat damage % |
| 29 | Sur | +20 mana | +50 mana | +50 mana | Blind has no channel |
| 30 | Ber | +25% damage | damage taken -8 | damage taken -8 | Crushing blow becomes damage % |
| 31 | Jah | +25% to hit | +50 life | +50 life | "Ignore target defence" is to-hit here |
| 32 | Cham | +15 damage | +10% magic resist | +10% magic resist | Freeze has no channel |
| 33 | Zod | indestructible | indestructible | indestructible | Sets the host's durability field |

Two runes act on the **item** rather than the totals, which is why they are called out: Hel reduces
the host's own strength/magic/dexterity requirements, and Zod sets its durability to
indestructible. Both are applied where the socket contents are read, not through the provider walk.

**Depth.** Runes drop in D2's own three bands, gated by the floor's item level: El-Amn early,
Sol-Um mid, Mal-Zod deep. The deepest runes should be Torment-depth finds.

## 7. The runeword table

A few hundred words, covering every non-jewelry host type. Hand-authoring that many is how a table
becomes inconsistent, so the table is **generated from a scheme and then tuned**, the same way the
73 item-set rungs were.

The scheme, per word:

- **Host**: one of the ten non-jewelry types - sword, axe, mace, bow, staff, shield, body armour,
  helm, and the worn accessories (shoulders, bracers, gloves, belt, legs, boots) grouped as armour.
- **Length**: 2 to 6 runes, which must equal the host's socket count exactly - so a six-rune word
  needs a 2x3 host, and the long words are automatically the rare ones.
- **Rune depth**: the word's tier is its deepest rune. A word made of El and Tir is an early-game
  word; one containing Zod is an endgame chase.
- **Grant**: a bonus package scaled to the word's total rune depth, so a longer or deeper word is
  always worth more than the sum of its runes - which is the entire reason to build a word rather
  than socket the runes loose.
- **Name**: D2's own names where a word matches D2's recipe, invented names elsewhere, drawn from
  the same register.

Every rune's description continues to list the words it belongs to - with several hundred words
that list needs a cap and a "and N more" line, or the description will not fit the panel.

## 9. Primal is orange, not gold - FIXED

The wiki's quality table claimed gold for Primal and for Buffed Unique. In the code
(`Item::getTextColor`) Primal is **orange** - true cyan was never available, since UiFlags is a
full 32-bit flag enum with no free bit and no cyan `.trn` ships in the game data - and Buffed
Unique is **whitegold**. Only the vanilla unique table is gold. Corrected on the Quality and
affixes page, with two new tag colours to render it.

## 10. Missing

The user's list ends at an empty point 10. Nothing is assumed for it.
