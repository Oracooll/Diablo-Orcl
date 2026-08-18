# Four Base Tiers

**Version:** 1.8.1
**Date:** 2026-08-19
**Tests:** 445/447 (the two standing baseline failures)

D2's Normal/Exceptional/Elite, one rung longer, named for the four difficulties.

## Two axes, kept separate

- **Quality** - basic, magic, rare, set, unique, primal. What was rolled ON the item.
- **Base tier** - Normal, Nightmare, Hell, Torment. What the item IS underneath.

Every quality appears on every tier. A Torment basic and a Normal primal are both ordinary finds, and
nothing ever stops dropping - which is the ground the salvage economy will stand on.

## Not 670 new rows

The obvious reading of D2 is three more copies of every base in `AllItemsList`: ~670 rows, an enum
that triples, and every `_item_indexes` bound in the codebase to re-audit. This does the same job
with a **byte on the item**: the base row stays one row, and the tier scales its numbers as the item
is generated. No save-index churn, no new sprites, no table-and-enum drift, and set and unique items
get tiers for free instead of needing a copy of themselves per tier.

## The numbers

| Tier | ilvl band | Damage / AC | Requirements | Durability | Value | Popup colour |
|---|---|---|---|---|---|---|
| Normal | 1-24 | x1.0 | x1.0 | x1.0 | x1 | White |
| Nightmare | 25-48 | x1.8 | x1.4 | x1.25 | x4 | Blue |
| Hell | 49-72 | x2.9 | x1.8 | x1.5 | x12 | Yellow |
| Torment | 73-96 | x4.2 | x2.2 | x1.75 | x30 | Gold |

One tier per 24-floor difficulty block of the area ladder, so the rule is one sentence: **the tier
you find is the difficulty you are in.**

Weighting, deepest-first among the tiers a level allows: **60 / 25 / 10 / 5**. At ilvl 1-24 that
collapses to Normal alone; by Torment all four are in play, and the tail is what keeps white items
falling for salvage.

## The tier is hashed, not rolled

`TierForItem(ilvl, seed)` derives the tier by hashing the seed rather than drawing from the seeded
stream. Every generated item is reconstructible from its seed - `RecreateItem` and `UnPackItem`
replay the whole sequence - so a draw inserted here would shift every affix roll after it and change
what existing seeds produce. The pack tests caught exactly that when the first version used
`GenerateRnd`.

## Three things the tests caught

1. **The seeded-stream shift** above - fixed by hashing.
2. **A `string_view` array overload** - the name prefix copied into a `char[64]` and handed the array
   to `string_view`, which took all 64 bytes, NULs included. "Brutal Long Sword of gore" came back as
   "Brutal Sword of gore".
3. **`recreate` does not mean recreate.** That parameter is set from `CF_UNIQUE` and means "this is a
   unique"; the actual reconstruction flag is `allowTieredRoll`. Gating on the wrong one left the
   recreate path tiering items it had to reproduce exactly. The tier now applies on fresh generation
   only - single-player never takes the recreate path, since `SaveItem` stores every stat.

The name prefix idea was dropped with (2): the engine composes a magic item's name against a width
budget and falls back to the base's SHORT name when it overflows, so "Jagged" cost the item its base
name. The tier line in the description carries it instead, in its own colour, which is where the
request put it.

## Files

- `Source/oracool/item_tiers.h` / `.cpp` (new), `Source/CMakeLists.txt`
- `Source/items.h` - `_iOracoolBaseTier`
- `Source/items.cpp` - the application point and the description line
- `Source/loadsave.cpp` - item format version 6

## To look at in game

Kill things at different depths and read the Tier line: white in Normal, and blue/yellow/gold
appearing as the difficulties open. A Nightmare-tier sword should hit noticeably harder than its
Normal twin and demand more strength for it.
