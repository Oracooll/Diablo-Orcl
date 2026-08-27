# Hover prices, and the overruled button (v1.9.73)

**Date:** 2026-08-27
**Version:** 1.9.72 → 1.9.73
**Tests:** 563, of which 561 pass — the two standing baseline failures, unchanged.

---

## Overruled, correctly

> "Repair All's cost on hover - make it on hover, overrule the current way."

Done. I had shipped the price permanently on the button and argued in the last report that the number
*is* the decision, so hiding it makes the button something you interrogate. That argument was fine as
far as it went; it was also answering a question that had already been settled — the original request
said "when hovered over" plainly, and I substituted my judgement for a stated preference on the
user's own panel.

Three plain words at rest, the number only while pointed at. The row reads quieter for it, which I
had not weighed.

## Hovering an item now says what it is worth

> "When i am in a vendor shop hover pop-ups to show sell price of items i hover over."

Appended to the hover the player is **already reading**, not given a popup of its own — "what is this
worth" is only ever asked about an item already under the cursor.

`ShopSellOfferFor` asks the same two questions the sell paths ask: `IsWitchShopScreen` decides whose
rules apply, and that vendor's own `SellOk` decides whether the item qualifies. Sharing those
judgements is the point — **a quoted price the shop then refuses to honour is worse than quoting
nothing**, and Adria refusing armour while a price sat beside it would have been exactly that.

So the line is silent when the vendor will not take the item, silent for gold, and silent when no
vendor is open.

---

## Still queued — six items

The Rare tab; the Set shop; the hammer-cursor Repair rework; Refresh on Basic/Rare/Supplies; and
docking every limestone window to the bottom of the screen.

The two new tabs are the next piece and the layout is now waiting for them — the third tab row exists
and is empty. Worth a look at the button rows at 16px before I build on top of them; if they read as
cramped, that is the moment to decide about the grid row rather than after.
