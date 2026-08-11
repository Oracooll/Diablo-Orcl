---
title: 2026-08-11 - Inventory Panel Geometry
date: 2026-08-11
tags: [dev-report]
summary: The geometry foundation for the 320x660 top-right inventory window. Establishes why the panel owns its own rect, why the cell stays 28px, and why the item grid stops at y 596.
---

# Inventory Panel Geometry

First slice of the inventory revamp: `Source/oracool/inventory_layout.h/.cpp`, the single
source of truth for the new window. No behaviour yet - nothing calls it. Landed alone so the
three decisions below are settled and compiler-enforced before any of the invasive rewiring
starts.

## The panel owns its own rect

`SidePanelSize {320, 352}` is shared by five panels - inventory and spellbook resolve through
`RightPanel`, character sheet, quest log and stash through `LeftPanel`. Stretching it to 660
would drag the other four along. The inventory therefore gets `GetInventoryPanelRect()`,
independent of `CalculatePanelAreas()`, and the other four keep their old vertically-centred
flyout placement untouched.

Placement is flush to the top-right corner, with no inset. At 660 tall on a 720 screen there
is nothing to spare, and any top margin pushes the bottom further under the mana orb.

## The cell stays 28px

`INV_SLOT_SIZE_PX` is 28 because **item icons are fixed-size sprites cut at 28px per cell in
the original art**. A 1x1 potion is a 28x28 sprite, a 2x3 sword is 56x84. Moving the grid to
any other pitch would leave every item icon in the game either needing a rescale or sitting
with a visible gap inside its cell.

The frame components were first read as implying a 31px cell (33px outer on the 1x1). They do
not - that is a 28px interior inside a ~3px border, and the same holds throughout: 2x2 is
62x62 around 56x56, 2x3 is 62x90 around 56x84. **The art was cut for a 28px grid already**, so
it needs no rescaling and lines up with the item sprites for free. Ten columns sit at 280px
inside the 320 panel with a clean 20px margin either side.

## Ten tabs, not eight

The concept composite showed eight tabs. `tab-and-button-states-reference.png` supplies ten
(I-X, three states, two sizes) - and the codebase **already runs ten**: `ActiveInventoryTab`
is 0-9, one main backpack plus nine extra, with `InvTabGrid`, `AutoPlaceItemInExtraTabs` and
the cross-tab sort all built and tested. Ten is therefore both what the art wants and what
avoids touching the tab persistence layer. Ten tabs at 24px plus a 46px SORT button fit
across the panel width on one line.

## Why the grid stops at 596

The user asked to nudge the panel up to reduce how much of the bottom row the mana orb eats.
The panel cannot move - it is already flush at y 0, and 660 + the orb's 96 exceeds the 720
screen, so the overlap is pure geometry.

The fix came from the user's own follow-up: move *the grid* up rather than the panel. Since
the panel is being assembled from components, the grid does not have to sit at the bottom of
the background art. Vertical budget:

| Block | y range |
|---|---|
| Equipment, helm to boots | 18 - 340 |
| Tab row + SORT | 368 - 396 |
| Item grid, 7 rows of 28 | 400 - 596 |
| Decorative footer | 596 - 660 |

The orb's top edge is screen y 624. The grid ends at 596, so the orb covers **only the
footer** - no live cell is obscured at all. Better than minimising the overlap; it removes it.

This is load-bearing and easy to break later, so it is a `static_assert` against
`OrbClearanceBottom` rather than a comment. Four more assert the grid fits the panel, the tab
row clears the grid, the tab strip fits the width, and the boots slot clears the tab row.

## Equipment slots

Thirteen, in one `EquipSlots` table: three columns (left 34, centre 132, right 230), sizes in
grid cells so they match the item sprites that land in them. Seven map onto existing
`inv_body_loc` values; six (shoulders, gloves, bracers, belt, legs, boots) have no item types
behind them yet and will draw their frame while rejecting drops until those exist.

## Verification

Debug build clean, `ORACOOL_VERSION` 1.0.78. Static asserts are the only meaningful check at
this stage - there is nothing to play-test until the panel is wired up.

## Next

Compose the panel art from the components; rewire `InvRect[]`, `InventoryGridCells` (40 -> 70)
and `InventorySizeInSlots` ({10,4} -> {10,7}) onto this geometry. That step is save-breaking
via `PlayerPack`'s fixed-layout array - the user has accepted a character reset for it.

## Related

- [[2026-08-11 - Burger Menu Icon Row]]
