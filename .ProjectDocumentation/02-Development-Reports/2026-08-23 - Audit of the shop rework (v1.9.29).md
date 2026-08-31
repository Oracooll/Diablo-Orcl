# Audit of the shop rework, v1.9.26–1.9.28 (v1.9.29)

**Date:** 2026-08-23
**Version:** 1.9.29
**Tests:** 512/514 — the two standing baseline failures. One test added.
**Status:** built, not played.

Requested before the next playtest. Four defects found, all four fixed. Two of them would have been
visible in the first minute; two would not have been visible at all.

## Found and fixed

### 1. Roughly a sixth of Griswold's stock was unbuyable, silently

The worst of the four, and it was mine, introduced two versions earlier.

`PlaceStock` packed the stock row-major into the 160-cell grid and **dropped whatever did not fit**.
That was fine when the smith carried 25 items; v1.9.28 raised him to 45. Weapons and armour occupy
four to six cells each, so 38 generated items plus 7 salvage charms need roughly 190 cells. About
five or six items would have been generated, priced, sorted — and then never drawn, with nothing
anywhere saying so.

The grid **pages** now. An item that will not fit on the current page starts the next one; two
arrows and a `1/2` readout sit at the ends of the gold row, which was a centred line with both ends
going spare (the space above the grid is otherwise fully spoken for — see the `static_assert`).
The keyboard cursor turns the page with it, so arrowing past the end of page one no longer moves an
invisible selection that Enter would then buy.

The page test in the hit-tester is load-bearing and worth naming: every page reuses the same cells,
so without it a click resolves to whichever item occupies that cell on **any** page, and the first
match is page 0's.

### 2. Pepin would have bought anything Griswold buys

`ShopSellHeldItem` picked between `WitchSellOk` and `SmithSellOk`. Pepin is neither — he has never
had a Sell screen, so no function speaks for him — and he fell through to the smith's arm. Dropping
an item on the healer's panel would have sold it, at Griswold's prices.

Selling is now gated on the vendor being Griswold or Adria before either predicate is consulted.

### 3. `ShopRepairAll` could spin

The loop's exit depends on each pass actually repairing something. Anything that charges without
clearing the damage — a `storehidx` encoding `SmithRepairItemAt` cannot decode, say — would loop,
taking gold until the player could no longer afford the next one. Inherited from
`SmithRepairAllItems`, but this put it behind a button.

Bounded at 48, which is `storehold`'s capacity: a run that repairs something every pass can never
reach it.

### 4. The inventory shut and reopened around every purchase

`StartStore` reopens the inventory for a shop tab. Confirm, No money and No room are not shop tabs,
so buying anything closed the inventory for one screen and reopened it after. Now it also reopens
when `stextshold` is a shop tab — the screen you came from and will go back to.

## Checked and found correct

Recorded because each was a plausible failure that would not have shown up in a build:

- **Sprite anchoring.** `InvDrawSlotBack` is bottom-anchored (`targetPosition.y - size.height + 1`),
  so `SpriteAnchor` lines the backing up with the sprite exactly as `inv.cpp` does. The one-pixel
  offset between backing and sprite is the inventory's own and is shared, not introduced.
- **Dangling stock pointers.** `GetShopStock` for the Supplies tab builds a temporary vector of
  `ConsumablesStockEntry` and pushes `entry.item` out of it. Those point at `smithPepinPotions` and
  `witchitem`, not into the temporary — safe. The Sold tab's point into `BuybackStock`, and the only
  caller that mutates it does so after its last read.
- **`TakeGold`** is bounded by `_pNumInv` and does not index off a zeroed `InvGrid`.
- **Salvage charm room.** `iCnt` is capped at `SMITH_ITEMS` minus `SalvageTierCount`, so
  `StockSalvageCharms` always has exactly its seven slots.
- **`SmithMenuLine`** falls back to the shop door for the tabs that are no longer menu entries, so
  Escape out of Sell still lands somewhere real.

## One test added, and what it does not cover

`Stores.Sold_BuyBackChargesTheSalePriceNotTheItemValue` pins the two claims that are checkable
headless: a sale reaches the Sold tab, and the tab offers it at the price paid rather than at the
item's worth (for anything magical those differ by several times).

It cannot run the buyback itself. Placing an item calls `NetSendCmdChBeltItem`, and the network layer
is not up in the test binary — the process dies with an access violation. **No existing store test
executes a persisted placement either**, which is why this had not been noticed before; the two that
come closest (`SimulateSmithConsumablesPurchaseForTest`, `SimulateSmithPremiumBuyForTest`) stop at
the probe and at the stale-row guard respectively. The test therefore pins the charge from the
refusal side: one gold short must refuse, consume nothing and charge nothing.

That is a gap in the harness, not in the shop, and it is worth its own pass — a store test that can
complete a purchase would cover a lot of code that currently has none.

## Still unverified

The same list as before, minus what the audit closed. In particular the drag itself: the
`LeftMouseDown` routing change is the load-bearing piece and no test can reach it.
