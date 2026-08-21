# Sets You Are Collecting, Variants That Know Where They Are

**Version:** v1.9.10 → v1.9.11
**Date:** 2026-08-22
**Tests:** 495/497 (the two standing baseline failures)

Two Pipeline rows, both Small, both about the same failure: a system that worked but said nothing.

## 1. Sets drop as sets

`TrySpawnNamedSetPiece` rolled 3%, then picked **uniformly** across every eligible piece of all
fifteen sets — around ninety of them at depth. Holding five of the Ashen Saint's six made the sixth
no likelier than a piece of a set you had never seen. Completing a ladder was attrition, and the
cumulative bonus ladder — the entire reason a named set exists over a good rare — was something you
finished by accident or not at all.

The pick is now weighted. A piece is worth more when you already hold pieces of **its** set and do
not hold **this** one:

```
weight = 1                       // nothing held, or you already hold this piece
weight = 1 + held * 3            // you are collecting this set and this piece is missing
```

Two decisions inside that:

- **Held means held ANYWHERE** — worn, carried, or in the stash. `WornSetPieces` already existed and
  was the wrong question: bonuses are for wearing, but collecting happens in the chest. Counting
  only worn pieces would bias against exactly the player this helps — the one hoarding four pieces
  they cannot equip yet. New `IsSetPieceHeld` / `HeldSetPieces` in `item_sets.cpp`.
- **A piece you hold falls to the floor weight, never to zero.** Duplicates are legitimate, and a
  zero would make a piece sold by mistake unobtainable for as long as the game believed you had it.

`HeldSetPieces` is computed once per **set**, not per piece — it walks the backpack and the whole
stash, so calling it in the inner loop would be quadratic in stash size on every drop.

The weight rule itself was lifted out of the drop hook into `SetPieceDropWeight(heldInSet,
holdsThisPiece)` so it can be read and tested without spawning a monster.

## 2. Variants know which floor they are on

The four variants shipped at v1.9.7 on a flat 15% **everywhere**, so a Cathedral skeleton and a Hell
knight drew from the same list. "Ashen" was texture, not information.

Each dungeon type now offers a subset:

| Dungeon | Roster | Why |
|---|---|---|
| Cathedral | Hollow, Feral | The two body variants. Teaches the idea without an elemental resistance a floor-two character may have no answer to. |
| Catacombs | Hollow, Feral, Stormtouched | Lightning is the first resistance worth planning around. |
| Caves | Feral, Ashen | The lava tileset; fire-hardened belongs where fire lives. Dropping Hollow makes the Caves read as faster, not merely tougher. |
| Hell | all four | The deepest floors are where the player is expected to have answers. |
| Crypt | Hollow, Stormtouched | The drained and the charged. No fire in a Crypt. |
| Nest | Feral, Ashen | A hive over lava. |
| **Town** | **none** | Load-bearing, not tidy. An Ashen Griswold is nonsense. |

`VariantForSeed(seed, dungeon)` is the whole rule with nothing implicit — it reads no globals, which
is what makes the roster testable without building a level. `VariantOf(monster)` is the same
function with `leveltype` and the unique/champion exclusions supplied.

## Tests

Both features shipped with tests, and **the variants had none at all before today** — v1.9.7 went
out untested.

`NamedSetDropsLeanTowardTheSetYouAreCollecting` pins the weight rule in both directions and builds
the same piece three times over — worn, carried, stashed — because "held anywhere" is the part that
would silently half-work. It also pins that a duplicate is one piece of progress, not two.

`MonsterVariantRostersArePerDungeonAndComplete` pins that town yields None for 500 seeds, that no
roster repeats an entry (a repeat silently doubles that variant's share), that **every variant in
the enum appears in some roster** — one that appears in none is dead code wearing a name — that the
Cathedral offers no elemental variant, and that the rate is still 15% ± 1 over 10,000 seeds. It also
checks 2,000 seeds per dungeon never produce a variant outside that dungeon's own roster, which is
the assertion that fails if the draw ever stops going through the table.

## What to look at in game

- Find one piece of any named set, stash it, then keep killing. The rest of that set should start
  turning up noticeably more often than pieces of sets you have none of.
- Compare a Cathedral run to a Hell run: the Cathedral should only ever show Hollow and Feral.
- Town should never show a variant.
