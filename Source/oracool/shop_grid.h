/**
 * @file oracool/shop_grid.h
 *
 * Oracool: the D2-style shop - one panel, one grid of item icons, tabs across the top.
 *
 * ## What this replaces
 *
 * The vanilla store is a 592x292 text box: four lines per item, four items visible, a slider down
 * the right. Griswold's basic stock alone is twenty-five items, so buying anything meant scrolling
 * a list that showed six percent of itself at a time.
 *
 * This draws the same stock as an icon grid instead, at the STASH's dimensions - ten columns by
 * sixteen rows of 28px cells (user request, 2026-08-21: "make items display grid the size of STASH
 * grid. it can fit comfortably"). Items occupy their real inventory footprint, so a two-handed
 * sword reads as a two-handed sword before you hover it.
 *
 * ## Why the panel moved
 *
 * 10x16 at 28px is 280x448. That does not fit inside the store box, so the shop becomes a full
 * 340x720 panel in the top-left slot - the same rect the stash, character sheet and quest log
 * already share. Nothing else is open while a shop is, so there is no collision, and the grid lands
 * at the same pitch and the same screen column as the stash's, which is what makes the two read as
 * the same kind of surface.
 *
 * ## What it does NOT do
 *
 * It does not own the transaction. A click resolves to a stock index and hands that to
 * ShopSelectIndex (stores.h), which runs the tab's existing Enter handler - afford check, room
 * check, stale-row guard and all. The confirm, no-money and no-room screens are still the vanilla
 * text box. That boundary is deliberate: those checks have each been a reported bug, and a grid
 * that re-implemented them would be a second place for them to go wrong.
 *
 * ## Placeholder, and honestly so
 *
 * Flat themed fill, ornate border, text labels - the same treatment every Oracool window wears
 * while it waits for art. The visuals are explicitly not final (user, 2026-08-21: "we will workout
 * the final visuals later").
 */
#pragma once

#include "engine/point.hpp"
#include "engine/rectangle.hpp"
#include "engine/surface.hpp"
#include "stores.h"

namespace devilution::oracool {

/** @brief Whether @p id is drawn as the icon grid rather than as the vanilla text list. */
bool IsShopGridScreen(TalkID id);

/** @brief The shop panel's screen rect. Valid whether or not a shop is open. */
Rectangle GetShopPanelRect();

/** @brief The grid's screen rect, inside the panel. */
Rectangle GetShopGridRect();

/** @brief Draws the whole shop - panel, tabs, grid, footer. Does nothing off a grid screen. */
void DrawShopGrid(const Surface &out);

/**
 * @brief Hit-tests the grid. True if the click landed on the panel and was consumed.
 *
 * @p rightClick separates looking from buying (user, 2026-08-26: "purchase should be done by
 * right-clicking, not by left clicking and confirming"). A left click on an item only moves the
 * selection, so the player can read a price and a stat line without committing to anything; a right
 * click is the purchase, and there is no confirmation after it.
 *
 * Every other control on the panel - the tabs, the page arrows, the service buttons, the close X -
 * answers to both buttons, because for those the click IS the whole intent.
 */
bool CheckShopGridClick(Point position, bool rightClick = false);

/** @brief Moves the keyboard cursor by @p rows and @p columns, wrapping through the stock order. */
void MoveShopGridSelection(int columns, int rows);

/** @brief Runs the selected item's transaction, as clicking it would. */
void ActivateShopGridSelection();

/** @brief Puts the cursor back on the first item. Called whenever a shop screen opens. */
void ResetShopGridSelection();

/**
 * @brief Fills InfoString from the shop item under the cursor. True if it did.
 *
 * The stat block is a popup that follows the cursor (DrawCursorTooltip), not a readout at the far
 * end of the panel - the same box the inventory, stash and belt already use on hover, so an item
 * reads identically wherever the player meets it.
 *
 * Called from UpdateInfoString rather than from the draw, because UpdateInfoString runs AFTER the
 * store draws and clears whatever it finds. It is the one function that decides what the tooltip
 * says, which makes it the only place this can be answered.
 */
bool SetShopHoverInfoString();

/**
 * @brief Whether the last hover pass landed on a shop item.
 *
 * The tooltip asks this to decide between its two treatments: a padded, bordered plate for an item's
 * stat block, and bare outlined text for a one-line label. Without it the shop's block drew as
 * outlined text over the grid, which is what the first playtest of the panel showed.
 */
bool IsShopItemHovered();

} // namespace devilution::oracool
