# Audit 7, findings 1-3: the shop tells the truth about its stock (v1.9.98)

**Date:** 2026-08-27
**Version:** 1.9.98
**Source:** `chatgpt audits/CHATGPT_DEEP_AUDIT_v1.9.97_2026-08-27.md`, findings 1-3 (all P2)

All three are the same shape: a piece of shop state that DESCRIBES the shelf disagreeing with what
the shelf actually is. Each was verified against the code before being fixed; all three were real.

## Finding 3 - a held stack was preflighted at one item's value and paid at the whole stack's

`StoreGoldFit` read the price off the item itself:

```cpp
bool StoreGoldFit(const Item &item, bool freesItemCells)
{
    int cost = item._iIvalue;
```

`_iIvalue` is the sale price only on a **storehold display copy** - the sell-list builder overwrites
it so the row can show a price (`StartSmithSell`). Everywhere else it is the item's full value. The
three list-path gates pass display copies and were correct by accident; the two direct gestures
(sell the held item, right-click sell from the backpack) pass a pristine item.

For an ordinary item that made the gate four times too strict - annoying, never lossy. For a
**stack** it made it far too lax, because `GetItemSellValue` multiplies a stackable consumable by
its count. A 99-potion stack was approved against one potion's value and then credited at
ninety-nine quarters of it. With `Stash.gold` at its `INT_MAX` cap and limited backpack headroom,
the difference is gold that goes nowhere and an item that is gone anyway.

**Fix.** The gate now takes the exact amount:

```cpp
bool StoreGoldFit(int price, const Item *itemFreeingCells)
```

Both direct gestures pass the `GetItemSellValue()` they already computed and then credit. The three
list paths pass `StoreHoldSalePrice(storehold[idx])` - a named accessor, so a display copy's price
reads as a price rather than as a field access that happens to hold one. `nullptr` replaces
`freesItemCells=false`, which makes the cursor case unable to claim cells it never occupied.

**Not done, deliberately:** the audit also asks that a non-zero `CreditSaleProceeds()` return be
treated as a transaction failure. Rolling back after settlement means un-placing gold already split
across the Stash pool and backpack stacks, which is itself lossy. With the gate now exact the
remainder is unreachable, and the existing named red log line is the right thing to leave behind for
a case that is supposed to be impossible.

## Finding 1 - a Premium purchase resurrected every trimmed slot

A purchase did `numpremium--; SpawnPremium(*MyPlayer);`, and `SpawnPremium`'s refill branch fills
**every** empty vanilla slot whenever `numpremium < maxItems`. The one-page trim empties the slots
that will not fit, so the first purchase refilled all of them at once. The comment at the call site
said "restocking a sold slot"; the implementation restocked the whole shelf.

**Fix.** `RestockOnePremiumSlot(slot, player)` (`items.cpp`) refills exactly the slot a purchase
emptied - `SpawnOnePremium` for a vanilla slot, a one-entry `StockOracoolMagicItems` range for an
Oracool tail slot. `RecountPremiumStock()` replaces the counting loop that used to live inline, and
now runs after every trim as well, so `numpremium` stops overstating a trimmed shelf.

The replacement is kept only **if the shelf still fits**: `ShopStockFitsOnePage()` asks the question
`TrimShopStockToOnePage()` answers destructively. Trimming after the restock instead would have been
wrong - premium items are not all one size, so a larger replacement can push a *different* item off
the page, and the trim would delete that item to pay for this one. Declining costs the shelf a slot
until the next Refresh; the trim would have cost the player an item they had not bought yet.

**Refresh Until** had the second half of the same bug: it scanned the raw `premiumitems` array and
could report "Found X" for an item `PlaceStock` cannot fit, and left whatever generation was current
untrimmed on the timeout and safety-limit exits. It now trims and recounts before the scan, so all
three exits leave a materialised shelf and "found" means visible.

## Finding 2 - Supplies kept a hidden reserve behind Pepin's potions

`witchitem` is shown by two tabs: Adria's own Buy tab, where it has the page to itself, and
Griswold's Supplies tab, where Pepin's four potions come first. It was trimmed against Adria's tab,
so it fit there and overflowed on Supplies - and the overflow stayed alive in the array, surfacing
as soon as a visible Supplies item was bought.

`TrimShopStockToOnePage` had refused `SmithConsumables` outright, to avoid clearing the potions.

**Fix.** The refusal is now per-**entry** rather than per-tab: `ShopSlot::neverTrim` marks Pepin's
potions, which are placed and shown like anything else but never cleared. That lets the combined
shelf be materialised without touching the fixtures on it.

`TrimWitchStockToOnePage()` then trims Adria's array against **Supplies**, the tighter of the two
pages, everywhere the array is regenerated. Supplies is strictly smaller, so anything that fits
beside the four potions fits without them - settling Supplies settles Adria's own tab too. It costs
her tab the few items that could never have been shown on Supplies anyway, which is the price of the
two views agreeing about what is in stock.

**Additional hardening from the same finding:** `SmithConsumablesStock()` pushed all four Pepin
entries unconditionally, though `InitializeSmithPepinPotions` claims an unavailable potion is
"skipped by the draw". Two of the four types come from `ItemMiscIdIdx`, which can answer `IDI_NONE`.
All four resolve today, so the claim was true by luck rather than by construction. Empty entries are
now filtered at the source, where every consumer sees one list.

## Verification

Four new tests in `test/stores_test.cpp`. Each defect was reintroduced verbatim and the matching
test watched to fail - and the first attempt at two of them was **vacuous**, which is why this
section names the exact reintroduction rather than only the result:

| Test | Reintroduced | Result |
|---|---|---|
| `SaleFitGateAnswersOnThePriceNotTheItemValue` | - | characterisation: pins that the two numbers differ |
| `SellingAHeldStackThatCannotBePaidForIsRefused` | `StoreGoldFit(sold._iIvalue, nullptr)` | **failed** |
| `RestockingOnePremiumSlotDoesNotRefillTheTrimmedOnes` | `numpremium--; SpawnPremium(...)` | **failed** |
| `SuppliesHasNoHiddenReserveBehindPepinsPotions` | trim `WitchBuy` instead of `SmithConsumables` | **failed** |

The premium test first called `RestockOnePremiumSlot` directly and passed with the defect in place,
because the defect was in what the *purchase* called; it now goes through
`SimulateSmithPremiumBuyForTest`. The Supplies test first had the `neverTrim` guard removed, which
proves nothing: the potions are first in placement order and always place, so that guard is
defensive rather than load-bearing. The load-bearing change is that Supplies is trimmed at all.

Suite: **574/576**, the two standing baseline failures
(`Drlg_l1.CreateL5Dungeon_diablo_3_844660068`, `Timedemo.WarriorLevel1to2`) and nothing else.
`stores_test.exe --gtest_shuffle --gtest_repeat=40`: 840/840.

## Still open from this audit

Findings 4-7 (packer concurrency, archive content verification, zip publication, wiki integrity) are
build and release tooling, and are the next unit of work.

Not pushed: GitHub Actions minutes are exhausted until roughly 2026-09-01.
