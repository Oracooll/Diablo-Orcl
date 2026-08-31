# Third audit of the shop rework (v1.9.31)

**Date:** 2026-08-23
**Version:** 1.9.31
**Tests:** 512/514 — the two standing baseline failures.
**Status:** built, not played.

The first two audits covered the transactions, the geometry, the click router and the shared
containers. This one went at the window lifecycle — what else lives in the slot the shop moved into,
and what the new click routing made reachable.

Two findings. The first is the most serious thing any of the three audits turned up, and it is
entirely a consequence of decisions made in this rework.

## 1. The shop shares a slot with four other windows, and lost to all of them

The shop panel is 340×720 in the **top-left slot**. So are the character sheet, the quest log, the
stash, the waypoint list and the crafting window. `scrollrt` draws `DrawSText` *before* the
left-panel content, so any of those opened over a shop would be **drawn on top of it** — while the
shop underneath went on swallowing every click inside its rect, because `CheckShopGridClick` claims
that rect first. Visible but dead: the worst of the three possible outcomes.

Both directions were reachable:

- **Opening a shop over one of them.** `StartStore` closed the character sheet, the quest log and
  the stash — but not the waypoint list or the crafting window. They were missed because the
  omission was invisible until v1.9.26: before the shop moved into that slot, a store was a box in
  the middle of the screen and nothing it opened could collide with it. Both are closed now.
- **Opening one of them over a shop.** This one v1.9.28 created. Making the click router stop
  treating a shop as modal — necessary, so items could be dragged from the inventory — also made the
  burger menu reachable while a shop is open. Character, Quests, Crafting and the Runeword Book are
  each one click from there.

For the second direction the shop closes and the newer window wins. The test sits in `DrawSText`,
which is the one place that runs every frame a shop is up; putting it at each opener would mean
finding all of them, and then finding the next one somebody adds.

## 2. The Sold tab said "You get"

`ShopPriceLabel` still answered for the screen this tab used to be. Hovering an item on Sold showed
`You get: 240` — the number is right, the direction is backwards: it is what buying it back costs,
not what the player would be paid. Now `Buy back`.

Small, but the sort of thing that makes a player distrust every other number on the panel.

## Checked and found correct

- **`CloseAllWindows`** (the space-bar master closer) already routes stores through `StoreESC`, so
  the shop closes the way it always has — one level out to the vendor's dialog. That is deliberate
  and documented in the function; no change needed for the new panel.
- **The premium save-format change round-trips.** `SaveGame` and `LoadGame` both write and read
  `giNumberOfSmithPremiumItems`, which is now `SMITH_PREMIUM_ITEMS` in both, and the array is that
  long. Every other reader of `premiumitems` (`SmithBuyPItemAt`, `ScrollSmithPremiumBuy`) is bounded
  by the same constant. Nothing still assumes 6 or 15.
- **`PremiumLevelDelta` at level 1** yields a quality level of 0 for the cheapest slot, which is what
  vanilla's `premiumlvladd[0] = -1` did at the same point. Not a new edge.
- **Drops are on mouse-down**, matching the inventory's own `CheckInvItem` routing, so a drag onto
  the shop behaves like a drag anywhere else.

## Known and deliberate

Levski's Roar is not in the close guard. It routes clicks ahead of the store and would shadow the
shop — but it opens at a monument in the dungeon, not from the town's burger menu, so it cannot be
open at a vendor. It is also the one window that is allowed to refuse to close.

`TalkID::SmithRepair` and `SmithRecharge` remain as orphaned screens, as noted in the previous
audit.

## Still unverified

The drop itself, the icons at 20px, and how many pages a real 45-item stock actually fills. Three
audits have not made any of those visible from here.
