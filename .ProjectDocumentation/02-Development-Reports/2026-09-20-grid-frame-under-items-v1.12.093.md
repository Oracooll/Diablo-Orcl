# A gold outline and grey cell lines under every grid item (v1.12.093)

**Date:** 2026-09-20 · **Version:** v1.12.093 · **Tests:** 831/831 (after the test update)

User: "when items are visualized on inv grids everywhere except item slots on body, add the thin 1px grey grid
behind their sprites + add 1px gold outline of their rectangular grid. Gold outline + grey grid inside. 1px each."

## Where

`InvDrawSlotBack` (inv.cpp) is the one backing call under every item - the backpack, the stash, the shop grid -
and since this version the Cube's grid too (levski_roar.cpp calls it before `DrawItem`, so the Cube's items get
the tier tint, the stone and the frame the pack has). It gains `bool gridLines = true`; the two body-slot calls
in `DrawInv` pass false. The belt has no backing call and is unchanged.

## What is drawn

After the tint and before the sprite: 1 px grey lines (`GridFrameGrey = PAL16_GRAY + 9`) on every 28 px cell
boundary inside the footprint, then the 1 px gold outline (`GridFrameGold = PAL16_YELLOW + 4`) on the footprint's
edge. A 1x1 item gets the outline only; a 2x3 gets the outline and three inner lines. Both indices are the
tunables; the line primitives clip to the surface.

## The test

`InvTest.EtherealItemsWearAPurpleTintAndNoBorder` asserted the pre-v1.12.093 rule and failed on the first build
(its centre sample sat on the x 28 grey line, its corner on the gold). It is now
`EtherealItemsWearAPurpleTintAndAGridFrame`: the tint is sampled inside a cell, the corners are asserted gold, the
x 28 and y 56 boundaries grey, and a body-slot call (gridLines = false) still tints corner and centre alike.
