---
date: 2026-08-15
version: 1.6.9
area: Self-audit / stash, tabbed inventory
---

# Audit Round Six — Stash and Tabbed Inventory

User-directed continuation: *"keep auditing. stash and tabbed inventory next."* These two were left
out of rounds one through five on the grounds that `pack_test` and `writehero_test` cover them — and
that reasoning was half right. The tests cover the **round-trip**: what a correct build writes, a
correct build reads back. What they cannot cover is a file the build did *not* write correctly — and
this fork's autosave rewrites both files constantly, so a torn write is a when, not an if.

Both fixes are load-time hardening in loadsave.cpp. Nothing in the runtime paths needed changing.

## Fix 1: a torn stash header could hang the load for minutes

`LoadStash` reads a `pages` count and loops over it — **before** `IsStashSizeValid` runs, because the
exact-size check needs `itemCount`, which the format stores after the grids. A corrupted count in the
billions would spin the grid loop against an exhausted stream long before the validation ever saw it.

The file itself is the cheapest ceiling: a page costs at least its grid on disk, so *more pages than
the whole file could hold* is corruption, now rejected up front with the same message the exact check
uses. Vanilla has the same shape but writes its stash once per session; ours is rewritten on every
autosave, which is why this ranks as a fix here and a footnote upstream.

## Fix 2: grid references were trusted against lists they might not match

Same finding in both systems, so one explanation:

- A stash grid cell is an index+1 into `stashList`; `GetItemIdAtPosition` feeds it to a
  `std::vector` **unchecked**.
- A tab grid cell is an index+1 into `InvTabList[t]` (a 70-item array); cell values up to 127 index
  past it entirely.

The existing validation proves the file's **shape** — right version, right byte count, plausible item
count — but not that its references point inside each other. A record whose grid disagrees with its
own item count (the torn-write case again) passed every existing check and became an out-of-bounds
read waiting on a hover.

Both loaders now clamp: any cell pointing past the item count it was saved beside becomes an empty
cell. The items themselves are kept — at worst an item loses its grid spot, never the other way round.

## Audited and found sound

| Path | What was checked |
|---|---|
| `RemoveStashItem` | Swap-compaction fixes references across **all** pages; current-page-only clearing is correct because every caller removes a hovered item; `pop_back` present |
| `SortStash` | Rebuilds list and all grids wholesale through the same first-fit placement — no stale references possible |
| Store sell path | `storehidx`/`storehTabIdx` captured indices are rebuilt by `StartStore(stextshold)` after **every** sale, and the tab array rides every swap in both sort loops; `SmithSellAllItems` re-scans per iteration |
| Sell gold overflow | `Stash.gold <= INT_MAX - cost` with inventory fallback |
| Deposit gold overflow | Same guard on the paste path |
| Gold withdraw | Input max clamped to `min(RoomForGold(), Stash.gold)` at open; the dialog cannot survive any interaction that would invalidate it |
| Stash paste | `FindTargetSlotUnderItemCursor` clamps the item rectangle against `StashGridSize - itemSize`, so a 2×3 item cannot write past row 16 |
| Page navigation | `SetPage`/`NextPage`/`PreviousPage` all clamp to `LastStashPage`; the page loaded from the file goes through `SetPage` |
| Tab remove/compact | `RemoveActiveInvItem`/`RemoveExtraTabItem` mirror vanilla `RemoveInvItem`'s swap-compact exactly, including negative continuation cells |
| Tab click | All ten positions bounded by `TabCount`; `ActiveInventoryTab` reset to 0 by `CloseInventory`, and stale values are bounds-safe regardless |
| Tab save versioning | Version 2 checked for exact equality; truncated streams stop cleanly; absent file = empty tabs |
| `heroinvtabs` load order | `_pLevel`/`_pClass` set before `CalcPlrInv`'s mask rebuild; readied-spell validation still last |

## The lesson this round adds

Round five's checklist asked of any state: *where does it live, is it saved, does it hold still.*
This round adds the fourth question, for anything read back from disk: **does the file's shape prove
its references?** Size and version checks pass a record whose parts disagree with each other. The
cheap answer is referential clamps at load — three loops, no format change, and the failure mode
becomes "an item loses its grid spot" instead of undefined behaviour on hover.

## State

**354/356** — the usual two. `Loadsave` passes, which round-trips the stash and tab structures the
guards now sit in front of.
