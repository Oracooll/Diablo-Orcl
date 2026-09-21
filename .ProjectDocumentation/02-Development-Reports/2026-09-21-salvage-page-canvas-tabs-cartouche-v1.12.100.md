# The Salvage page on the user's own canvas, with his tabs beside it and a titled frame (v1.12.100)

**Date:** 2026-09-21 · **Version:** v1.12.100 · **Tests:** 832/832

Three asks in one pass (user, 2026-09-21).

## 1. The canvas

"use this Canvas image ... Griswold Salvage UI 340x720.png": already 340x720, copied as `ui\salvage_canvas_tall.png`
in place of v1.12.099's resampled full-size painting. Measured: the canvas's opening is the shared one (x 22..317,
y 25..694) and the lit forge gives way to bare floor at about y 385, so the icon rows keep their places - White,
Magic, Rare, Unique at y 344 across the floor line, Set, Primal, Ethereal at y 410.

## 2. The tabs stay in view

"make sure when a user clicks on Salvage tab the tabs column remains visible." The Salvage page is not a shop screen
(`stextflag` is None while it is open), so the shop's own column was not drawn. `shop_grid` now exports
`DrawShopTabColumnFor(out, open)` and `ShopTabAt(position, open)` - the internal column parameterised by which tab is
open rather than reading `stextflag` - and the docked Salvage page draws Griswold's column with Salvage lit, and
routes clicks on it: another tab closes the page and opens that shelf (`CloseLevskiRoar` then `StartStore`), Salvage
itself is absorbed. Tested before the window's own rect, since the column sits outside it.

## 3. The frame and its title

"make the Salvage Results frame border 1px thick and use same color you use for items sprites frame" and "put Salvage
results title somewhere along the top border ... splits into two lines with rounded edges to outline the Salvage
results text and merges again back into one border line."

`DrawSalvageResultsFrame` draws one pixel of `PAL16_YELLOW + 10` - inv.cpp's `GridFrameGold`, the same gold every item
sprite is outlined with since v1.12.094. On the top border the line runs in from each side, curves apart through a
quarter ellipse (`PlotQuarterArc`, reach 7, bow 9) into a line above the title and a line below it, and curves back
together. "Salvage Results" sits between them in the game's gold. The four rings of v1.12.099's dark gold frame are
gone with `DrawDarkGoldFrame`.

The results box moved to (30,486) 280x126 so the upper line of the cartouche clears the icons by ten pixels; its
bottom edge is y 612, still above the inventory grid's floor at 618, so the orbs stay clear.
