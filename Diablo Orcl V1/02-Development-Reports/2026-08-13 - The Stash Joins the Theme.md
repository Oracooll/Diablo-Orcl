---
date: 2026-08-13
version: 1.1.85
area: UI / Stash
breaking: stash save format (deliberate)
---

# The Stash Joins the Theme

## What was asked

> Apply theme window 340x720 to Stash.
> 1. Title
> 2. Gold counter + browsing buttons + Sort button
> 3. Expand the inv grid as much as possible from below the necessary buttons down to about 660
>    pixels. Below that we enter area of central HUD.

(The request said 360x720; 340 was used and confirmed - every other window in the set is 340, and
breaking that for one panel would show.)

## The window

The sixth and last window onto the shared theme. `data\stash.clx` is no longer loaded; the panel is
half-transparent fill under the ornate bevel, with an outlined FontSize30 "STASH" and the same
separator as the rest.

```
0..24      top margin
24..74     title
74..77     separator
77..101    gap
101..127   page row:  <<  <   Page N / 100   >  >>
131..153   gold row:  GOLD: n                    SORT
161..654   the item grid
654..720   clear - the central HUD begins around 660
```

The four page-navigation arrows keep their original art: they are purpose-drawn controls rather than
chrome, and are the one part of the old panel worth carrying over. **Sort** became a text button like
the character sheet's RESET, which took it out of `StashButtonRect` - so that table dropped from five
entries to four and the indices after Sort shifted down by one. `StashButtonPressed` indexes both the
table and the art, so the switch in `CheckStashButtonRelease` was renumbered to match.

## The grid, and how "as much as possible" is enforced

10x10 became **10x17**, running y 161..654.

The row count is not a number someone eyeballed - three `static_assert`s pin it:

```cpp
static_assert(StashGridBottom <= 660, "Stash grid now overlaps the central HUD");
static_assert(StashGridBottom + StashCellPx > 660, "Another stash row would still fit - raise StashGridRows");
static_assert(StashGridLeft >= StashMargin, "Stash grid is wider than the panel's margins allow");
```

The second one is the interesting one: it asserts the request itself. "As much as possible" is
checked by the compiler rather than claimed in a comment, so a later change to the row height or the
control rows that frees up space will fail the build instead of quietly wasting it.

Everything else fell out for free, because the existing code was well factored: every hit-test -
`FindTargetSlotUnderItemCursor`, `CheckStashHLight`, paste, cut - already derived from
`GetStashSlotCoord` and `StashGridRange`, both computed from the constants. Changing the constants
moved all of it.

## The save format, deliberately broken

`StashGrid` is `std::array<std::array<StashCell, StashGridRows>, StashGridColumns>` and is written
cell-for-cell into the stash file, so 170 cells per page is not readable as 100.

`StashVersion` bumped 1 -> 2. `LoadStash` already rejects a mismatched version with a player-facing
message ("Items already in the Stash could not be recovered; new items placed in the Stash will be
saved correctly from now on"), so this takes the supported path rather than corrupting anything -
but it does mean **existing stash contents and stashed gold are lost on first load**.

A migration reading the old 10 rows into the top of the new grid would have avoided that and was the
first plan. It was dropped on an explicit call: *"wipe the stash, don't worry"* / *"we are developing
a product, saves are not important."* Recorded here because that is the kind of decision worth being
able to find later.

`StashGridColumns`/`StashGridRows` moved to stash.h precisely because three places derive from them -
the array type, the window layout, and `IsStashSizeValid`'s expected file size. That last one still
had a hardcoded `10 * 10` and would have silently rejected every new save as "size invalid" if it had
been missed.

## The click-through rule

The stash was the **last left-hand window still routing on `GetLeftPanel()`'s 320x352**. At 720 tall
that would have leaked every click below y=352 straight to the ground - the same bug the character
sheet, quest log and waypoint list had. `GetStashPanelRect()` is exported and
`GetLeftPanelContentRect()` and `GetPanelPosition(UiPanels::Stash, ...)` both use it now.

## Files

- `Source/qol/stash.h` - grid dimensions, `GetStashPanelRect()`.
- `Source/qol/stash.cpp` - layout constants, themed `DrawStash`, relocated controls, text Sort.
- `Source/loadsave.cpp` - `StashVersion` 2, `IsStashSizeValid` derived from the constants.
- `Source/control.cpp` - routing and panel position.

## Verification

Debug config builds clean at `1.1.85`; full suite 351/353, the two known pre-existing failures.
Geometry is enforced by the three `static_assert`s above.

Not seen in game. To confirm: the four page arrows and SORT all respond; clicking the gold total
opens the withdraw prompt; items place and pick up across all 17 rows (especially the bottom ones,
which is new space); clicking empty panel space does not walk the player; and the first load reports
the stash reset rather than misbehaving.
