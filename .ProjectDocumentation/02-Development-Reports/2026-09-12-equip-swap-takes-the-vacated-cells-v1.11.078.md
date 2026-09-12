# The displaced item takes the hole the equipped one leaves

**Version:** 1.11.078
**Date:** 2026-09-12
**Branch:** renderer-32bit

## What was asked

> "when i right click an item from inventory into an item slot the current item being replaced to
> take the same spot in the inventory the new item just occupied, unless it is bigger, in which case
> look for suitable place or throw on ground."

## Why it could not already work

`CheckInvCut`'s auto-move branch - which a right-click reaches with `automaticMove=true` - did this,
in this order:

1. take the displaced item out of the body slot and `AutoPlaceItemInInventory` it,
2. equip the new item,
3. remove the new item from the bag.

The displaced item was therefore placed while the new one was **still in the grid**. The cells it was
about to vacate were occupied at the one moment that mattered, so they were never a candidate, and
`AutoPlaceItemInInventory`'s scan sent the displaced item wherever it first found room - usually the
far end of the bag. No amount of preferring the vacated slot helps while the order is that way
round; the reorder is the fix, not an optimisation of it.

## The new order

1. Capture the equipped item **and** its anchor cell and tab, before anything moves.
2. `RemoveActiveInvItem` - the bag cells are now free.
3. Place the displaced item: **the vacated anchor**, else anywhere, else the floor.
4. Clear the body slot and `AutoEquip` the captured item.

`PlaceItemInTabSlot` fails without writing when the item does not fit the hole, so *"unless it is
bigger"* needs no size test of its own - the fit check IS the test. "Suitable place" is the existing
`AutoPlaceItemInInventory` (which also reaches the extra tabs), and the floor is `TryDropItem` at the
player's feet.

Three things the reorder forced, each a real hazard rather than bookkeeping:

- **The captured copy.** `RemoveActiveInvItem` compacts the list, so `iv - 1` stops meaning the
  equipped item the instant it is called. The code used to re-read it from that index afterwards.
- **The double removal.** The removal at the bottom of the branch still ran, and on a compacted list
  it would have deleted whichever item was swapped into that index. `newItemTakenFromBag` guards it.
- **The failure path.** The old code could abort while everything was still in place. Now the item is
  out of the bag before the equip, so if `AutoEquip` ever failed it would be held nowhere - it is put
  back, and failing that dropped, rather than lost.

Nothing can be destroyed: the last resort everywhere is the cursor, via `TryDropItem`'s own refusal
("where would I put this"), because the body slot is cleared on the assumption the displaced item
found a home.

## The tabbed inventory

The anchor is captured with its tab, because `AutoPlaceItemInInventorySlot` writes `InvGrid`/`InvList`
- tab 0 only - while the click is read through the `ActiveInventoryTab` accessors. Placing a displaced
item "back where the other one was" while looking at page 3 would otherwise have written it to page 1.
`PlaceItemInTabSlot` routes to `AutoPlaceItemInExtraTabSlot` for any page but the first.

## The test earns its place

`ActiveInvAnchorSlotOf` is the one piece of arithmetic here that can be wrong by exactly one row and
fail **silently**: `AddItemToInvGrid` marks an item's BOTTOM-left cell with the positive list index,
while every placement helper takes a TOP-left anchor. Off by a row, the fit check just fails and the
displaced item goes to the far end of the bag - precisely the behaviour being fixed, with no error
anywhere.

So the test asserts the premise as well as the result: that the grid really does mark the bottom-left
cell, and that the anchor comes back as the cell the item was *placed* at. It covers a 2x2 item, the
degenerate 1x1 where both cells coincide, and an unreferenced index returning -1 rather than cell 0.

Writing it hit both documented backpack-test traps in turn, which is worth recording:

- `MyPlayer` **faults** - `AddItemToInvGrid` calls `NetSendCmdChInvItem` when handed MyPlayer, and
  that is an access violation under the harness (0xc0000005).
- `Players[1]` is **out of range** - the harness sizes that vector to one entry, so it asserts inside
  `std::vector` instead of yielding a spare player.

A stack `Player` is neither, but starts full of garbage, so the test zeroes `InvList`, `InvGrid` and
`SpdList` by hand - the belt too, because `AutoPlaceItemInInventorySlot` ends in `CalcScrolls`.

## Verification

Debug and Release build clean; **720/720** tests pass. RTM updated.

What the suite cannot reach is the swap end to end - driving `CheckInvCut` needs the cursor-to-slot
mapping and a real equipped body slot - so the in-game check is worth doing deliberately: right-click
a one-cell ring over an occupied ring finger and confirm the old ring lands in the cell the new one
left, then repeat with a two-handed weapon replacing a one-hander, where the displaced item is bigger
than the hole and should fall through to "suitable place".
