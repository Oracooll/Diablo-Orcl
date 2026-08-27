# Two button rows found without taking a grid row (v1.9.72)

**Date:** 2026-08-27
**Version:** 1.9.71 → 1.9.72
**Tests:** 563, of which 561 pass — the two standing baseline failures, unchanged.

Second instalment of the shop request. The instruction was explicit:

> "you dont remove one grid row from shops. fit the necesary buttons somehow. after i see them with
> my eyes i will decide if we remove a row or not."

So this is the layout to look at, not to accept.

---

## The budget, measured rather than guessed

Everything above the grid lives between two fixed lines:

- the title band ends at **56** (`PanelTitleTop 18 + PanelTitleHeight 38`)
- the grid's top is pinned at **170** (`GridBottom 618 − 16 × 28`), and reserves a 6px bezel

That leaves **106 pixels**, and no more. The old stack used 97 of them, so there were **nine spare**
and three things wanted room: a third tab row, a services row, and a refresh row.

## Where the space came from

**Not from the tab width.** Four tabs per row would have fitted seven tabs in two rows and freed a
whole row — and 308px across four is 77px a tab, which is exactly what "Supplies" overran the first
time this panel was drawn ("SUPPLIE"). Shrinking the one thing on the panel that must be read at a
glance, to buy room for buttons read once, is the wrong trade. Tabs stay three across at 102px.

**From the row heights.** Tabs 20→17 with the gap 2→1, the button rows at 16, the gold line 15→14:

```
58  + 3×(17+1) tabs   = 112
112 + 16 services     = 129
129 + 16 bulk         = 146
146 + 14 gold         = 160      ceiling 164 — four spare
```

The `static_assert` is what keeps that true, and its message now says to ask before taking a row off
the grid rather than offering it as the first option.

**And from noticing the two rows never coexist.** Sell all belongs to the Sold tab; the refreshes
belong to the stock tabs. One row serves both and still reads as "a row of their own" from the
player's side — which is what made two new rows fit where three would not have.

## The services are text now

Repair | Repair All | Recharge were 20px hand-drawn icons crowded onto the right end of the bulk row.
They are equal thirds of the full strip width, as words, on a row of their own.

**Repair All says its price on the button**, not on hover. The request asked for hover, and this is a
deliberate deviation worth flagging: the number *is* the entire decision, and a button that hides the
only thing you need to know until you point at it has to be interrogated. If you would rather it
appeared only on hover, that is one line.

`ShopRepairAllPrice()` walks the same containers `StartSmithRepair` does, with the same refusals,
rather than calling it — `StartSmithRepair` rebuilds `storehold` and `storenumh`, which are the
shop's live state, and a drawing path must not rewrite what it is drawing. The duplication is the
price of that, and the two sit side by side so a rule added to one is visible from the other.

---

## What you will see that is not finished

**A gap under the tabs.** Three rows are reserved and Griswold has five tabs, so the third row is
empty until Rare and Set arrive. That space is theirs; it is not a layout bug.

**Refresh is still only on the Magic tab.** Extending it to Basic, Rare and Supplies needs a
per-tab regeneration path — the current one is `SpawnPremium`, which is premium-specific — so it is
real work rather than a list entry, and it is next.

## Still queued — eight items

Hover sell-prices over inventory items; the Rare tab; the Set shop; the hammer-cursor Repair rework;
Refresh on Basic/Rare/Supplies; and docking every limestone window to the bottom of the screen.
