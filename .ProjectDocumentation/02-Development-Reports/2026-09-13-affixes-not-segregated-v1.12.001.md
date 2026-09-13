# Affixes are one list — no prefix/suffix segregation

2026-09-13 — v1.12.001 (item format 10)

## Why

> "i wanted to move to D3 model where affixes are not segregated into prefixes and suffixes. Did we do this migration
> of logic and if we did, i dont want affixes to still be segregated meaning i now want all possible combinations of
> affixes to be able to occur in magic/rare/unique/primal, including ONLY prefixes and ONLY suffixes for ALL affixes
> slots these items have."
>
> "i have told you many times - dont protect save formats. always aim for the more robust and optimized code."

## Where it stood

- **Drawing was unified at v1.11.105.** Every quality drew from one pool: the prefix table, the suffix table and
  OracoolPoolRows. Magic items already took any combination: their table affixes sit in the vanilla pair in either
  slot.
- **Tiered storage was still segregated.** `_iOracoolPrefixes[3]` + `_iOracoolSuffixes[3]`, each affix filed by the
  table it came from. So:
  - a Rare could not be four prefixes;
  - a Buffed Unique could not be five and one;
  - a Primal was always three and three.

  It had been kept to spare the item save format.

A first pass this session kept both arrays and treated them as six anonymous slots, again to avoid a format bump.
The user rejected that kind of compromise outright, so the model was replaced properly.

## The model now

- **`Item::_iOracoolAffixes[MaxOracoolAffixes = 6]` + `_iOracoolAffixCount`** — one list, in roll order. It holds
  every affix of a Rare, Buffed Unique or Primal, and a magic item's pool affixes (Movement Speed, Faster Cast). The
  prefix/suffix fields and `MaxOracoolAffixesPerSlot` are gone.
- **Which table an affix came from is a property of its power type.** No type appears in both tables (checked
  across both), so nothing needs to store it.
- **`GetTieredItemAffixes(..., int guaranteedAffixes, ...)`** — Rare 2, Buffed Unique 4, Primal 6, replacing a
  per-slot minimum that was doubled.
  - `apply` appends to the list whatever the source table.
  - `draw` offers every table while the list has room.

  All prefixes, all suffixes, or any mix can occur at every tier.
- **`RepairOracoolAffixesIfCorrupted`** walks the one list and repairs each affix against the table its type
  belongs to.
- **Readers** each walk one list: `OracoolAffixesUsed`, `RederiveFastCast`, the tooltip, the indestructible check,
  the shop product line and the loader's Movement Speed re-derivation.
- **Fixed-power uniques** (vanilla and the fork's 250) carry authored powers, not rolled affixes, and are
  unaffected.

## Save format

**`OracoolItemFormatVersion` 9 → 10.** The item extension record now writes one affix count and six affixes. It
used to write a prefix count, a suffix count and 3 + 3 affixes, so the record is one byte shorter. Hero items, the
stash and the inventory tabs saved at version 9 are refused by the exact version check, as for every earlier bump.
V1 always starts a New Game.

## Tests

- **New:** `PrimalItemTest.TieredAffixesAreNotSegregatedIntoPrefixesAndSuffixes`. Over 1,500 rolls each, it
  requires:
  - a Rare that is prefixes only;
  - a Rare that is suffixes only;
  - a Buffed Unique with more than three affixes from one table;
  - a Primal with more than three affixes from one table.
- **New:** `Item.RepairOracoolAffixesIfCorrupted_ReadsTheTableFromTheAffixType`. A suffix-table affix and a
  prefix-table affix side by side are each repaired against their own table.
- **Rewritten onto the one list:**
  - the Rare, Buffed Unique and Primal count, duplicate, perfect-roll, beneficial and stat-range tests (now
    checking both tables by type);
  - the repair tests;
  - the unified-pool test in items_test;
  - the Movement Speed and Faster Cast record tests in oracool_audit_test;
  - the item-extension round-trip tests in loadsave_test (six affixes).
- **Renamed:** `NeverExceedsThreePrefixesOrSuffixes` → `NeverExceedsSixAffixes`, and the Primal `...ThreePrefixes
  AndThreeSuffixes...` tests → `...ExactlySixAffixes...`.

## Docs

- `wiki/affixes.html`: the "three from each table" ceiling is gone, and the no-segregation and item-level-ceiling
  rules are stated.
- Memory: the Loot 2.0 note is updated, and a new feedback rule says never to protect save formats.
