# An eighth plate: salvage one item, chosen with the hammer (v1.12.101)

**Date:** 2026-09-21 · **Version:** v1.12.101 · **Tests:** 832/832

User: "add one more icon next to Salvage Ethereal items - Salvage an Item - Lets the user select an individual item in
inventory backpack to salvage. When a user clicks on this icon change cursor to Repair Hammer vanilla cursor, then
when user selects an item it is destroyed, salvaged and cursor returns to normal one ... Now you need to arrange a 4x2
grid of icons keeping the distance between them as it is now on row 1."

## The grid

Eight plates at row one's own pitch, 66 px, both rows starting at x 43: White, Magic, Rare, Unique at y 344; Set,
Primal, Ethereal and the new hammer at y 410. The 320x352 page has no room for an eighth and shows the seven.

The icon is `Icon 8 Salvage Individual Item.png` (1254x1254 on a green screen): the plate's bounding box cut off the
green, resampled to 56x56, and the green fringe cleared to transparent - `ui\salvage_item.png`.

## The hammer

A click on the plate arms `SalvageItemCursorArmed` and sets vanilla's `CURSOR_REPAIR`, and the plate stays lit while
it is armed. The next click is read in `TryIconCurs` (diablo.cpp) BEFORE the shop's hammer and before the vanilla
Repair skill - a hammer armed here always spends the click, so it can never fall through to the skill that shortens
an item's maximum durability. On a backpack item (main or tab) `UseSalvageItemCursor` breaks it down; on anything
else the hammer is simply taken back. Either way the cursor returns to the hand.

`SalvageSingleItem` (salvage.cpp) is the one-item twin of `SalvageAllInBackpack`: the item goes first, which frees
the cell the materials land in, and the overflow line is the same. The window's message then reads "1 Rare Items
destroyed / 5 Rare Fibres Salvaged", the same two lines every bulk press writes.

The hammer is taken back when the window closes, when a bulk plate is pressed, and on a new game, so it cannot
outlive the page that armed it.
