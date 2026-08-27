# The tabs move outside the panel (v1.9.83)

Date: 2026-08-27
Version: 1.9.83
Tests: 562/564 (the two standing baseline failures)

---

## The tab column

> "cant we just move basic, magic, rare, unique, set, supplies, sold to the right of the store
> window as vertical tabs one under the other?"

Yes — and it is the answer to a problem the two previous layouts were both solving the wrong way.

The tabs were a three-row strip **inside** the panel, costing three of the 106 pixels between the
title band and the pinned grid. Yesterday's fix bought that space back by turning every control on
its side, which paid for it with vertical labels — slower to read, and a stacked-glyph renderer to
maintain. **Both attempts were rationing the same scarce band.**

The band was never where the tabs had to be. The panel is 340 wide against a screen at least 640, and
**the column to its right is empty**. Moving the tabs there costs the panel nothing, so:

- the tabs get ordinary horizontal labels at a comfortable size, in 104×26 buttons;
- the services and bulk actions go back to plain wide rows inside the panel, with 26px each and
  genuine slack for the first time;
- the vertical-label renderer is deleted.

The grid keeps all sixteen rows, which was the standing constraint.

### The one thing that needed care

The tab column **floats over the play area**, not over a panel. Every router that asks "is the
pointer on the shop?" had been asking `GetShopPanelRect().contains()`, and a router left un-updated
would let clicks fall straight through the tabs to the world — which is precisely the bug this panel
has already had once, when towners were being named through it.

So the question is asked in one place now: `IsPointOverShop(position)`, used by the click router, the
hover router, the interface test and the shop's own two early-outs.

**It is a predicate, not a bounding rect, and that distinction is load-bearing.** The column is a
short stack partway down the panel's right side; a rectangle enclosing both would also enclose the
tall empty strip above and below it, swallowing world clicks in a band a hundred pixels wide.

One deliberate ordering choice: the tab test runs **before** the held-item branch. Dropping an item
on a tab switches tabs rather than selling it — a mis-drop on the shop's own navigation should not
cost you a sword.

---

## Magic Oracool items on the Basic tab

> "why am i seeing magic oracool items in basic shop?"

A real defect, and mine. `StockOracoolVendorItems` built its items through `SetupAllItems` — **the
drop path**. That rolls affixes and can roll a quality tier on top of them. Right for something
falling out of a monster; wrong for this shelf.

Griswold's shop has a rule and the rule is legible: **Basic sells plain gear, Magic sells affixed
gear**, and the tabs beyond those sell what their names say. An Oracool base arriving on Basic
already magical broke the one promise that tab makes.

They are built the way `SpawnSmith` builds its own basic stock now: attributes from the base row, a
vendor ilvl stamp, a chance of a base **tier** — and no call to the affix roller at all. A base tier
is not an affix; it is what the item *is*, and Griswold's vanilla basics already roll for one.

The createInfo stamp stays a bare level rather than `CF_SMITH`, for the reason that function's header
has recorded since it was written: `CF_TOWN` is the route that re-derives an item's index by
replaying its seed, and a town-stamped Oracool item comes back after a reload as something else.

**Worth flagging:** affixed Oracool bases now appear only as drops. Griswold's Magic tab is the
premium stock, which rolls through the droppable pool that excludes Oracool items by design — so
there is currently no shop that sells an Oracool base *with* affixes. Say the word if that gap should
be filled.

---

## Verification

562/564 at `ctest -j 2`, the two standing baseline failures and nothing else.

## To look at in game

- The tab column down the right of the shop window, horizontal labels, active tab filled solid.
- Click *past* the tabs — into the gap above and below them — and confirm the world still responds.
- Griswold's Basic tab: the Oracool gear (Shoulders through Spectral) should be plain white now,
  possibly with a tier prefix, never blue.
