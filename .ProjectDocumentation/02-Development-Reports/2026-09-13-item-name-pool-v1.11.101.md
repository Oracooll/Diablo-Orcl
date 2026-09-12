# Items are named from a pool, generated on the fly

2026-09-13 — v1.11.101

## Why

> "we must generate a pool of name affixes to make items sound more interesting and generate their
> names on the fly in real time."

This follows directly from v1.11.100. D2 builds a magic item's name **out of its affixes** — "Garnet
Cap of the Tiger" — which is precisely why a magic item can only ever carry one prefix and one
suffix: there is no way to write a four-affix item's name that way. Once the budget went flat and
D3-style, the naming had to follow.

Tiered items had a second problem: `"{TierLabel} {BaseName}"` gave "Rare Cap", "Primal Great Helm" —
readable, but it says nothing and repeats endlessly across a run.

## The pool

`Source/oracool/item_names.{h,cpp}`: 64 adjectives × 64 nouns = **4096 names**, rendered as
"Adjective Noun" — *Tarnished Doom*, *Smoldering Crest*, *Rotting Bane*, *Sundered Tomb*.

Both tables are `N_()`-marked and run through `_()` at the point of use, like every other name table
here.

## The part that actually matters: it is DERIVED, never rolled

The name is hashed out of `item._iSeed` and **consumes no randomness at all**.

That is not a stylistic choice. `SetupAllItems` is replayed from a stored seed to rebuild every
dungeon item on load, and one extra `GenerateRnd` inside that replay shifts every roll after it —
which is how a "Helm of harmony" once came back as a "Great Helm of haste". Hashing the seed makes
the name perfectly reproducible and costs the replay nothing.

The hash is splitmix32's finaliser. A plain `seed % 64` would not do: `AdvanceRndSeed` hands out
related seeds to items dropped together, so taking the low bits would march the first word down the
table and name a whole room of loot "Ashen …", then "Bleak …".

## What it replaced, and what it did not

| Item | Before | After |
|---|---|---|
| Magic | "Garnet Cap of the Tiger" | pool name |
| Rare / Buffed Unique / Primal | "Rare Cap" | pool name |
| Unique | hand-authored | **unchanged** |
| Set piece | hand-authored | **unchanged** |
| Runeword | the word's name | **unchanged** |
| Plain / unidentified | base name | **unchanged** |

Uniques and set pieces are hand-authored named objects ("Vhal's Blackened Halo") and a pool would
erase deliberate design work.

## A regression this caused, and its fix

`drop {name}` (`DebugSpawnItem`) matched **only** `_iIName`. That worked because a magic item's name
contained its base type — "helm" is inside "Amber Helm of harmony". Pool names carry the base type
nowhere, so `drop helm` would have found nothing but plain items.

New `DebugItemNameMatches` matches the item's own name **or** its base type's name, and both search
loops use it. `giverare` and friends were never affected: `DebugSpawnTieredItem` filters on
`_iOracoolTier`, not on the name.

## Tests

`OracoolItemNames.NamesAreTwoWordsDerivedFromTheSeedAndNeverRolled` covers the five things that
could quietly break:

1. **Determinism** — same seed gives the same name after the RNG stream has been moved underneath it.
2. **Zero RNG consumption** — a seeded draw is identical with and without a name generated in between.
3. **Shape** — exactly two non-empty words, short enough for `_iIName[64]`.
4. **No clustering** — 64 *consecutive* seeds must give ≥60 distinct names and ≥30 distinct first
   words, which is the assertion that would fail if the hash were removed.
5. **Reachability** — every index of both tables is produced over 200k seeds, so a table longer than
   the generator's modulus cannot silently strand its tail.

The **pack corpus is the end-to-end proof**: 82 golden names were regenerated, and the suite passing
means every one of them round-trips through pack → unpack → seed replay unchanged.

Three tier tests in `items_test.cpp` asserted the old naming (`find("Rare")`) and now assert the
two-word shape instead.

## Verification

- Debug: **730 tests, 0 failed.**
- Release built and linked; `DiabloOrcl RTM\DiabloOrcl.exe` refreshed.
- No asset changed, so no MPQ repack.

## Worth knowing

- Items already in a stash keep the names they have — this changes generation, not stored items.
- `UpdateHellfireFlag` detects vanilla-Hellfire items by replaying names and comparing; with pool
  names that comparison no longer matches for newly generated items. It only ever mattered for real
  vanilla Hellfire saves, which this fork does not load.
