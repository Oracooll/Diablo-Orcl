---
version: v1.12.112
date: 2026-09-21
area: UI / Griswold's shop
tests: 832/832
---

# The white icon set goes in, and the Salvage page starts behaving like a tab

## RfA-26 delivered: batch 49, audited rather than trusted

The pack claims binary alpha, no hue and the stated clearances. Audited independently of its notes:

| Check | Result |
| --- | --- |
| sizes | 6 x 24, 6 x 56, 6 x 224, all exact |
| partial alpha | none, in any of the 18 files |
| hue | max channel spread **8**, against the brief's limit of 12 |
| clearance | >= 1 px at 24, >= 2 px at 56 |
| black | 24-45% of each mark - the outline and shadow |

Installed over batch 48's same-named files.

**The contrast problem is solved; three of the six still read poorly.** Repair, Repair All and Refresh are clean.
Sell reads as a lump rather than a palm holding a coin, Sell All as a cloud with holes rather than a spill, and
Recharge as an odd crown. Flagged to the user for a short follow-up on those three - simpler silhouettes, not more
detail.

## The Salvage page's three misalignments

All three had one root cause, though this version treats them as three: **the page is a window, not a store
screen.** It sets `stextflag` to None and opens on its own, so every piece of store machinery looked past it.

1. **The X** was hand-authored at (316,5) while every other window uses `GetWindowCloseButtonRect` - (319,3) on a
   340-wide panel. The docked page now asks the shared helper. The small painted page keeps its own, which is
   measured against art the shared corner knows nothing about.
2. **Walk-away** never applied. `UpdateStoreState` measures `stextflag`'s towner, and there was none.
3. **Clicking the towner again** did nothing. With a normal tab that click puts the dialog back, which reads as
   the shop closing; a window had nothing to replace it.

`TownerForOpenVendorPage` and `CloseVendorPageForTowner` answer 2 and 3 for any docked vendor page - Griswold's
Salvage and the artisans' workshops and books alike, because they share the code and fixing one would have left
the other visibly different.

A refused close still stands: a page holding items with no room to give them back says so in red, and the dialog
does not bury it.

## Superseded almost immediately

The user's next instruction - "all tabs for all vendors are to be considered Stores, not windows" - replaces the
mechanism behind 2 and 3 for the Salvage tab with the store machinery itself. What survives is the X fix and the
handling for the artisans' pages, which are reached from dialogs and genuinely are not tabs.

## Build

Debug, clean. 832/832.
