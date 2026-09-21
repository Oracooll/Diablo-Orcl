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
 * ## The controls wear the vanilla button
 *
 * The tab column, the service and bulk-action buttons and the gold line are Diablo's own small dialog
 * button (ui_art\but_sml, read from the player's archive at runtime and never shipped), desaturated to
 * the limestone's grey, with every label in gold on top (user, 2026-09-11). The tabs are the same
 * button laid on its side, their labels reading down them. The panel itself is not the shop's: it is
 * the shared 340x720 side-panel background every limestone window wears, and the grid keeps its bezel.
 *
 * Without that file the controls fall back to three blank limestone plates (ui\shop_tab.png,
 * shop_button.png, shop_gold_plate.png - batch 6), and without those to the flat fill and the ornate
 * border, so a build short of art still draws a whole shop.
 */
#pragma once

#include <vector>

#include "engine/point.hpp"
#include "engine/rectangle.hpp"
#include "engine/surface.hpp"
#include "stores.h"

namespace devilution::oracool {

/**
 * @brief Whether the vanilla dialog button (ui_art\but_sml, from the player's own archive) loaded - what
 * the controls wear when it did. False means the limestone plates are standing in; the log says why.
 */
bool ShopVanillaButtonArtLoaded();

/**
 * @brief Draws the tab column @p open's vendor shows, beside the shop panel's rect.
 *
 * Exported for Griswold's Salvage page, which is not a shop screen but sits in the shop panel's place and keeps his
 * tabs in view beside it (user, 2026-09-21). Drawn from the shop's own column, so the two cannot drift apart.
 */
void DrawShopTabColumnFor(const Surface &out, TalkID open);

/** @brief The tab under @p position in that column, or TalkID::None. The caller decides what switching costs. */
TalkID ShopTabAt(Point position, TalkID open);
/** @brief Whether @p id is drawn as the icon grid rather than as the vanilla text list. */
bool IsShopGridScreen(TalkID id);

/** @brief The shop panel's screen rect. Valid whether or not a shop is open. */
Rectangle GetShopPanelRect();

/**
 * @brief Where the shop's close button sits.
 *
 * Exported for the audit test that pins every window's X to the shared corner. The shop is the one
 * window that computed its own - at right-34 / top+14, 20x20, against the helper's right-21 / top+3,
 * 18x18 - so it is also the one where "the button is where the helper says" needs asserting against
 * the window's OWN answer rather than against the helper twice.
 */
Rectangle GetShopCloseButtonRect();

/**
 * @brief Whether @p position is on the shop at all - the panel OR its tab column.
 *
 * What every click and hover router should ask, rather than GetShopPanelRect().contains(): the tabs
 * moved out of the panel and into a column beside it (user, 2026-08-27), so they float over the play
 * area and a router testing the panel alone would let clicks fall straight through them.
 *
 * A predicate rather than a rect on purpose - see the note at the definition.
 */
bool IsPointOverShop(Point position);

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
 * @brief Discards stock on tab @p id that one page cannot show, so the shelf is FIXED.
 *
 * Called once when a vendor's stock is generated. Without it `PlaceStock` is only a view over an
 * oversized array: buying a visible item frees cells, the next draw re-runs placement, and an item
 * that was previously skipped appears with no refresh (external audit of v1.9.92, finding 5). The
 * shop then advertises one page while holding a hidden reserve the player can mine by buying.
 *
 * The user's instruction was "keep available items up to 1 page worth of quantities", and this is
 * what makes that true of the STOCK rather than only of the drawing.
 *
 * Entries marked ShopSlot::neverTrim are placed and shown but never cleared - Pepin's four infinite
 * potions on the Supplies tab, which are not the shop's to discard. Everything else on a mixed
 * shelf is trimmed normally, which is what lets Supplies be materialised rather than exempted.
 */
void TrimShopStockToOnePage(TalkID id);

/**
 * @brief Whether every entry of @p id's stock currently fits on one page.
 *
 * The question TrimShopStockToOnePage answers destructively, asked without clearing anything - so a
 * caller that has just added one item can put it back rather than let the trim decide which of the
 * shelf's other items pays for it.
 */
bool ShopStockFitsOnePage(TalkID id);

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
/** @brief The shop item under the cursor, or nullptr - for the comparison panel (user, 2026-09-05). */
const Item *HoveredShopItem();

/** @brief One run of a sliced button: @p length pixels from @p source in the art to @p dest in the control. */
struct ButtonSliceSpan {
	int source;
	int dest;
	int length;
};

/**
 * @brief How a button face @p source pixels long covers a control @p target long, along one axis.
 *
 * At 1:1, never scaled. Longer than the face: both @p cap-pixel ends whole and the middle repeated
 * between them. Shorter: the face's first half and its last half, butted, so both ends survive and the
 * middle that could not fit is what goes. The spans cover the target exactly, in order.
 */
std::vector<ButtonSliceSpan> SliceButtonAxis(int target, int source, int cap);

} // namespace devilution::oracool
