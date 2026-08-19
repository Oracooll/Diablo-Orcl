# Second Audit of the 1.8.x Loot Mechanics

**Version:** 1.8.6
**Date:** 2026-08-19
**Tests:** 445/447 (the two standing baseline failures)

A second pass over everything from 1.8.0 to 1.8.5, run after the first audit found a claimed fix that
had never applied. This one starts by verifying the previous fixes actually exist in the code.

## Verification of the previous round

| Fix | Claimed in | Verified now |
|---|---|---|
| Tier word no longer prepended to item names | 1.8.1, again 1.8.5 | **Present** - 0 occurrences of the name write |
| Area level suppressed in town | 1.8.4 | **Present** |
| Quality rolls guarded by `> 0` | 1.8.4 | **Present** - all three |

The 1.8.1 claim was the false one; 1.8.5 removed it for real.

## A1 - FIXED (serious): the tier could hang a shop

`SpawnOnePremium` generates in a retry loop that discards any item priced over the vendor cap, and in
Diablo - not Hellfire - **that loop is unbounded**. The engine's own comment says so:

```cpp
const bool unlimited = !gbIsHellfire; // TODO: This could lead to an infinite loop if a suitable item can never be generated
```

Torment multiplies item value by thirty. Tiering the item and letting the loop reject it therefore
did two bad things: it skewed shop stock toward the cheapest bases (the only ones whose x30 stays
under the cap), and it pushed an already-unbounded loop toward the failure its own TODO predicts.

`ApplyVendorTier` now takes the vendor's cap and checks the tier's value **before** applying it. Over
cap, the item simply stays Normal - the item, the cap and the loop all survive. Every vendor passes
its own cap: `MaxVendorValue` for the smith and witch, `MaxBoyValue`/`MaxBoyValueHf` for Wirt, and 0
(meaning "no cap") for the premium path when it is generating with price limits ignored.

## A2 - FIXED: shop books were gated by an estimate

`GetBookSpell` reads the item's ilvl to decide which spell bands this depth may offer, and it runs
INSIDE `GetItemAttrs`. The vendor ilvl was being stamped after that call, so shop books still used
the old `lvl * 2` fallback.

`StampVendorItemLevel` now runs before `GetItemAttrs` at all five vendor sites. Shop books obey the
same band gate as dungeon books: at Adria's Hell ilvl of 54-64 the band-30 books become available,
and not one difficulty sooner. That closes F3 from the first audit.

## A3 - checked and correct

- `VendorItemLevel` maps difficulty to block 0/1/2/3 exactly (verified against `AreaLevel(1, d)`).
- `ApplyVendorTier` sits before `GetItemBonus` in all four paths that have one, so affixes roll on
  top of the tiered base rather than being scaled by it.
- The vendor tier is applied on fresh stock only, never in the `Recreate*` twins - matching the
  dungeon path, which is what keeps the pack tests green.
- The two hashes (is-it-tiered, which-tier) use different salts, so they are independent.
- Requirement scaling (x2.2 at Torment) cannot lock a class out: the highest authored strength
  requirement is well inside every class's maximum.

## A4 - known and unchanged

Adria's four PINNED books skip the vendor path entirely and still use the `lvl * 2` estimate. They
are fixed indices with fixed spells, so the estimate cannot produce a wrong band for them - but they
are the one remaining place where an item's ilvl is inferred rather than stamped.

## Files

- `Source/oracool/item_tiers.h` / `.cpp` - the cap parameter, `StampVendorItemLevel`
- `Source/items.cpp` - five vendor sites reordered and given their caps
