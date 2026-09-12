# No item on an extra inventory tab was ever hovered

2026-09-12 — v1.11.081

## What was reported

> 4 socket shiels with 2x2 assets dont show gold rings when in my inv grid. i now have 1 canticle
> 4 sock i cant see the gold rings when i hover over it.

## What it actually was

Not size. Not socket count. Not shields.

`DrawInvBackpack`'s draw loop decided whether an item was under the cursor like this:

```cpp
const bool hovered = ActiveInventoryTab == 0 && pcursinvitem == ii + INVITEM_INV_FIRST;
```

and `CheckInvHLight` (`Source/inv.cpp:3486-3493`) **deliberately** leaves `pcursinvitem` at `-1` for
an item on an extra tab. Its own comment says why: the legacy drag/drop code reads that value and
assumes tab-1 indices, so an extra tab's item must not appear there. The hover is recorded in
`pcursinvtabidx` / `pcursinvtabitem` instead, which is how the tooltip and the single-shot
cursor actions (Identify, Repair, Recharge, Oil) still reach it.

The draw loop never asked those. So from **tab 2 onward nothing was ever hovered**: no gold socket
rings, no socketed stones, and no hover outline either — and `hovered` gates all three.

Why it presented as a 2x2 problem: the later tabs are where the big two-by-two items accumulate, so
the failing cases the user met were shields. A 1x1 ring on tab 4 was exactly as invisible; a 2x2
shield on tab 1 worked fine. The size correlation was real in the observation and absent in the
cause.

The socket overlay itself was never at fault. `DrawSocketOverlay`
(`Source/oracool/socket_overlay.cpp:79`) lays sockets out as `i % columns` / `i / columns` with no
table and no per-size branch, so a 2x2 with 4 sockets maps cleanly to the four cells. Its only
geometric bail-out is `row >= cells.height`, which cannot fire for a legal footprint.

## The fix

The rule moved into `IsActiveInvItemHovered(int listIndex)` (`Source/inv.cpp:972`, exported in
`inv.h`): tab 0 answers from `pcursinvitem`, every other tab from the tab-cursor pair, with the
tab index checked as well as the item index so a hover on tab 2 cannot draw on tab 3.

It is a function rather than a fixed expression because **getting this wrong is invisible**. The
only things that read it are the socket rings and the hover outline, so a wrong answer looks like
missing art, not like a bug — which is exactly why it survived from the day tabs were added until a
user counted rings on a Canticle. `pcursinvtabidx` and `pcursinvtabitem` gained
`DVL_API_FOR_TEST` so the test can set them.

Drawing is all this decides. Drag/drop still goes through `pcursinvitem` and is untouched, so the
reason the `-1` exists is preserved.

## The test

`InvTest.EveryTabSItemCanBeHoveredNotOnlyTheFirstTabS` pins six facts: tab 0 still answers from
`pcursinvitem`; tab 0 must **not** consult the tab cursor; an extra tab's hovered item now says yes;
only the item under the cursor says yes; a hover on tab 2 does not light up tab 3; and nothing
hovered means no.

**722/722 tests pass** (up from 721). Debug and Release build clean, RTM refreshed. No asset
changed, so no MPQ repack.

## What to look for in play

Hover the 4-socket Canticle again, on whatever tab it is on — four gold rings. Worth checking the
hover *outline* on a later tab too, since it was missing for the same reason and is the more
noticeable of the two once you know to look.

If the rings are still absent, the remaining explanations are that its `_iSocketed[]` entries are
not `EmptySocket` (in which case `DrawStone` runs and draws a gem sprite instead of a ring), or that
the item is a *named* unique on the Canticle base rather than a plain one — a named unique carries
its own `IPL_INVCURS` frame, and it is that frame's row in the width/height tables that decides the
footprint the rings are laid out in.
