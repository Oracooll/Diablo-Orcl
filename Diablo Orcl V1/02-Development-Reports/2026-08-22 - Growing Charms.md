# Growing Charms

**Version:** v1.9.21 -> v1.9.22
**Date:** 2026-08-22
**Phase:** D2MXL-to-ORCL Phase 3

## What shipped

Three charms whose value scales with how many milestones the character has claimed:

| Charm | Base | Per milestone |
|---|---|---|
| Charm of Trials | +8 life | +3 |
| Charm of Deeds | +4 all resistances | +2 |
| Charm of Legend | +5% magic find | +2 |

Base is deliberately **below** the fixed charm covering the same stat. At eight milestones a growing
charm passes its fixed cousin — so choosing between a Charm of Vigor and a Charm of Trials is
"good now" against "better later", which is a real decision rather than a strictly-better item.

They are charms in every structural sense — `IsOracoolCharmIdx` includes them — so the active cap of
three, the drop walk and the stash sort all pick them up without being told they exist.

## The plan budgeted a format bump. It was not needed

Phase 3 was planned to "ride Phase 1's byte" on the assumption that a growing charm needs per-item
state. It does not.

Milestones already live in the hero chunk tail (shipped v1.9.20), so a growing charm's power is a
**pure function of something already persisted**. No field on the item, no `OracoolItemFormatVersion`
move, no save invalidated. The value is recomputed every time the totals are built, which is the
same thing every other derived property in this fork does.

**The trade, stated plainly:** growth belongs to the **character**, not the object, so two copies of
the same charm are worth the same. There is no charm with a private history.

That is arguably the better reading anyway — the charm is a record of what you have done rather than
a thing with an invisible past — and it is what genuinely ties this phase to Phase 2 instead of
merely shipping after it. The test asserts the two-copies behaviour explicitly, because it is the
thing a reader would otherwise assume works the other way.

## The one real refactor

`ApplyCharmToTotals(uint16_t, ItemBonusTotals&)` became
`ApplyCharmToTotals(const Player&, uint16_t, ItemBonusTotals&)`, and `CharmEffectLine` the same. A
growing charm cannot be evaluated from its index alone.

The player is threaded to **all** charms rather than branched at the call site, so there stays one
door onto "what is this charm worth". The provider passes a small context struct carrying both the
owner and the totals, since `ForEachActiveCharm` takes a plain function pointer.

## Decisions worth keeping

- **`GrowingStat` is an enum, not a field name.** Adding a growing charm that grows something new is
  then a compile error here rather than a charm that silently grows nothing.
- **The description prints the current value AND the rate.** A charm whose worth changes silently is
  a charm the player cannot compare against the fixed one beside it, and a line that only stated the
  rate would be unreadable mid-run.
- **Charm of Deeds grants all three resistances**, which is why its per-milestone number is the
  smallest of the three.

## Tests

`GrowingCharmsScaleWithClaimedMilestones` pins that all three are recognised as ordinary charms
(otherwise they escape the active cap), that the growth is exactly the flat advertised rate — a
charm growing faster than its own description would make that description a lie — that the three
move different stats, that two copies are worth two charms, and that a **fixed** charm is untouched
by any of it. That last one is the guard against the growth branch catching the wrong charms.

Icon strip 486 -> 489 frames; `oracool.mpq` repacked.

## What to look at in game

Pick up a Charm of Trials early and read its line. Hit level 20 and read it again — the number
should have moved by exactly three, and the log should have said a milestone was met.
