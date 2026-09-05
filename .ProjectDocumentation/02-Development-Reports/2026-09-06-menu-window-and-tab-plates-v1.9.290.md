# The burger menu as a window; the inventory tabs on plates (v1.9.290)

**Date:** 2026-09-06
**Request:** "i want burger menu to popup a window same as skill pickers. width equal belt width. bottom flush with top border of lmb/rmb slots. all icons in it to utilize 37x38px backing and chatgpt made vanilla-style icons which you will make a prompt for and place when ready. also - tab icons above inv grid - to utilize 28x28px vanilla backing - grey for inactive, gold for active. gold to be again bigger to stand out from inactive. tab icon to be chatgpt generated."

## The menu window (hud_menu.cpp)

The bare icon row above the XP counter is gone (its geometry is in the history at v1.9.289). In its place a window in the skill picker's dress: two half-transparent passes, the ornate border, the red X, a "Menu" title band, and eight cells of the vanilla 37x38 plate at native size, four across in two rows. Its width is the belt's - first cell's left edge to last cell's right edge - and its bottom edge IS the HUD plate's top edge. `GetHudMenuWindowRect` / `GetHudMenuCellRect` expose the geometry.

A cell's plate is GOLD (Ready) when the entry's window is showing and light grey (Unspent) otherwise; hover is the Abilities window's deeper shadow under the plate; the orange click flash stays. Until the glyph pack lands the entry's initial stands in, white, FontSize24, with the text shadow. Clicks: a cell acts, the X or any miss closes.

## The inventory tabs (inv.cpp)

Each tab is the vanilla plate shrunk to its 28x28 cell through the new `DrawPlateIn(out, cell, tint)`: light grey when closed, gold and grown to 32x31 (two out each side, three up, bottom pinned - `GetActiveTabRect`) when open, hover as the deeper shadow. The reliquary-chest atlas is no longer drawn and stays filed. The numeral stands in, white with the text shadow, until the tab glyphs arrive.

## The brief

`.ProjectDocumentation/06-Reference/ChatGPT Brief - Belt Menu and Inventory Tab Glyphs.md` - eight 37x38 menu glyphs and ten 28x28 numeral glyphs in the skill-glyph two-colour format, with the wiring plan for when the zip arrives.

## Test

`OracoolAudit.TheMenuWindowSpansTheBeltAndSitsOnThePlate` - left and right edges equal the belt's, bottom equals the plate's top, eight 37x38 cells inside the window and not overlapping.

Suite 633/634, the standing dungeon-generation failure only.
