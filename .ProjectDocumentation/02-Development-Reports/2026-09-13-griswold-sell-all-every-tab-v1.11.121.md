# Sell all from every Griswold tab

2026-09-13 — v1.11.121

## Why

> "in griswold shops - any screen that has refresh button reduce its width in half and add a second button next
> to it SELL ALL. i want to be able to sell all items from any tab of griswold shop."

Sell all existed only on the Sold tab, so emptying the pack meant leaving whatever shelf the player was browsing.

## What changed

Every Griswold tab now carries **Sell all** on its action row (`GetShopActions`, `stores.cpp`):

| Tab | Action row |
|---|---|
| Basic, Rare, Supplies | Refresh · **Sell all** — each half the row |
| Magic | Refresh · Refresh until (when enabled) · **Sell all** — halves, or thirds with Refresh until |
| Unique, Set | **Sell all** — no Refresh on these shelves, so it takes the whole row |
| Sold | Sell all — unchanged |

No layout code changed: `ShopControlRect` already divides the action row's width between however many actions a
tab has, so adding the button is what halves Refresh.

## How it dispatches

The Sold tab's Sell all dispatches on `SmithSellAllLine()`, but that number is shared — at the normal font size
`PremiumRefreshUntilLine()` is the same line, and the Magic tab dispatches by line, so reusing it there would have
fired Refresh until. The buy tabs' Sell all therefore uses `GriswoldTabSellAllLine` (1000), a value past the
store's 24 text lines, caught at the top of `ShopActivateAction` before any line-based handler runs.

It calls the existing `SmithSellAllItems`, which now takes the tab to return to: it sells everything Griswold
buys (the same `SmithSellOk` list the Sold tab uses) and **stays on the tab it was pressed on**. If the gold will
not fit, the No Room screen returns to that tab too.

## Tests

`StoresTest.SellAllIsOfferedOnEveryGriswoldTabAndStaysOnIt` — Sell all on all six buy tabs; on Basic it sits
after Refresh on a different line; pressed on Basic it empties the pack, pays, and leaves the player on Basic.

## For the user to look at

Any Griswold tab: the bottom row reads Refresh | Sell all (or Sell all alone on Unique and Set). Pressing it sells
the pack without leaving the shelf.
