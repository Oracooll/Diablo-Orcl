# The shop shelf is one page (v1.9.70)

**Date:** 2026-08-27
**Version:** 1.9.69 → 1.9.70
**Tests:** 563, of which 561 pass — the two standing baseline failures, unchanged.

First instalment of the sixteen-item shop request. Three items done; the rest still queued.

---

## The grid no longer pages

> "BASIC items tab sometimes offers more than one tab worth of items and in those cases a next/prev
> tab arrow buttons appear. avoid this from hapenning. keep available items up to 1 page worth of
> quantities."

**This reverses a decision made deliberately three days ago**, so the reasoning is worth stating
rather than quietly overwriting.

`PlaceStock` paged. Its comment argued that silently unbuyable stock is the worst available outcome —
worse than a second page — and on its own terms that was correct. Given a shop that has *already*
generated forty-five items into a grid holding roughly thirty, hiding some of them is indeed the
worst way to resolve it.

**The premise was the problem, not the conclusion.** A vendor does not have to generate more than it
can show. What Griswold carries is now *defined* as what fits: the overflow was never on the shelf,
so nothing is dropped from a list the player could otherwise have reached.

The page arrows and the "1/2" label were already guarded by `pageCount <= 1`, so they disappear on
their own now that `PageCount` returns a constant.

One detail in the packing worth keeping: an item that does not fit does **not** stop the loop. A
small item after a large one may still fit a gap the large one could not use, so the shelf packs as
densely as first-fit allows.

## Uniques are a page-full, and the INI is one switch

> "Uniques items quantity to be a page-full. Remove Number of Uniques items from INI file. Unique
> shop to be only ON/OFF settable in the INI."

`SmithUniqueItemsMaximum` 8 → 40, and the generator now asks for that maximum rather than reading a
count from the INI. `Griswold Unique Shop Items` is gone entirely — from `options.h`, `options.cpp`,
the INI writer, and demo mode's reset list. `Griswold Sell Unique Items` remains as the on/off
switch.

Forty rather than a number computed from the grid, deliberately: the grid measures **cells**, and
uniques run from a 1×1 ring to a 2×3 breastplate, so no single item count fills 160 cells. Since the
shelf is now defined by what fits, this only has to be comfortably *more* than a page holds — the
page decides where the stock ends.

## Supplies fills a page as a consequence

Adria's shelf is her 45 items plus Pepin's potions, which already exceeds a page in cells. With the
single-page rule that tab now reads as full without any new stock, so the "add more staves and books
if you need to" half of that request was not needed. **Worth checking in play** — if it still looks
sparse, the stock counts are the knob.

---

## Still queued — ten items

Hover sell-prices over inventory items; a RARE tab; a SET shop; the hammer-cursor Repair rework;
Repair / Repair All / Recharge as a text-button row; Repair All's cost on hover; Refresh and Refresh
Until on their own row; Refresh added to Basic/Rare/Supplies; and docking every limestone window to
the bottom of the screen.

The two new tabs are the next piece of work and they carry a structural cost worth flagging now: the
tab strip reserves two rows of three, and Griswold would have **seven** tabs. `DrawShopTabRow`
asserts on exactly this, so the strip has to grow before either tab can be added.
