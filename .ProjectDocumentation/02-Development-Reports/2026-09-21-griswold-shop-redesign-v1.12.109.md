---
version: v1.12.109
date: 2026-09-21
area: UI / Griswold's shop
tests: 832/832
---

# Griswold's shop: his own forge, six painted services, and the button mechanic goes game-wide

## The asks

> I want to redesign Griswold Shop [...] First - We replace the canvas for all his tabs, including the new Salvage
> tab. We remove the title Griswold from all tabs. We remove the Salvage title from Salvage tab. For all the shops
> tabs we place the 6 button frames + Gold icon [...] placed according to their position in the Guide Canvas. Over
> the buttons you will place the icons from RfA 25 [...] Order of placing is - Repair, Repair All, Recharge, Sell,
> Sell All, Refresh. Sell an item is a new button.

> the six buttons to be sinkable and to operate on release of click within their area [...] we need to make this
> programming mechanic a default for buttons game-wide.

> the same sound should be played when clicking on all tabs. check if it is true.

## Placement came from the guide, not from the eye

The user supplied a plain 340x720 canvas and a **guide** canvas carrying the frames and the gold in position.
Diffing the two isolates the guide marks exactly, so every number below is measured:

| Mark | Box |
| --- | --- |
| six button frames | (24,128) (60,128) (96,128) | (210,128) (246,128) (282,128), each 34x34 |
| gold pile (ink) | (27,632), 27x18 |
| gold count | (25,657) to (58,669) |

Three frames stand either side of Griswold, who occupies the middle of his own painting - so the row reads as two
groups of three, and the user's order falls into that split as the three things done TO your gear and the three
done WITH the shelf.

The gold icon's ORIGIN is back-calculated rather than taken from the mark: the file is 28x28 and its ink starts
five rows down, so the file goes at (27,**627**). Placing it where the ink was measured would have dropped the
pile five pixels.

## The grid did not move, and that was worth checking

The canvas looked at first like it wanted a shorter grid - a forge scene on top, a flat dark area below. Overlaying
the CURRENT grid bounds on the guide settled it: the 16-row grid's top (173) lands eleven pixels under the button
row, its bottom (621) six above the gold pile, and its sides (30 and 310) match the dark area's edges. **The canvas
was designed around the existing geometry.** No rows move, and the standing rule - "you dont remove one grid row
from shops" - is untouched.

## The six buttons

Fixed frames at the measured positions, wearing RfA-25's **24x24 glyphs** (batch 48) centred with a 5px margin -
the size asked for in the brief precisely because a 34px frame is what they had to sit in.

All six are drawn on **every** one of his tabs. One the tab cannot do is desaturated in place rather than left
out: the frames are painted fixtures now, and a row with a hole in it reads as a bug rather than as a rule. The
grey is the same white-hue `TintRectRgb` pass the inactive Act buttons wear.

| Slot | Does | Gated? |
| --- | --- | --- |
| Repair | arms the hammer, then click an item | no - his service, live on every tab |
| Repair All | `ShopRepairAll()` | no |
| Recharge | arms the recharge cursor | no |
| **Sell** | **arms the hammer to sell ONE item** | no |
| Sell All | the tab's bulk-sale row | yes - `ShopTabHasSellAll` |
| Refresh | the tab's stock-refresh row | yes - `ShopTabHasRefresh` |

The two gated answers are derived from `GetShopActions`' own rows (`ShopTabHasSellAll` / `ShopTabHasRefresh` in
stores.cpp), so the gating can only be changed in the one place and cannot drift from the list it reads. Refresh
is absent from Unique and Set by a rule worth keeping: both shelves are drawn without replacement, so a refresh
would reshuffle the same contents and read as broken.

## Sell an Item

A new `ShopServiceCursor::Sell` beside Repair and Recharge. It wears the **hammer** - `CURSOR_REPAIR`, as asked -
so the graphic, the click-an-item targeting and `TryIconCurs`' inventory/tab routing all come for free; the enum
is the only thing separating a sell click from a repair click, exactly as it already separates a paid repair from
the Repair skill. It is asked BEFORE the paid repair in `TryIconCurs`, and always spends the click, so a refused
sale can never fall through to a repair the player did not ask for.

`ShopSellItemAt` uses the vendor's own `SmithSellOk`/`WitchSellOk`, records the sale unchanged for buyback, checks
gold room, removes the item and plays `IS_GOLD` - the gold drop the user asked for.

## The button mechanic is now the default

Written into memory as a standing rule rather than a per-button choice. The six new buttons have it, and the
**backpack tabs** - the last strip in the game still acting on the press - were given it too: sink 2px down-left
while held, page turns on the mouse-up and only inside the tab, hit test on the unsunk rect.

One caveat recorded with the rule: this engine has **no right mouse-up**, so a right-click cannot use the mechanic
and must act on the press.

## The tab sound audit - it was NOT true

| Tab strip | Before | After |
| --- | --- | --- |
| Vendor tab column | `titlemov` on press, every press | unchanged |
| Workshop tabs (Gillian, Ogden) | `titlemov` on press, every press | unchanged |
| Backpack tabs | `titlemov` **only when it switched** | `titlemov` on every press |

The backpack tabs were silent when clicking the tab already open, on the grounds that nothing had changed. A
button that answers only sometimes reads as a button that missed the click.

## Test

`InvTest.EveryTabPositionOpensAStoragePage` failed, correctly - it asserted the page turned on the press. Rewritten
to press, assert nothing happened yet, then release; and extended with the case the old test never covered: a
release dragged OFF the tab turns no page.

## What was removed

* The "GRISWOLD" title band, on all his tabs. The painting is a portrait of the man.
* The "Salvage" title on the Salvage page. Its layout now carries an empty title rect, so the draw skips it.
* His Salvage-only painting (`ui\salvage_canvas_tall.png`) is no longer read - one canvas behind every door of his
  shop, so moving between the shelves and Salvage no longer changes the room. The file stays in the archive.

## Build

Debug, clean. 832/832. Nine new assets packed by the normal build.

## Not verified

Nothing has been seen on screen. Worth a look on the next visit to the forge:

1. **The glyphs at 24px on the painted frames** - they were checked on a limestone plate, not on this canvas.
2. **The greyed buttons** - Sell All and Refresh are absent from some shelves, so most tabs will show grey.
3. **"Refresh until"** has no frame. Six were specified and six were built; that option-gated seventh action is
   now unreachable on Griswold's page. It needs a decision: a seventh frame, or drop it.
