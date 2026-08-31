# The sell path was destroying items and value (v1.9.69)

**Date:** 2026-08-27
**Version:** 1.9.68 → 1.9.69
**Tests:** 563, of which 561 pass — the two standing baseline failures, unchanged.

---

Taken out of a sixteen-item request and shipped first, because both of these were actively costing
the player items and gold every time they used the shop.

## 1. The wrong item was sold

> "i right click on items to buy them back and then to resell then over and over and something weird
> happend. random itrem get sold back"

Mine, from v1.9.65. `ShopSellInventoryItem` read `pcursinvitem - INVITEM_INV_FIRST` as a **grid cell**
and resolved it through `InvGrid`. It is an index into the backpack **list** — that is what
`CheckInvHLight` builds it from, via `GetActiveInvListItem`.

The two numbering schemes agree only by coincidence, and a sale **compacts the list**, so the
coincidence broke after the first sale. Hence "random", and hence only after repeated selling.

Now uses the list index directly, and is tab-aware through `GetActiveInvListItem` /
`RemoveActiveInvItem` — the same correction `UseInvItem` already carries, for the same reason:
reading `InvList` directly silently uses the wrong item on tabs 2-10.

## 2. Every round trip through a vendor divided the item by four

> "something weird is happening with the sell/back back/resell, rebuyback price of items. it is like
> it is constantly changing and reducing."

**Not mine, and not new to the right-click gesture — it affected every way of selling.**

Selling overwrote the item's own `_ivalue` and `_iIvalue` with the quarter-price it fetched, and
*that mutated copy* went onto the buyback shelf. So buying it back handed the player an item worth a
quarter of the one they sold. Sell that, and it fetched a quarter of the quarter.

`RecordSale`'s own comment described the mechanism as a feature: *"The price the player was paid is
already in `_iIvalue` by the time anything calls this — that is what makes buyback 'at sold price'
free."* Free, and wrong: it bought the display price by spending the item's identity.

`SoldItem` now carries `price` beside `item`, and the item is stored **pristine**. All three sell
paths pass the price explicitly:

- the right-click sell — captures the item before removal
- the drag-onto-panel sell — same
- the text-list sell — this one needed the most care. `storehold`'s copy is a *display* copy whose
  values the list builder had already overwritten, so the pristine item is read from the player's
  pack **before** the removal that destroys it.

## 3. Buying back plays the gold sound

`ShopBuyBack` completes its own transaction rather than going through `ConfirmEnter`, so it never
inherited the sound added there yesterday.

---

## No test, and why

I tried to pin this with a real round trip — sell an item, buy it back, check what came home — and
the shop path takes an access violation in this harness within a millisecond. It needs game state the
unit suite does not stand up.

I could have shipped the test I wrote first instead: it asserted that `GetItemSellValue` returns the
same number for the same item. That passes against the **broken** build, because the function was
never the problem — the mutation of the item was. A test that cannot fail is worse than none, so it
is not here.

**These two fixes are therefore verified by inspection, not by test.** That is a real gap and it is
the honest description of it. Making the shop reachable from the unit harness is the follow-up.

---

## Still queued from the same request

Thirteen items, none of them started: hover prices in the shop; page-capping the BASIC tab; a RARE
tab; filling out SUPPLIES; the Repair-as-hammer-cursor rework; Repair/Repair All/Recharge as a text
button row with a hover cost; Refresh and Refresh Until on their own row and added to more shops;
uniques as a full page with the INI reduced to on/off; a new SET shop; and docking every limestone
window to the bottom of the screen.
