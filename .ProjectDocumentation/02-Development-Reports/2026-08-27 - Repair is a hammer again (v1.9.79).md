# Repair is a hammer again (v1.9.79)

**Date:** 2026-08-27
**Version:** 1.9.78 → 1.9.79
**Tests:** 564, of which 562 pass — the two standing baseline failures, unchanged.

---

> "REPAIR button - change mechanic. Make it work as the vanilla Repair Item skill - summon a Hammer
> cursor instead of the regular cursor, then click on item i want repaired and it gets repaired,
> consuming the necessary amount of gold. Basically borrow the entire mechanic behind the vanilla
> Repair Item skill but produce 100% durability recovery."

**Borrowed, not reimplemented**, which is the whole point of the request and worth being literal
about. The shop's hammer *is* `CURSOR_REPAIR` — the same cursor state the Repair skill sets — so it
inherits the hammer graphic, the click-an-item targeting, and `TryIconCurs`' routing across the
backpack, the extra inventory tabs and the stash. None of that is written twice.

One boolean separates them. `IsShopRepairCursorArmed()` is checked at the top of the existing
`CURSOR_REPAIR` branch:

- **skill**: partial durability, free — unchanged
- **shop**: full durability, charged

`RepairPriceFor` returning 0 doubles as the "not a valid target" test, so clicking an undamaged item
costs nothing and does nothing rather than taking gold for no work. Clicking something unrepairable
does the same.

## What did not change

**Dropping an item on the button still repairs it.** The hammer is what the button does when your
hand is *empty*; the drop path is untouched. Two gestures, same outcome, and neither had to be given
up for the other.

## The disarm, which is the part that could have bitten

A hammer left armed by a shop the player has since walked away from would repair the next thing they
clicked and charge them for it. It is cleared in `InitStores` alongside the buyback shelf — the same
place every other piece of per-visit shop state is reset — and again immediately after the click.

---

## Still queued — three items

The Rare tab; the Set shop; and Refresh on Basic/Rare/Supplies.

Those three are one piece of work, not three: Refresh has to reach a Rare tab that does not exist
yet, and both new tabs need the same "curated shelf" plumbing the Unique tab has — a stock array, a
generator, an INI toggle, and a case in each of about twenty switches. **Doing that by copying the
Unique tab twice would be the wrong shape**; it wants generalising into one shelf mechanism first,
which is why I have not started it at the tail of a long session.
