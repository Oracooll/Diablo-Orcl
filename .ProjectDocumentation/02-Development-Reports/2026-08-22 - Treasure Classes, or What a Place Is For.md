# Treasure Classes, or What a Place Is For

**Version:** v1.9.12 → v1.9.13
**Date:** 2026-08-22
**Tests:** 497/499 (the two standing baseline failures)

## The problem

Every drop hook in this fork was flat. The socketable roll was 3% gem, 1% charm, 2% rune, 1% jewel
on **every monster in the game**, and the named-set roll was 3% everywhere.

Depth changed which items were *eligible* — `BandedQlvl` keeps a Radiant jewel out of the Church —
but it never changed what a floor was **for**. Two floors at the same area level were
interchangeable, and the only reason to prefer one place over another was how fast you could clear
it. That is a dungeon with depths but no places.

## The tables

| Where | Class | Socketable | Set | Gem | Rune | Jewel | Charm |
|---|---|---|---|---|---|---|---|
| Cathedral | The Cathedral's Offering | 7% | 3% | **60%** | 25% | — | 15% |
| Catacombs | The Catacombs' Reliquary | 8% | 3% | 25% | 20% | 10% | **45%** |
| Caves | The Caves' Forge | 9% | 4% | 15% | **55%** | 10% | 20% |
| Hell | Hell's Own Hoard | 11% | 5% | 10% | 30% | **45%** | 15% |
| Nest | The Nest's Clutch | 9% | 4% | 10% | 15% | 35% | **40%** |
| Crypt | Na-Krul's Vault | 11% | 5% | 10% | **50%** | 30% | 10% |
| Town | Nothing at all | 0% | 0% | — | — | — | — |

The family columns are **shares of the socketable draw**; the first two are chances per kill.

Each zone gets a clear majority rather than a gentle tilt. The rejected alternative was
30/25/25/20 everywhere, which reads as noise: nobody farms a 5% edge they cannot feel, and a
treasure class nobody can feel is a table that exists for its own sake.

The reasoning per zone:

- **Cathedral** is the gem floor — gems are the socketable a new character can actually use: small
  broad stats, a cheap ladder, no runeword to look up first. **Jewels are zero here, not rare.** The
  shallow end should not be teaching two socket economies at once.
- **Catacombs** is the charm floor, and also where the extra backpack tabs start to matter; charms
  are the one family bounded by inventory rather than by sockets.
- **Caves** is the rune floor. The runeword engine is the deepest change in the game to farm for,
  and it is where the Anvil sits.
- **Hell** is the jewel floor with runes behind it — both permanent unconditional power, and Hell is
  where a character has the sockets to spend them.
- **Crypt** is runes again, harder. Na-Krul's vault should rival the Caves.
- **Nest** is charms and jewels, the two families that need no host item to pay off.

## Bosses: a multiplier, not a table

An ordinary monster pays **1×**, a champion **2×**, a unique **4×**, applied to both the socketable
and the set-piece chance. So a champion in the Caves is a better rune chance than a champion
anywhere else *and* better than an ordinary Caves monster — from one number, not a second table.

Deliberately **not a guarantee**. A boss that always drops a socketable turns farming into a loop
with no roll left in it, and the moment that is true the class stops being a reason to go somewhere.

## What a treasure class does not touch, and cannot

**The base item pool is unchanged.** `GetItemIndexForDroppableItem` is replayed from an item's seed
by `UnPackItem` to recover its index, which makes that pool part of the save format — the constraint
recorded on every Oracool drop hook. A zone that altered which swords were in it would transform
every existing item in every existing save.

So a class redistributes only the hooks that were already additive. That is enough to make the Caves
the place to farm runes and Hell the place to farm jewels, and it costs nothing in the save.

**The RNG stream is unchanged too.** Still one `GenerateRnd` for whether-anything-drops and one for
which-family, exactly as before, so no zone consumes the level's stream at a different rate than
another. What changed is only what the draws are taken against.

## Tests

`TreasureClassesGiveEveryZoneSomethingOfItsOwn` walks each table's entire weight range through the
real `FamilyForRoll` and counts where every roll lands — the actual distribution, not a restatement
of the table. It then asserts the part that would rot silently: that every zone's best family is
**at least 35% of its draw** (a tilt is not a treasure class), that the six zones favour at least
three *different* families between them, that the Cathedral's jewel weight is zero, that Hell pays
more often than the Cathedral, that town pays nothing, and that no two class names collide.

Six zones that all favoured gems would pass every other assertion and still leave every floor
interchangeable, which is why the majority-diversity check is there.

`TreasureBonusRewardsChampionsAndUniques` pins 1/2/4, and specifically that a unique which *also*
carries a lesser affix still gets the unique's four — the order of the two tests inside
`TreasureBonusFor` is the whole of that, and reversing them is a one-character change nothing else
would notice.

## Wiki

The sockets page now renders the whole table, parsed out of `Classes[]` **and its dungeon switch** —
the switch is read rather than assumed, because "row 4 is Hell" is exactly the correspondence that
survives a reorder in the source and not on the page. The old flat per-family drop row is gone; it
read four constants that no longer exist.

## What to look at in game

- Clear a Caves floor and a Catacombs floor of similar depth. The Caves should feel like runes; the
  Catacombs like charms.
- Kill champions specifically — they should be visibly worth crossing the room for now.
- The Cathedral should never drop a jewel at all.
