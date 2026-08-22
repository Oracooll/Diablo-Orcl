/**
 * @file oracool/shop_tabs.h
 *
 * Oracool: one shop per vendor, divided into tabs.
 *
 * ## The problem
 *
 * Griswold's dialog had grown to nine entries - talk, buy basic, buy premium, buy unique, buy
 * consumables, sell, repair, recharge, leave - and every service added another. A menu that long
 * stops being a menu and becomes a list you read every time.
 *
 * D2 and D3 both answer this the same way: the vendor has ONE door, and everything is behind it,
 * split into tabs. That is what this is.
 *
 * ## Why a tab strip and not a grid shop
 *
 * The obvious reading of "D2-style shop" is the item GRID, and that was the first design. It was
 * rejected for this pass, on purpose:
 *
 *  - Every transaction in stores.cpp derives WHICH ITEM from `stextvhold + ((stextlhold - stextup)
 *    / 4)` - the text list's scroll position. A grid has no text lines, so a grid shop means
 *    rewriting all seven transactions, and that arithmetic is the direct cause of the store crash
 *    fixed at v1.8.90 and the three stalled walks found at v1.8.94. Rewriting it to build a
 *    placeholder is a lot of risk bought for visuals that are explicitly not final.
 *  - The tab strip delivers the whole stated goal - one dialog option, everything behind it - and
 *    changes no transaction at all.
 *
 * So the grid stays on the backlog as the visual pass, and this is the structural one. The tabs it
 * defines are exactly the tabs a grid would need, so that pass inherits them.
 *
 * ## Placeholder, and honestly so
 *
 * The strip is drawn with the same ornate border and flat panel every other Oracool window wears
 * while it waits for art. It sits ABOVE the store panel rather than inside it, which is what lets
 * it exist without reflowing a single one of the store's fixed text-line positions.
 */
#pragma once

#include "engine/point.hpp"
#include "engine/rectangle.hpp"
#include "engine/surface.hpp"
#include "stores.h"

namespace devilution::oracool {

/** @brief Whether @p id is a screen that lives behind a vendor's single door. */
bool IsShopTab(TalkID id);

/** @brief The tabs offered alongside @p id, in display order. Empty when @p id is not a shop tab. */
std::vector<TalkID> ShopTabsFor(TalkID id);

/** @brief The short label on @p id's tab, untranslated. */
const char *ShopTabName(TalkID id);

/** @brief The tab strip's screen rect, empty when no shop is open. */
Rectangle GetShopTabStripRect();

/** @brief Draws the strip above the store panel. Does nothing when no shop is open. */
void DrawShopTabs(const Surface &out);

/**
 * @brief Hit-tests the strip. True if the click landed on a tab and switched to it.
 *
 * Called BEFORE the store's own click handling, because the strip sits outside the panel the store
 * hit-tests and would otherwise be dead pixels.
 */
bool CheckShopTabClick(Point position);

} // namespace devilution::oracool
