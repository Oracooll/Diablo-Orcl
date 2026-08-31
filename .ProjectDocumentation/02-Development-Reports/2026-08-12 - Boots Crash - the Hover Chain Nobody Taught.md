---
title: 2026-08-12 - Boots Crash - the Hover Chain Nobody Taught
date: 2026-08-12
tags: [dev-report]
summary: Equipping boots crashed the game - not in the equip, but in the hover check one frame later. CheckInvHLight's hand-written slot chain never learned the six new slots, left its item pointer null, and dereferenced it. CheckInvCut had the same hole, which would have made the new items impossible to un-equip.
---

# Boots Crash: the Hover Chain Nobody Taught

User report: "I tried putting boots in the boots slot - crash!" On 1.1.15, which already carried the slotSize fix - so this was a different crash.

## The mechanism

The paste itself succeeded. The crash fired one frame later, in `CheckInvHLight` - the hover check that resolves which item the cursor is over.

That function maps the hovered slot to an equipped item through a hand-written if-chain: `SLOTXY_HEAD`, the rings, the amulet, both hands, `SLOTXY_CHEST`, then the backpack and belt ranges. `Item *pi` starts as `nullptr`, and the six new slots match no branch, so control falls straight through to `if (pi->isEmpty())` - a null dereference.

Why "putting boots in the slot" is the exact trigger: while dragging, the hover path doesn't resolve items. The paste empties the cursor **while it is still sitting on the boots slot**, so the very next frame is the first bare-handed hover over a new slot - and the crash. Any bare-handed pass over any of the six slots, full or empty, would have done the same.

This also retroactively explains part of the earlier amulet crash report: with a new-slot item equipped by autosave-reload, both this null-deref and the slotSize overrun were live at once.

## The sibling hole: un-equip

`CheckInvCut` - click an equipped item to pick it back up - is built from the same pattern: one hand-written block per slot. No block for the six new slots, so an equipped pauldron or boot **could not be removed at all**: the click fell through and did nothing. Not a crash, just a one-way slot, which the user would have found thirty seconds after the crash was fixed.

Both fixed with a single range branch each rather than six copied blocks - `SLOTXY_*` and `inv_body_loc` are parallel across the whole equipped range (there is a `static_assert` pinning that), so `r` *is* the body location.

## The pattern, named

This is the third hand-enumerated slot structure to break on the same change, after `DrawInv`'s 7-entry `slotSize[]` and the two test fixtures' 7-entry pack initialisers. The audit heuristic that found none of them: greps for `NUM_INVLOC` and `default:` find code that *mentions* the range - the defects live in code that *enumerates* it, which by definition does not mention it. The reliable search key turned out to be `SLOTXY_CHEST` / `INVLOC_CHEST`: the last vanilla slot appears at the end of every hand-written chain, exactly where the new slots should have been added. Both of today's holes were found that way, in one grep.

`CheckInvHLight`'s fall-through is also hardened - `pi == nullptr` now returns "no hover" - so a hypothetical fourteenth slot degrades to a missing tooltip instead of a crash.

## Verification

Debug build clean at `ORACOOL_VERSION` **1.1.16**. Tests **348/350**, the same two pre-existing failures.

Play-test: equip and un-equip each of the six new slots (bare-handed hover over each, full and empty, is the crash case); shift-click un-equip into the backpack; and the boots specifically, since that was the report.

## Related

- [[2026-08-12 - Equipment Slots Play-Test Fixes]]
- [[2026-08-12 - Six New Equipment Slots]]
