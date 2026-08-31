# Shop grid: what the first screenshot showed (v1.9.27)

**Date:** 2026-08-23
**Version:** 1.9.27
**Tests:** 511/513 — the two standing baseline failures.
**Status:** built, not played.

The v1.9.26 panel was shipped unseen. The user played it and sent a screenshot. Two of the four
problems in it were bugs I could not have caught without the picture, which is the whole argument
for [[feedback_screenshot_is_the_only_verification]].

## 1. The stock climbed out of the grid

Every multi-cell item drew above where it belonged — a three-cell sword anchored on its top row
appeared 56px high, over the tab strip.

`ClxDraw` renders **upward** from the point it is given. I anchored sprites at the item's top-left
cell plus 27px, which is the bottom of its *first row*, not of the item. The inventory anchors the
same way and is correct, which is what made the copy look right: `AddItemToInvGrid` marks an item's
**bottom-left** cell as its first slot, so `slot + cellHeight` there already *is* the item's bottom.
This grid packs from the top-left, so the item's own height has to be added back.

Fixed in `SpriteAnchor`, which is now a named function with that explanation on it, rather than a
`constexpr Displacement` shared by two loops.

Worth noting: `qol/stash.cpp` uses `INV_SLOT_SIZE_PX - 1` where `inv.cpp` uses
`InventorySlotSizeInPixels.height`. One of those two is off by one pixel. Not touched here — it is a
separate question from this bug, and the stash renders correctly today.

## 2. The tabs did not fit

Four tabs across a 340px panel is 77px each, and the screenshot showed **"SUPPLIE"** — the label
overran the tab. "Recharge" is the same length and would have done the same thing on the row the
items were covering.

Three per row now: 102px, wide enough for any label the tab sets contain. Griswold's seven tabs make
three rows of 3/3/1, and the short last row keeps the full tab width and centres, as before.

## 3. The grid sits where the stash's does (user request)

> bring the grid down as low as it is in the stash. all ui buttons to be on top of it. no need for
> gap between the grid and the bottom bezel.

`ShopGridTop = GridBottom - ShopGridRows * ShopCellPx` — the same expression `qol/stash.cpp` uses,
from the same constant. The three grids now end on one line, and the bezel's last pixel lands on the
HUD's content edge, so there is no gap under the grid.

Everything else moved above it: tabs, then the bulk-action row, then the gold readout, then the
grid. The footer box is gone entirely.

That budget is now fixed at both ends, and it is tight — the two spacers are 2px. A `static_assert`
holds the line and it **fired on the first build**, because `GridFrameWidth` is the carved stone
bezel's 6, not the procedural bevel's 3. Reserving the smaller number is exactly what the inventory's
own asserts were written to catch, and it caught me too.

## 4. Item stats are a popup (user request)

> i want item stats to be a pop up, not what you use now.

The hovered item's name and price were a two-line readout at the bottom of the panel. They are the
cursor-following tooltip now — `oracool::DrawCursorTooltip`, the same box the inventory, stash and
belt already use, showing the same `GetItemStr` + `PrintItemDetails` block, plus one price line that
says what the number is for (on Repair and Recharge the player is not buying the item).

It hooks in at `UpdateInfoString` rather than at the draw, and that placement is the point:
`UpdateInfoString` runs *after* `DrawSText` and clears whatever it finds, so anything set during the
shop's draw was wiped before the tooltip rendered. It is also the one function that decides what the
tooltip says, which makes it the only correct place to answer.

**Known gap:** the popup is mouse-driven. The keyboard cursor moves the selection outline but shows
no stat block, because the tooltip is anchored to the pointer. Not addressed here.

## Still unverified

Everything above. Specifically: whether three tab rows plus two control rows actually clear the grid
by eye rather than by assert, whether the popup's stat block fits on screen when hovering a bottom
row, and whether the bulk-action buttons are legible at 102px.
