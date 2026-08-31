---
date: 2026-08-16
version: 1.7.10
area: Megaplan Phase 2.3 - respec at Adria
---

# Adria Reclaims the Points

The respec, exactly where the megaplan put it: Adria's shack, for gold.

A new line in her menu - "Reset skill points (N gold)" - between Recharge staves and Leave. The
price is RespecCost from 1.7.9: 500 gold per sunk point, floor 1000, printed on the line itself
so the decision is made before the click. With nothing invested the line sits grey and
unselectable; with too little gold the click lands on the standard not-enough-gold screen and
returns. On success the gold is taken, every invested point returns to the unspent pool
(RefundAllSkillPoints, already pinned by 1.7.9's tests), the event log records the transaction,
and the menu rebuilds so the line greys out immediately.

"Leave the shack" moved from line 20 to 22 - measured against the store panel first: line 22
renders at y=296 of the 320px panel and the click handler's bound is y<=320, so both the draw
and the mouse reach it. Keyboard navigation follows the selectable flags and needed nothing.

**State: 398 tests, the usual two.** The store flow itself is input machinery (play-test item);
the refund math it calls was pinned in 1.7.9.

## Phase 2 remaining

The run toggle, then the aura gameplay pass (its plan doc is written), then the skill tree UI
polish if the sheets prove too flat in play.
