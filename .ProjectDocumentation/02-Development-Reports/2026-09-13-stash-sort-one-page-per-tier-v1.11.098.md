# SORT gives every quality tier its own page, and keeps a set together

2026-09-13 — v1.11.098

## What the user asked

> "when sorting the stash sort different tiers of items (basic, magic, rare, etc...) in different
> tabs. try sorting set items of same set close to each other."

## 1. One page per tier

Six tiers, weakest first, so the page number climbs with the name colour the player already reads:

| Page | Tier | Colour |
|---|---|---|
| 0 | Plain | white |
| 1 | Magic | blue |
| 2 | Rare | yellow |
| 3 | Set | green |
| 4 | Unique (vanilla and Buffed) | gold |
| 5 | Primal | orange |

`StashSortTierOf` reads `_iOracoolTier` **before** `_iMagical`, and that order is the whole
correctness argument: `MakeSetItem` marks a set piece `ITEM_QUALITY_UNIQUE`, so reading `_iMagical`
first would put every green item on the gold page. This is the same trap `Item::getTextColor`
documents, and the classification here is deliberately the same one it makes — if the name is
green, the piece is on the Set page, with no way for the two to disagree.

### Pages are handed out as tiers are placed, never reserved

`AutoPlaceItemInStash` begins its first-fit scan on `Stash.GetPage()` and only moves forward. So
seating the page before each tier's run *is* the mechanism: the run fills that page, spills onto
the next if it is longer than a page, and the tier after it starts on the first page still empty
(`FirstEmptyStashPageFrom`, extracted from `FirstEmptyStashPage`).

Reserving six pages up front would have been simpler and wrong. An absent tier would leave a hole,
and a hole is exactly what the material and consumable layouts below call "the first empty page" —
they would have moved into the gap and shared it with each other.

`SortStash` now also restores the viewed page to 0 at the end. It used to set the page once before
placing anything and leaving it there was free; the per-tier seating moves it as a side effect, so
without this the player would end up looking at whichever page the last tier happened to land on.

## 2. A set's pieces together

On the Set page the **set outranks the category**. Sorting by category first is what scatters a
set: every set's helm sorts with every other set's helm, and the sets come out combed into each
other down the page.

The key is `set index * 100 + piece index`, both from pointer arithmetic into `ItemSets` and
`ItemSetItems` — the identity of a set piece is its position in those tables, and there is no set
id field on `Item` to read. That also means a set's pieces come out in the order the set data
declares them (helm, torso, and down the body), which is the order the set's own tooltip lists.

Because the key is unique per piece, the footprint-first packing rule never applies within a set.
The set page packs a little less tightly than the others as a result. That is the trade the request
asks for.

A piece whose icon matches no definition sorts last rather than being dropped. It should be
unreachable — the Set tier is only ever applied by `MakeSetItem` — but sorting is not the place to
find out, and a stray green item is better misplaced than lost.

## Test

`OracoolAudit.SortGivesEveryQualityTierItsOwnPageAndKeepsASetTogether`. One item per tier plus four
set pieces from two sets, deposited **interleaved** (Ashen, Stormcrow, Ashen, Stormcrow) so the
result is the sort's work and not the deposit order.

**Both halves proven by reverting them separately.**

Disabling the per-tier page seating:

```
  Which is: 1
  Which is: 6
two quality tiers shared a page
```

All six tiers collapse onto page 0 — which is what SORT did before this change.

Disabling the set key:

```
set SET_ASHEN_SAINT resumes after SET_STORMCROW - the two sets are combed into each other
set SET_STORMCROW resumes after SET_ASHEN_SAINT - the two sets are combed into each other
```

ABAB, exactly as predicted.

## Verification

- Debug: **728 tests, 0 failed** (727 + the new one). Every existing sort test still passes,
  including the material page, the salvage row, the book rows and the stackable merge.
- Release built and linked; `DiabloOrcl RTM\DiabloOrcl.exe` refreshed.
- No asset changed, so no MPQ repack.

## What to look at in play

1. Fill the stash with mixed loot and hit SORT. Page 1 should be plain white items, page 2 blue,
   page 3 yellow, page 4 green, page 5 gold, page 6 orange — skipping any tier you own none of.
2. The green page: pieces of one set should sit in a run together, in body order, before the next
   set begins.
3. Runes/gems/salvage and the potions/scrolls page follow on the first pages after those, as
   before.
4. SORT should leave you looking at the first page.
