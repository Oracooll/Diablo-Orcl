# Fifth /dev batch: backpack pages by level, passive slots at 10-40, stash nav ticks — v1.12.176

2026-09-25

> check dev notes and process

Three notes, coded in full and built once.

## Inventory Tab Level Gates

New `[Oracool]` option `Inventory Tab Level Gates` (`OptionEntryBoolean inventoryTabLevelGates`, default
on, written to the INI with its comment). `InventoryTabRequiredLevel(page)` = `page * 10` for pages 1–9
(displayed 2–10), 0 for the backpack or with the option off; `IsInventoryTabLocked(player, page)`.

- **Read as full:** `AutoPlaceItemInExtraTabSlot` refuses a locked page. Every automatic placement
  onto a page goes through it — `AutoPlaceItemInExtraTabs` (pickups, purchases, stash transfers) and
  both of SORT's placement passes — so nothing is placed there by any path.
- **SORT** does not gather from a locked page (it could not put anything back).
- **Button:** a locked page draws its plate with the level in red (shadowed) instead of the chest
  glyph; `CheckInventoryTabClick` takes the click without pressing and logs "Backpack page N opens at
  level L."; `DrawInventoryTabs` drops back to page 1 if the open page is locked.
- **Why not placeholder items** (the note suggested them, "of your choice"): an invisible item is still
  an item to the save, the charm cap, the salvage totals and every loop that walks the pages. A
  refusal at the placement choke point is the same "full" with nothing to leak or clean up.

Tests: `InvTest` turns the gates off in its fixture (a fresh `Player` is level 0);
`TabbedInventory_LevelGatesLockPagesAndReadThemAsFull` checks the ladder, the refusal at level 9, the
opening at 10, and the option off.

## Passive slots at 10, 20, 30, 40

`PassiveSlotRequiredLevel`'s table; the gate test renamed and rewritten (`...AtTenTwentyThirtyForty`).
Every other test that fills slots runs its hero at level 30 or more (checked before the build).

## Stash page buttons tick

`IS_TITLEMOV` (the vendors' tab sound) at the press in `CheckStashButtonPress` and once per hover
entry, tracked in `DrawStash`.

Debug build and tests at the end of the batch.
