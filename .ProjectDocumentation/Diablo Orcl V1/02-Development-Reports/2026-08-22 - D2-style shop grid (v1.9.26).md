# D2-style shop: the grid (v1.9.26)

**Date:** 2026-08-22
**Version:** 1.9.26
**Tests:** 511/513 — the two standing baseline failures (`Drlg_l1.CreateL5Dungeon_diablo_3_844660068`,
`Timedemo.WarriorLevel1to2`).
**Status:** built, not played.

## What was asked

> lets work on shops - making them D2 style. […] We aim at having one dialog option in vendors -
> Enter Shop. All possible item they sell are in that one shop divided in different tabs.

and then, mid-build:

> make items display grid the size of STASH grid. it can fit comfortably.

v1.9.25 delivered the structural half — one door per vendor, everything behind it as tabs, drawn as
a strip above the vanilla store box. This is the visual half: the box is gone and the stock is an
icon grid.

## What shipped

### The panel

`Source/oracool/shop_grid.{h,cpp}`. A 340×720 panel in the top-left slot — the same rect the stash,
character sheet and quest log already share. Nothing else is open while a shop is, so the slot is
free.

It had to move. A ten-by-sixteen grid at the inventory's 28px pitch is 280×448, and the vanilla
store box is 592×292; the grid does not fit inside it at any position. Putting the shop where the
stash lives also means the two grids land at the same pitch and the same screen column, which is
what makes them read as the same kind of surface.

Contents, top to bottom: vendor name, tab rows, grid, footer. The footer carries the hovered item's
name and price, the tab's bulk-action buttons, and the player's gold. A red X sits top-right.

### The grid

Ten columns by sixteen rows at `INV_SLOT_SIZE_PX`, the stash's geometry to the pixel. Items occupy
their real inventory footprint and are packed row-major first-fit, so a two-handed sword reads as a
two-handed sword before you hover it. 160 cells against a 25-item stock is a lot of slack; an item
that somehow did not fit is dropped rather than wrapped, because a wrapped item would be drawn over
another one's cells.

Keyboard: arrows move the cursor through the stock order (left/right by one, up/down by a grid row),
Enter buys, Escape leaves. Left and right previously did nothing on a store screen — the text list
had one item per row — so they were free to take.

### The transactions were NOT rewritten

This is the part worth reading. Every transaction in `stores.cpp` derived *which item* from
`stextvhold + ((stextlhold - stextup) / 4)` — the text list's scroll position. That arithmetic is
the direct cause of the store crash fixed at v1.8.90 and the three stalled walks found at v1.8.94,
and a grid has no text lines to derive anything from.

Rather than reimplement seven transactions, the grid resolves a click to a **stock index** and hands
it to `ShopSelectIndex` (stores.h), which puts that index where the tab's own Enter handler expects
to find it and then calls the handler. Every guard those handlers carry — can the player afford it,
will it fit, is the row stale — runs unchanged, and there is still exactly one copy of each. The
confirm, no-money and no-room screens are still the vanilla text box.

Two facts made that bridge safe to write:

- `GetShopStock(TalkID)` is explicit about the fact that a tab's display order and its transaction
  index are **not the same number**. Griswold's basic stock is indexed by array slot, holes
  included; his premium stock by visible position, holes skipped; sell/repair/recharge by position
  in `storehold`. A grid that invented its own numbering would have sold the wrong item on whichever
  of those it guessed wrong.
- The seven transactions were first split into `…At(int idx)` forms with thin scroll-position
  wrappers, so the index is now a parameter everywhere it is used. Two callers that had been faking
  up a scroll position purely to make the old derivation yield zero — `SmithSellAllItems` and
  `SmithRepairAllItems` — now just say `0`.

### Bulk actions

`Sell all`, `Repair all`, `Refresh` and `Refresh until` were border-hugging rows on the text panel,
which no longer exists on those screens. They are footer buttons now, via `GetShopActions` /
`ShopActivateAction` — the same bridge, addressing the action by the store line it still owns, so
the handler that dispatches on that line is unchanged. The gating conditions are copied from the
places that used to add the rows, and they have to match: the handlers re-test them and silently do
nothing when they disagree.

### Adria's door

Her menu had Buy / Sell / Recharge as three entries; it has one `Enter Shop` now, matching
Griswold. Pepin already had one. Lines 16 and 18 are left empty rather than closed up — the respec
and leave lines are addressed by number from four places, and renumbering them buys nothing but a
chance to miss one. The door itself became `WitchShopDoorLine` for the same reason: four sites
address it, and three of the four would still compile after a miss.

### The tab module split

`oracool/shop_tabs.{h,cpp}` now answers only "which tabs does this vendor have, and what are they
called". The geometry, drawing and hit-testing moved into `shop_grid.cpp` when the tabs moved inside
the panel. The split is worth keeping: the tab **sets** are a statement about the vendors, derived
from the menus they replaced; the tab **rects** are a statement about one panel's layout.

## One test changed

`Stores.SmithSell_FourItemPage_SellAllRowNotHijackedByPremiumRedirect` went red, correctly. Its
last third pressed Enter on Sell All's text row; Sell is a grid screen now, so `StoreEnter` reaches
the grid cursor instead. Retargeted to `ShopActivateAction`, which is the door that button goes
through now. The routing assertions above it (`ResolveBackRowClickLine`) still pass and were left
alone.

**Backlog row:** `ResolveBackRowClickLine` and the border-hugging-button redirect it implements are
now unreachable from every screen that had such a button, because all of them are shop tabs. It is
still tested and still correct; it is just dead. Worth deleting in its own pass rather than in this
one.

## Not verified

Nothing here has been played. Specifically unseen: the panel against the real screen at 960×720, the
grid's packing with a full 25-item stock, whether the two tab rows and the footer are legible at
FontSize12, the red X, and every one of the four bulk-action buttons through their new door. A
screenshot is the only verification for a window drawn over the world.
