# Shop: the second screenshot pass (v1.9.28)

**Date:** 2026-08-23
**Version:** 1.9.28
**Tests:** 511/513 — the two standing baseline failures.
**Status:** built, not played.

Seven items, all from the user, after playing v1.9.27. Two were bugs in the screenshots; five were
the shop growing into what it should have been.

## 1. The popups had no backing

The stat block drew as bare outlined text over the grid instead of on the padded, bordered plate the
inventory's own hover gets.

`cursor_tooltip.cpp` picks between its two treatments with `IsHoveringItem()`, which reads
`pcursitem` / `pcursinvitem` / `pcursstashitem` / `ActiveTabItemHovered`. A shop item is the
vendor's, not the player's, so **none of those four ever describe one** — the tooltip had no way to
know it was looking at an item.

The shop records its own hover instead (`ShopHoverActive`, exposed as `IsShopItemHovered`), written
on the same once-per-frame pass that clears and repopulates the other four. That is the argument the
function's existing comment already makes for deriving this from hover globals rather than from a
flag somebody has to remember to set; this just adds a fifth to the list.

## 2. Towners were being hovered through the shop

The screenshot showed "Gillian the Barmaid" while the cursor sat on the Repair tab.

Two fixes, because it was two problems wearing one symptom:

- `CheckCursMove` had no idea the shop panel existed, so it went on targeting the world underneath
  it. It now bails inside the panel rect, exactly as it already does for the inventory and the
  stash.
- `SetShopHoverInfoString` returned false whenever the cursor was not on an item, letting every
  producer further down `UpdateInfoString` have a go. It now answers for **anything inside the
  panel** — an item, a service icon's hint, or nothing at all.

## 3. Fine → Magic

One string.

## 4. The shops are full now

The grid holds 160 cells; vanilla's stock counts were sized for a list that showed four rows.

| | was | now |
|---|---|---|
| `SMITH_ITEMS` | 25 | 45 |
| `WITCH_ITEMS` | 25 | 45 |
| `SMITH_PREMIUM_ITEMS` | 15 (6 stocked outside Hellfire) | 30 |

Counts are derived from the arrays rather than hand-written, and floored at three quarters full so a
bad roll still leaves a full-looking shop.

**This found a real overrun**, and it was already there. `SortVendor` walked to the first empty slot
with nothing stopping it at the end of the array. Griswold could already fill his: `iCnt` is capped
at `SMITH_ITEMS` minus the salvage charms, and `StockSalvageCharms` then adds exactly that many, so a
maximum roll left no empty slot and the sort ran off the end. Raising the counts turned an occasional
overrun into the normal case, which is how it surfaced. `SortVendor` takes a bound now, and all three
call sites pass their array's real length.

Two supporting changes:

- Vanilla's two premium quality-level tables mapped a **slot number** to a delta, so they could only
  describe a stock of exactly their own length. Replaced by `PremiumLevelDelta(index, count)`, which
  computes the same −1..+3 spread for any length, and the two hardcoded level-up rotations became
  one that discards the cheapest third.
- `giNumberOfSmithPremiumItems` is `SMITH_PREMIUM_ITEMS` in both games now. **This is a game-save
  format change** — a save written before this version will not load. V1 always starts a new game.

## 5. The inventory opens with the shop

`StartStore` still calls `CloseInventory()` first (it also shuts the stash and the gold prompt, which
have no business being open over a shop, and the shop occupies the stash's slot) and then reopens the
inventory for a grid screen.

`LeftMouseDown` had to change with it. It swallowed **every** click while a store was open, which
would have made the inventory beside the shop untouchable. A shop grid is a panel, not a modal
screen: only clicks that land on it go to the store, and the rest fall through to the normal
inventory routing — which is what lets the player pick an item up at all. Every other store screen
(towner dialogs, Confirm, No money) still swallows the whole screen.

## 6. Selling is a drag; Sell is now Sold

Drop an item from the inventory anywhere on the shop panel and the vendor buys it, using that
vendor's own `SmithSellOk` / `WitchSellOk` — Adria still will not take armour. **A refusal leaves
the item in the player's hand**; swallowing an item a vendor will not buy is how you lose one.

The Sell tab became **Sold**: what the vendor has already bought this session, offered back at the
price they paid. Every sale goes through one `RecordSale`, whichever door it came in by — a click on
the old list, Sell all, or a drag. Newest first, forty deep, cleared with the rest of the stores.

Not saved, deliberately: a buyback list that outlived a reload would have to outlive the shop
restocking too, and "the thing you just sold is still there" only has to hold for as long as changing
your mind is plausible.

`Sell all` survives as a text button on the Sold tab. It is the one thing the redesign would
otherwise have deleted silently — there is no sell list to bulk-sell from any more, so it sells
straight out of the inventory and the results land on the tab you are looking at.

## 7. Repair, Repair all and Recharge are icon buttons

They are services performed on an item you already have, not screens with stock to browse, so they
stopped being tabs. Three 20px buttons flush right on the control row: drop an item on **Repair** or
**Recharge** to have it done, click **Repair all** for the whole inventory. Adria gets Recharge only.

The icons are **drawn, not blitted** — there is no art, and the request was icons and no text. Filled
rectangles: a hammer, a hammer over three dots, a bolt. When art arrives this is one blit per button
and no geometry moves. Because an icon says nothing, hovering one puts its meaning in the tooltip.

Pricing a held item meant the repair and recharge formulas had to be reachable from outside the
`storehold` list they lived in. `RepairPriceFor` / `RechargePriceFor` are now the single copy, and
`AddStoreHoldRepair` / `AddStoreHoldRecharge` call them. That also fixed a small pre-existing wart:
`AddStoreHoldRepair` wrote an item into `storehold` **before** deciding it was too cheap to repair,
then returned without counting it, leaving a stale entry one past `storenumh`.

One test caught the extraction changing behaviour: `AddStoreHoldRepair_magic` hands the function a
bare struct with durability fields and nothing else, so an added `isEmpty()` refusal silenced it.
Removed — `_iMaxDur <= 0` already covers an empty item.

## Not verified

All of it. In particular: whether a dragged item actually reaches the shop (the click-routing change
is the load-bearing part), whether the three icons read as anything at 20px, whether a 45-item stock
actually fills the grid or overflows the 160 cells it has, and whether the Sold tab is populated by
all three sale paths.

`TalkID::SmithRepair` and `SmithRecharge` still exist as screens and still answer `IsShopTab`, but
nothing routes to them. Left rather than removed — that is a deletion pass, not this one.
