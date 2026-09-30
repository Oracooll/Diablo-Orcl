# 2026-09-30 - Whole-code audit, round 9 (v1.12.234)

**Date:** 2026-09-30. Debug only. The audit continues at the user's word.

Round 9 ran six read-only tracks:
1. rifts, second pass;
2. the Abilities window and tree investment;
3. the Hellfire content;
4. the waypoint system;
5. gold and the economy;
6. missile graphics and the sprite machinery.

Every finding was verified against the code first.

## Fixed: mana costs

- **Book-learned Mana, the Magi, the Jester and the five Runes cost the whole mana pool.** They have had books since
  v1.5.49 but kept vanilla's cost of 255. That value was a sentinel meaning "staff or item only", which
  `GetManaAmount` prices at the entire pool.
  - Each book cast emptied the orb.
  - The Magi left less mana than it gave.

  They now carry real costs: Mana 10, the Magi 60, the Jester 25, the Runes 20 each, with their per-rank reductions
  kept. These numbers are a first cut, for the user to correct.

## Fixed: rifts

- **The way home sat inside the loot pile.** The loot scatters over the 3x3 around the guardian's corpse, and the
  return trigger was put on a neighbour tile. A pickup walk that stopped on it sent the hero home, and in a Guardian
  Rift the rest of the pile and the next keystone were lost. The trigger now goes on an item-free tile outside that
  ring.
- **A cleared Guardian Rift's pile is protected.** Opening another rift before walking out through its exit wiped the
  keystone and the six items left for a hero who died or ran out of room.
- **Guardian tiers past a difficulty's 16 rungs** wrapped back to floor-1 monsters. For example, a tier-17 key in Normal
  drew zombies at 148%. Tiers above the block now stay on rung 16 and scale 3% a tier, as `rift.h` promises. A low key
  in a higher difficulty takes rung 1.
- **The keystone's refusal in town** no longer adds a contradictory "only turns in town" line.

## Fixed: the economy

- **A unique bought from Griswold's Unique shelf kept the shelf price (20x) as its value.** It sold for five times a
  found copy and repaired at twenty times. It now takes its own value on purchase.
- **Vanilla uniques are valued at their base tier.** `GetUniqueItem` wrote the row's untiered value after the tier had
  been applied. The result: a Torment unique sold for less once identified, and repaired at the Normal price.
  - The new helper is `oracool::ScaleValueForBaseTier`.

## Fixed: waypoints

- **Level 16's sigil** can no longer stand inside Diablo's three lever-sealed quadrants. It was unreachable on foot, and
  arriving by waypoint skipped the levers.
- **No travel while dying.** A click during the death animation carried the corpse to the chosen floor.
- **Telekinesis cannot light a sigil**; it did so from any distance and closed the inventory.
- **The `givewp` debug command** covers the Hellfire act (17-24).

## Fixed: the Abilities window

- **Passive slot 4's right third** takes hover and clicks. The gates stopped at x 292; the slot runs to 309.
- **A refund keeps the hero's life.** Taking Endurance or a Vitality stat away at low life killed him, the same class
  of bug round 7 fixed.
- **A rank refused by its level requirement** now says so ("... rank 2 needs level 7.").

## Fixed: Hellfire and graphics

- **The Jester in town** no longer rolls a portal or a teleport. The town portal it opened led to the last portal's
  destination, which `Portals[]` keeps after closing.
- **Ground runes** ignore the hero's army, his companions, and townspeople's ids in town.
- **Berserk on a Luminous monster** resizes its light instead of orphaning it.
- **The Necromancer's poison Skeletal Mage** shot invisible bolts on any floor without acid beasts. The acid sheets are
  monster-owned and loaded only with those monsters. `AddMissile` now loads a monster-owned sheet on first use.
- **Giant, Runt and Colossal chargers** keep their size during the charge; the charge drew the unscaled animation.
- **The Cold pack's sheets and the Blessed Hammer spin** are `PngOnly`. No `.cl2` stands behind them, so a missing or
  rejected PNG ended the game at every level load.

## Not changed (open)

- **Keystones on extra backpack pages** are not seen by the Monument menu.
- **Cleanse** does not escalate the Remove price.
- **Crafting may mint sale value** (Torment crafts, Mystic rerolls). This is flagged, not measured.
- **Ordinary Giants and Runts** change size when their body lies down (the corpse table has no per-size slots).
- **Old saves** over a row's rank cap are not trimmed. V1 always starts a New Game.
- **Adria's respec** at low life leaves the hero on 1 life in town.

## Tests

No new tests; ctest 884/884.

Debug build and ctest: 884/884. The Debug `diablo.ini` was unchanged through the run. Nothing seen in play.
