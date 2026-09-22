# Shift+right-click splits in every container — v1.12.156

2026-09-22

> shift+right click to move part of a stack should work for stacks in every tab in stash and in
> inventory grid. double check that it does.

It did not. Two of the three places named were broken, one of them silently.

## What the check found

| Container | Before |
|---|---|
| Backpack, page 1 | works |
| Belt | works |
| Backpack, pages 2–10 | **dead**, and wrong when it was not |
| Stash, every page | **never wired** |

**The extra tabs.** `TryStartStackSplit` read `player.InvList[cii - INVITEM_INV_FIRST]` directly,
but `pcursinvitem` is a position in whichever page is *displayed*. Worse, an item hovered in an
extra tab never gets a `pcursinvitem` encoding at all — only `pcursinvtabitem` — and the shift
branch in `RightMouseDown` tested `pcursinvitem` alone. So the click was dead there; had it been
routed through, it would have offered a slice of whatever sat at that index in page one.

This is the same blind spot `UseInvItem` was fixed for in an earlier session — its comment says
so in as many words: "right-clicking a book in tabs 2-10 appeared to do nothing". The split was
never brought along.

**The stash.** No shift branch on `pcursstashitem` at all, so shift+right-click fell straight
through to `UseStashItem`.

## The fix

`TryStartStackSplit` reads through `GetActiveInvListItem`, and `RightMouseDown` gained two
branches — one for `pcursinvtabitem`, one for `pcursstashitem` — each sitting immediately before
the plain use it would otherwise fall through to, so a stack that cannot be split still drinks or
reads on a shift-click rather than doing nothing.

`TryStartStashStackSplit` and `OpenStashStackSplit` are the stash's own entry points. The stash
indexes nothing like the backpack: the index is a position in `Stash.stashList` and the page it
is drawn on is a separate fact that `RemoveStashItem` needs and cannot recover.

### The hazard the fix creates

The amount prompt does not hold the mouse. The player can turn to another backpack tab or stash
page while it is open, and the commit would then compact the wrong list or clear cell references
on the wrong page — `StashStruct::RemoveStashItem` walks `GetCurrentGrid()`.

So the tab and the page are **recorded when the prompt opens**, and every read and the removal go
through one accessor, `GoldDropSourceItem()`, which returns nullptr when the recorded page is no
longer showing or the stack has gone. The dialog then declines rather than guessing: both the
description and Enter use the same accessor, so they cannot disagree about which stack is being
split.

`RemoveStackSplit` dispatches to each container's own remover rather than a shared one — the
backpack compacts its list and fixes its grid references, an extra tab does the same without
network-syncing (no packet format exists for one), the belt resyncs the panel, and the stash walks
its page's cells and marks itself dirty. They are not interchangeable, and this is the only place
that has to know it.

Gold stays the backpack's alone: `RemoveGold` indexes `InvList` directly, and gold never reaches
this prompt from anywhere else — `StartGoldDrop` reads `pcursinvitem`, only ever set on page one,
and the stash keeps its gold as a number rather than as an item in a cell.

## Files

- `Source/control.cpp` / `control.h` — the source record, the accessor, the dispatching remover
- `Source/inv.cpp` / `inv.h` — the tab-aware read, the stash entry point
- `Source/diablo.cpp` — the two new branches in the right-click chain

Built clean, 832/832. No asset changes.

## What to look at in play

1. A stack of scrolls in backpack tab 3: shift+right-click, take part of it.
2. The same stack in the stash, on page 2 and on page 5.
3. Open the prompt on a stash stack, turn to another stash page, press Enter — nothing should
   happen, and nothing should be damaged.
4. Gold and the belt still split as they always did.
