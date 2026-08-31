---
title: 2026-08-11 - Inventory Window Live
date: 2026-08-11
tags: [dev-report]
summary: The 320x660 inventory goes in - own rect, 10x7 grid, generated InvRect, and the composed artwork drawn in place of the original CEL. Includes a save-corruption bug this work surfaced in the Town Portal staff filter.
---

# Inventory Window Live

Follows [[2026-08-11 - Inventory Panel Geometry]], which established the layout and the artwork.
This is the wiring: the panel is now what the game actually draws and hit-tests.

## The panel owns its own rect

`GetPanelPosition(UiPanels::Inventory, ...)` previously shared a `switch` case with
`UiPanels::Spell` and returned `GetRightPanel()`. Splitting `Inventory` into its own case
returning `oracool::GetInventoryPanelRect()` redirected nearly all drawing in one edit.

Hit-testing did not follow automatically: `inv.cpp` reached for `GetRightPanel()` directly in
seven places. Those are now the inventory rect. Four `inv_test` failures pointed straight at this
- the tests position the cursor through `GetPanelPosition`, which had moved, while the code under
test was still measuring against the old panel.

## InvRect is generated, not typed

The old table was 55 literal rectangles. At 10x7 it would have been 85, and every one would need
re-typing by hand whenever a slot moved - which happened three times during the layout pass alone.
`MakeInvRect()` now builds it at compile time from `inventory_layout.h`: equipment rects from
`GetEquipSlotRect`, grid cells from `GridOrigin` and `CellPx`. The artwork is composed against
those same constants, so the picture and the hit-testing cannot drift apart.

The belt entries at the end are still literals. They are positions on the main HUD, not this
panel, and were deliberately left alone.

`DrawInv`'s equipment draw positions come from `InvRect` too, rather than a second hardcoded
table - its first seven entries are the equipment rects in exactly the `INVLOC` order the draw
loop wants.

## Seven rows broke controller navigation

`plrctrls.cpp` enumerated row boundaries explicitly - `IsAnyOf(Slot, SLOTXY_INV_ROW1_FIRST,
ROW2_FIRST, ROW3_FIRST, ROW4_FIRST, SLOTXY_BELT_FIRST)` and similar, in about a dozen places.
That pattern stops scaling the moment a row is added.

Replaced with `IsAtInvRowStart()` / `IsAtInvRowEnd()`, modulo tests that hold for any row count.
One subtlety at the stash hand-off site: both helpers also report true for the belt's first and
last cell, so that call site excludes the belt explicitly - it is not part of this panel and must
not hand off to the stash.

## A save-corruption bug, surfaced by accident

`pack_test` failed with a staff going in as "Fire Wall" and coming back as "Lightning". That test
compares an item against its own pack/unpack round-trip, so this was not stale fixture data:
**staves were changing spell across save and reload**. Disabling the Town Portal filter made all
58 pass, confirming the cause.

The filter (added in [[2026-08-11 - HUD Follow-ups and Town Portal as Built-In Ability]]) sat
inside `GetStaffSpell`'s selection walk:

```cpp
if (sLevel != -1 && l >= sLevel && !oracool::IsBuiltInPortalAbility(...)) {
    rv--;
    bs = ...;
}
```

The walk counts `rv` down over spells passing that predicate, so excluding one shifts which spell
every subsequent roll lands on - and that mapping depends on `l` (= `lvl / 2`). Generation and
`RecreateItem` do not always arrive with the same `lvl`, so the two diverged.

Fixed by substituting on the *result* instead of filtering the roll:

```cpp
if (oracool::IsBuiltInPortalAbility(bs))
    bs = SpellID::Firebolt;
```

The walk is now bit-for-bit vanilla, so a seed always yields the same spell, and the remap depends
only on that spell. Town Portal staves still never spawn.

Worth generalising: a filter inside a seed-driven selection loop is only safe if every caller
reaches it with identical inputs. Substituting after the fact has no such requirement.

## Test baseline changes

Three, all deliberate:

- `writehero_test`'s golden hash - the save blob grew with `InventoryGridCells` (40 -> 70). This
  is the second re-baseline; the comment there now records both and warns against doing it
  casually.
- `MergeStackableItemIntoBelt` seeded belt slot 0, which is the Menu button since the HUD
  overhaul and correctly refuses items. Re-seeded to slot 1.
- `CheckInventorySortButtonClick` used `InventorySortButtonPosition`, now deleted along with
  `InventorySortButtonSize` - the SORT button has real art and a real rect.

## Verification

Debug build clean, `ORACOOL_VERSION` 1.0.86. Every test suite passes except `drlg_l1_test`, which
is **pre-existing and unrelated**: it fails on dungeon stairs placement, and `objects.cpp` carries
338 uncommitted lines from the waypoint work. Not investigated here.

Needs a play-test: the panel has never been on screen.

## Still open

- The six new equipment slots (shoulders, gloves, bracers, belt, legs, boots) draw their frames -
  they are baked into the panel art - but have no `inv_body_loc` behind them and accept nothing.
- Flyout panels other than the inventory still centre against the old HUD rect.

## Related

- [[2026-08-11 - Inventory Panel Geometry]]
