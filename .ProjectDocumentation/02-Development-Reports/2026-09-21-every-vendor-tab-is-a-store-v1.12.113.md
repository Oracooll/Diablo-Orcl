---
version: v1.12.113
date: 2026-09-21
area: UI / stores
tests: 832/832
---

# Every vendor tab is a store screen, and three tiles closes any counter

## The ask

> we need to make sure all tabs for all vendors are to be considered Stores, not windows.
> and they should share same behaviour, same sounds, same X button, same walk away (3 tiles) logic.

## One cause, not three symptoms

v1.12.112 fixed the Salvage page's X, its walk-away and its talk-to-towner close as three separate problems. They
were one: **the page was a window, not a store screen.** `StartStore(SmithTransmute)` set `stextflag` to None and
opened a window, so every piece of store machinery looked straight past it.

Now `stextflag` stays on `TalkID::SmithTransmute` while the page is up. That single change hands it the
walk-away, the talk-to-towner close, ESC and the overlap-closing rule that every other tab already had, instead
of each needing its own special case.

## A tab is not a grid

`IsShopGridScreen` was literally `return IsShopTab(id)`. Those were one function because **no tab had ever been
anything but a grid of stock**, and the Salvage tab is the first that is a tab without being a shelf. Keeping
them merged is what once made the Salvage tab draw an empty stock grid.

Separating them meant auditing all 21 remaining uses and deciding, one at a time, which question each was really
asking. The rule that came out of it:

> **`IsShopTab` wherever the panel's RECT is what matters** - routing, hover, world targeting, modality.
> **`IsShopGridScreen` wherever a SHELF OF ITEMS is what matters** - cell navigation, prices, the service cursors.

Four sites were asking the wrong one:

| Site | Was | Would have happened |
| --- | --- | --- |
| `diablo.cpp` LeftMouseDown | grid | **the page is MODAL**: every world click swallowed, the player frozen, never able to walk away |
| `cursor.cpp` targeting guard | grid | towners behind the page named and highlighted through it - the 2026-08-23 report, returning |
| `control.cpp` IsOverInterface | grid | the world clickable through the page |
| `shop_grid.cpp` IsPointOverShop | grid | the tab column outside the shop's footprint; a tab click falls through to the world |

**The modal one is the one to remember.** A store screen in this engine is modal *unless* it is a shop grid - the
grid was deliberately made non-modal so items can be dragged out of the inventory to sell. The moment Salvage
became a store screen that is not a grid, it landed on the modal side and froze the player where they stood, so
they could never walk away from the counter, which is precisely what making it a store was for.

**It passed all 832 tests in that state.** Nothing exercises click routing with a store open. It would have shown
up the first time the page was opened and a step taken.

## Keeping the flag and the page in step

Two pieces of state now have to agree: `stextflag == SmithTransmute`, and the window being open. Reconciled once
a tick in `UpdateStoreState` rather than maintained at every exit - the argument this file already makes for the
service cursor. `stextflag = TalkID::None` appears a dozen times here, and a list of places to also close the page
would have to stay complete forever.

A refused close still stands: a page holding items with no room to give them back says so in red, and the flag
goes back to the tab rather than the two drifting apart.

## Three tiles, every counter

`walkAwayTiles` was five everywhere but Wirt's. It is three for every vendor now, and the artisans' pages match.

The paragraph arguing for five is kept in place, marked overruled, because it states the trade: three is one step
off the two tiles `TalkToTowner` needs to OPEN a shop, so shops close far more readily than before. Easy to raise
if it proves twitchy.

## What survives from v1.12.112

The X (the docked page uses the shared corner) and the window handling for **Ogden's and Gillian's** pages, which
are reached from a dialog row rather than a tab column - there is no store screen for them to be. They no longer
claim Griswold's page, so one mechanism owns it.

## Build

Debug, clean. 832/832.

## Not verified

None of this has been seen in play, and the tests cannot see it. What to check: that the Salvage page does not
freeze the player, that three tiles is not too twitchy at every counter, that clicking Griswold with Salvage open
puts his dialog up, that Escape closes it, and that nothing behind the page can be clicked or named through it.
