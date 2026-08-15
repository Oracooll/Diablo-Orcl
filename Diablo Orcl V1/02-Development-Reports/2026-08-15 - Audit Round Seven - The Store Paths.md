---
date: 2026-08-15
version: 1.6.11
area: Self-audit / stores
---

# Audit Round Seven — The Store Paths

User-directed: *"keep auditing. the store paths next."* Nine fixes, all in stores.cpp, and most of
them are one family: **the stale selected row.**

## The family resemblance

The pattern was already on file. An earlier user report — buying out Griswold's basic stock kept
"selling" phantom items — was fixed at the time with a guard in `SmithBuyEnter` alone. This round
asked the obvious follow-up: *every* store screen restores its old selection after a purchase
rebuilds the list (`ConfirmEnter` ends with `stextsel = stextlhold`), so every screen whose list can
shrink has the same reachable stale row. What differs is only what the stale row corrupts:

| Screen | What a stale row did | Fixed |
|---|---|---|
| Smith buy | phantom item (the reported one) | already guarded |
| Sell (smith & witch) | stale `storehidx` into `RemoveInvItem`/`RemoveSpdBarItem` | guard added |
| Repair | writes `_iDurability` through `InvList[i]`, i from a dead screen — **OOB write**, InvList is 40 items and the index reaches 47 | guard added |
| Recharge | same, through `_iCharges` | guard added |
| Identify | stale `storehidx`/`storehTabIdx` marks the wrong item identified | guard added |
| Premium buy (Enter **and** completion) | the sparse-array scan's only loop condition was the skip count, so it **read past the array** until stray memory satisfied it; the completion copy also **clears** the slot it lands on | both bounded |
| Unique buy (Enter and completion) | shift loop starting from a raw, possibly negative index — OOB write before the array | both guarded |
| Witch/consumables buy | the fork's Griswold-consumables screen indexes a **freshly built vector** (Pepin's potions + live witch items) that is smaller than the fixed witch array — the existing `isEmpty` guard sat one line **after** the vector indexing | bounds test moved before the indirection, and the completion carries its own |

The completions that could bail now bail **before** `TakePlrsMoney`, so a stale row costs nothing
rather than charging gold for no goods (`SmithBuyPItem` had its scan after the charge; reordered).

## The one fix that is not a stale row

**`TotalPlayerGold` overflowed exactly when the player got rich enough.** It summed `_pGold +
Stash.gold` as plain int — and the stash's own deposit guard deliberately lets `Stash.gold` grow to
`INT_MAX`. Near that ceiling the sum overflows: UB formally, a negative wrap in practice, which
converts to a huge `uint32_t`, at which point `PlayerCanAfford` approves **everything** and
`TakePlrsMoney` — whose callers all trust that check — drives the stash negative, keeping the
wraparound alive. Self-amplifying once entered. Now a saturated 64-bit sum.

This one connects to round six: the sell path deliberately routes all sale gold into the stash pool,
which is precisely the mechanism that walks `Stash.gold` toward the ceiling.

## Audited and found sound

- Every `TakePlrsMoney` caller sits behind a `PlayerCanAfford` — verified all thirteen.
- Every buy runs a dry-run `StoreAutoPlace(item, false)` → NoRoom before Confirm — seven for seven
  with the persist calls.
- Repair-all and sell-all re-check affordability/fit **per iteration** and rebuild indices via
  `StartSmithRepair`/`StartSmithSell` each pass.
- Wirt's 50-gold peek and both his price formulas are guarded.
- The consumables screen's stock removal routes by `entry.vendorIndex` to the right vendor's array —
  the witch's stock cannot be corrupted from Griswold's page (chased and disproved).
- Healer buy already carried the full SmithBuyEnter-style guard.
- Recharge deliberately never sources from extra tabs (commented); identify handles them fully.

## Why the tests never caught these

`stores_test` pins the dispatch and arithmetic of **valid** rows. Every fix this round is about a row
that stopped being valid after the screen rebuilt under it — the store UI equivalent of round six's
torn file. Same closing question as that report, one layer up: *the screen's shape proves nothing
about its references.*

## State

**354/356** — the usual two. `stores_test` passes against all nine changes.
