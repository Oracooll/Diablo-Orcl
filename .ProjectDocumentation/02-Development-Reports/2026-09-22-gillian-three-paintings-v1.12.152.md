# Gillian's three paintings, and the icon row over her frame — v1.12.152

2026-09-22

## What was asked

> use gillian cube canvas also for first two tabs. rearrange buttons on top of smaller frame. on all her tabs which use the small frame canvas.

and then, as the files were renamed mid-request:

> use "Gillian Single Item Frame.png" for tabs 1 and 2 - where we need to place only one item.
> use "Gillian Multy-Item Frame.png" for tab 3 where we need multiple items in one grid.
> in tabs 1 2 3 place the icon buttons over the smaller frame.

## What landed

Her window had one painting on the Craft tab, one on Recipes, and the old code-drawn
interior on Reroll and Imbue. It now has a painting on every page.

| Tab | Painting | What stands in the small frame |
|---|---|---|
| Reroll | `gillian_frame_canvas.png` | one item, any footprint |
| Imbue | `gillian_frame_canvas.png` | one item, any footprint |
| Craft | `gillian_craft_canvas.png` (new) | a painted 3x4 well |
| Recipes | `gillian_recipes_canvas.png` | — |

`gillian_cube_canvas.png` was renamed `gillian_frame_canvas.png`: it is the empty-frame
file, and it now serves the two one-item tabs rather than the craft tab it was named for.

### Measured before anything was written

The three files are the same room and differ only inside the small frame.

- The small frame's borders run x 207..216 and 304..313, y 145..154 and 270..280, so its
  opening is **x 217..303, y 155..269** — 87 by 115.
- On the multi-item file, bright rules stand at **x 245 and 274** and **y 183, 212 and 241**.
  That is a 3x4 of 28px cells on a 29px pitch starting at (217,155) — the Roar's own cell,
  the same grid Ogden's canvas paints low on his page. So her craft grid is his code with a
  different origin (`CraftGridOriginFor`), not a second grid.
- The big frame opens at x 29..310, y 299..618 on all three, which is Ogden's recipe frame
  to the pixel.

### The icon row

Asked for three times, and it moved for a reason: the row sat at y 224, which was open
stone while the window drew its own walls and is the small frame and the woman's dress on
the painting.

The whole block — plate, the two pixels under it, the price line — now clears the frame's
painted band by four pixels, so the row's top is `145 - 4 - (34 + 2 + 14) = 91`.
Horizontally it is centred on the FRAME, then held inside the frame's own right edge at
x 313: a row of three plates is 158px against an 87px frame, so a row centred on it would
hang four pixels off the moulding. Held, it ends exactly where the frame ends and reaches
further left instead.

Reroll's single plate, Imbue's three and Craft's Transmute all sit on that one line, so the
button does not jump as the player moves between her pages.

The price line under each plate is now clamped to the canvas opening. It was drawn a gap
wider than the plate on purpose — eight digits need the air — but the plate on the right
end of the row now has painted moulding beside it rather than air.

### What moved into the big frame

Her list was at (104,84) and her message board at (30,336), laid out for a window that drew
its own floor. Both now sit inside the painted opening, on one dark layer: the list at
x 33..306, y 305..424, the board at y 433..612. Her Craft tab draws the board too — its big
frame would otherwise stand empty under the well, and "Nothing on the bench makes anything"
has to be readable somewhere.

### Her craft bench became a grid

The one-item bench and the pack-reagent loan were built on 2026-09-22 *because* the frame
held one item. The painting now puts twelve cells in that frame, so:

- `FindMysticRecipe` asks the **grid alone first**, exactly as Ogden's and the Cube's pages
  do. A player who lays out every input gets the plain contract every other crafting station
  in the mod has.
- The **pack still lends**, as a second question asked only when the grid cannot answer the
  first. That instruction stands ("yes, pull reagents from the pack") and it costs a player
  who does place the reagents nothing.
- The loan is never put in scratch slot 0. That is where `TransmuteLevskiGridWith` leaves the
  result, and the caller has to skip the loan's cell when it writes the scratch grid back —
  a loan in the result's cell makes those two the same cell, and the result would be dropped.
- Settlement order is unchanged and is the point: the scratch grid is a copy, the transmute
  runs on the copy, and only after it succeeds is the copy written back over the real well
  and the pack charged. A refusal cannot cost the player anything.

### Two item-safety gaps closed on the way

- **The bench does not travel.** Only Reroll and Imbue draw it, so an item left on it while
  the player moved to Craft or Recipes was invisible until the window closed — and on the
  new Craft tab it would have been invisible *under* the well sharing its frame. It goes
  back to the pack on the way out of those tabs, and when there is nowhere to put it the tab
  does not change. That is the answer `CloseWorkshop` gives to the same question.
- **The bench is no longer hit-tested on Craft.** Its rect is the well's rect now, and
  `CraftSlotAt` owns it. A control that is not drawn must not be clickable — the rule an
  invisible bench under Ogden's collection tabs taught this file on 2026-09-22.

### Smaller things

- `DrawBench` stopped drawing a plate fill, a gold outline and a 2x3 of cell lines. Those
  were the bench, back when the window drew its own furniture; over a painted frame they are
  a second frame inside the first, and the cell lines describe a grid the painting has not
  got. The item is fitted to the frame, as the craft cells are fitted to theirs.
- The craft cells got hover text, both hosts. They had none, which was survivable while the
  only grid was Ogden's low-painted well; hers stands where the player has every reason to
  expect a name.
- `RecipeOpeningRect`'s Gillian fallback is gone. There is one `FrameOpeningRect` now, used
  by every page of both windows.
- `SlotCells`, `SlotRect`, `CellPx`, `GoldRect`, `ButtonRowTop`, `MysticTransmuteRect` and
  `MysticCraftPlateSize` are all deleted — dead with the layout they belonged to.

## Files

- `Source/oracool/workshop.cpp`
- `Packaging/resources/oracool_assets/ui/gillian_frame_canvas.png` (renamed from
  `gillian_cube_canvas.png`)
- `Packaging/resources/oracool_assets/ui/gillian_craft_canvas.png` (new)

Asset changes, so `oracool.mpq` was repacked.

## What to look at in play

1. Her four tabs in order. The icon row should be on the same line on the first three, ending
   flush with the small frame's right edge, with nothing over the moulding.
2. Drop a sword on the Reroll frame — it should fit the frame, not spill.
3. Put something on the bench and switch to Craft. It should go back to the pack, silently,
   rather than vanish under the well.
4. The craft well: twelve cells, items fitted, names on hover.
5. One real craft both ways — reagents in the well, and reagents in the pack — checking the
   pack's count before and after. This is still the only path in the window that spends pack
   items, and it fails quietly in either direction.
