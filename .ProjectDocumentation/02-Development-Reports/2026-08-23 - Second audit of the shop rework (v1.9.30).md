# Second audit of the shop rework (v1.9.30)

**Date:** 2026-08-23
**Version:** 1.9.30
**Tests:** 512/514 — the two standing baseline failures.
**Status:** built, not played.

The first audit (v1.9.29) went through the transaction paths and the new geometry. This one went
through the parts that pass they hadn't touched: the click router end to end, the store's line-index
arithmetic under the new callers, and the two shared containers.

Three findings. One correctness, two layout/robustness. Nothing as bad as the paging bug.

## 1. Griswold and Adria shared one Sold shelf

`BuybackStock` was a flat list. Both vendors' Sold tabs read all of it, so an item sold to Griswold
appeared on Adria's shelf and could be bought back there — and buyback does not consult `sellOk`, so
Adria would have handed back plate armour she refuses to buy.

Each sale now records which vendor made it, and one `BuybackIndicesFor` filter serves **both** the
tab's contents and the buyback's target. Two copies of that filter is how you eventually sell back
the wrong item, so there is one.

## 2. The tab strip reserved a row it no longer uses

`ShopTabRows` was 3, sized when Repair and Recharge were still tabs. Five is now the most any vendor
offers (Basic, Magic, Unique, Supplies, Sold — the last two already conditional), so two rows is
enough and the third was a blank 22px band between the tabs and the controls.

Reclaimed, and spent on the gaps: the control stack's two spacers went from a cramped 2px to 10 and
8. The height is still fixed rather than derived from the live tab count, so the grid does not shift
when a vendor has fewer tabs — and `DrawShopTabRow` now asserts a vendor has not outgrown the
reservation, because the failure mode is a tab row drawn silently over the controls.

## 3. Two narrower ones

- **`healitem`'s length was written as a literal `20`** in `GetShopStock`, sitting beside three
  vendor arrays that had just been resized. Now `std::size(healitem)`.
- **The inventory reopened over the towner's dialog.** v1.9.29 reopened it whenever `stextshold`
  named a shop tab, but `stextshold` is only written by the Enter handlers, so after backing out of
  a shop it still names the tab you left. Narrowed to the three screens that actually return to a
  shop: Confirm, No money, No room.

## Checked and found correct

- **The drag can actually reach the shop.** Traced `LeftMouseDown` end to end: with a shop grid open
  and the click outside the panel, it falls through the HUD tests to
  `invflag && GetInventoryPanelRect().contains(...)` → `CheckInvItem`. So an item can be picked up
  while the shop is open, which is the load-bearing half of drag-to-sell. The other half —
  `CheckShopGridClick` claiming the drop — is one branch above it.
- **No special store line collides with `stextup`.** `ShopSelectIndex` parks the selection on
  `stextup` (5); `BackButtonLine` is 20–22 and every bulk-action line is derived from it, so the
  lowest is 17. A collision would have made a grid click on Griswold Premium hit Refresh instead of
  buying.
- **`ShopSelectIndex` is correct for any `stextup`.** It sets `stextsel = stextup`, so the handlers'
  `stextsval + ((stextsel - stextup) / 4)` collapses to `stextsval` regardless of what `stextup`
  happens to be — which matters because `StartSmithSell` returns early without setting it when
  there is nothing to sell.
- **`SmithMenuLine` falls back to the shop door** for the four services that are no longer menu
  entries, so backing out of any tab lands on a real line.
- **Supplies-tab indices.** `GetShopStock` numbers by position in the combined Pepin+Adria vector,
  which is exactly what `WitchStockItem(idx, true)` expects, and that call is bounds-checked.

## Known and deliberate

`TalkID::SmithRepair` and `SmithRecharge` still exist as screens, still answer `IsShopTab`, and
nothing routes to them. `GetShopActions` still has a `SmithRepair` arm that can no longer be
reached. Left in place — deleting the orphaned repair/recharge screens is a pass of its own, and
doing it in the middle of an audit is how you break the thing you were auditing.

## Still unverified

The drop itself, the three icons at 20px, whether a 45-item stock lands on one page or two in
practice, and every screen at 960×720 by eye.
