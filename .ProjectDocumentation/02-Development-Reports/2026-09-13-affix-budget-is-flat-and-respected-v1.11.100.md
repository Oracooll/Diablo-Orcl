# The affix limit is one flat number per tier, and the drop tail now respects it

2026-09-13 — v1.11.100

## The report

A "Garnet Cap of the Tiger" reached the stash carrying **four** affixes on a two-affix tier:

```
Magic Helm · Tier: Normal · Item Level: 18
Resist Fire: +45%      <- prefix "Garnet"        (IPL_FIRERES 41-50)
Hit Points: +50        <- suffix "of the tiger"  (IPL_LIFE 41-50)
+24% movement speed    <- drop tail
+14% faster cast rate  <- drop tail
```

> "it is a helm that should not be possible to roll. we have a problem in our item generation
> system." — "we have hard limits on number of affixes per item tier. they must be respected."

## The cause

`TryAddMovementSpeedToDrop` and `TryAddFasterCastToDrop` both guarded on the same line:

```cpp
if (item._iOracoolSuffixCount >= Item::MaxOracoolAffixesPerSlot)   // 3
    return;
```

`MaxOracoolAffixesPerSlot` is the **storage** bound on the Oracool affix arrays — Primal's
allowance. It is not the item's tier budget.

A magic item keeps its rolled affixes in the *vanilla* `_iPrePower` / `_iSufPower` fields and leaves
those arrays empty, so the guard read **0** on every magic item and never saw what the item was
already carrying. Both rolls then ran back to back in `FinalizeFreshDrop` with no budget shared
between them, so an item could take both.

## The model: flat, D3-style

> "i would like to move to D3 style. We call all possible item bonuses affixes and an item can have
> any combo of them within its limit of affixes."

Diablo II splits affixes into prefixes and suffixes in two separate tables and lets a magic item
take one of each, a rare three of each. Diablo III has no such split — an item carries an unordered
list, and only the count is capped. This fork was built on the D2 model; the budget now moves to
the D3 one.

The prefix/suffix **storage** stays exactly as it is, because it is part of the save format and the
vanilla tables are what the seed replay walks. What stops being prefix-and-suffix is the **budget**:

| Tier | Affix limit | Where the number comes from |
|---|---|---|
| Plain | 0 | carries no rolled affix |
| Magic | 2 | the vanilla `_iPrePower` + `_iSufPower` pair |
| Rare | 4 | one per slot plus the 30% bonus on each |
| Buffed Unique | 6 | two per slot plus the 30% bonus on each |
| Primal | 6 | three per slot, no bonus - already at the cap |
| Set | 0 | six fixed powers, which are a list and not affixes |

These are the counts the existing rollers already produce, so nothing that could legally exist
before becomes illegal now.

Three helpers carry it, in `items.cpp` and exported for the test:

- `OracoolAffixBudget(item)` — the table above.
- `OracoolAffixesUsed(item)` — **both stores added together**, which is precisely what the old
  guard failed to do.
- `OracoolHasFreeAffixSlot(item)` — the budget *and* the storage bound.

## The rarity was also inverted

Both rolls used to skip any item with an Oracool tier, so a **Magic** helm could carry Movement
Speed while a **Rare** one never could. With the flat budget that exclusion is gone: tiered items
spend from their own larger allowance. Uniques and set pieces stay excluded — both are marked
`ITEM_QUALITY_UNIQUE`, and their stats are fixed lists rather than rolls.

A consequence worth knowing: a Rare with two slots free can now take *both* drop-tail affixes,
where a magic item never can. That is the D3 model behaving correctly — the limit is a count, and a
rare's count is larger.

## Test

`OracoolAffixBudget.TheDropTailNeverPushesAnItemPastItsTiersAffixBudget` rebuilds the reported helm
exactly (magic, both vanilla fields spent, `_iCreateInfo` 18) and rolls it 4000 times, because each
drop-tail affix is an 8% roll and one attempt proves nothing. It also covers the magic-item-with-one-
free-slot case, plain items, Rare, Primal at its cap, and set pieces.

**Proven by reverting.** Replacing the budget check with the old always-true behaviour:

```
oracool_audit_test.cpp(5141): error: Value of: OracoolHasFreeAffixSlot(reportedHelm())
oracool_audit_test.cpp(5149): error: the reported bug: a full magic item took a drop-tail affix anyway
```

## Verification

- Debug: **729 tests, 0 failed.**
- No asset changed, so no MPQ repack.

## Not addressed here

Existing saved items keep the affixes they already have — this changes generation, not items
already in a stash. The reported helm will stay as it is unless it is re-rolled.

Item value is also not recomputed for a drop-tail affix (`CalcOracoolTieredItemValue` runs during
rolling, before the tail). That predates this change and is unchanged by it.
