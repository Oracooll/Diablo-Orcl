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
 * ## What lives here, and what does not
 *
 * This file answers only "which tabs does this vendor have, and what are they called". It shipped
 * first as a strip above the vanilla store box (v1.9.25); the strip's geometry, drawing and
 * hit-testing moved into oracool/shop_grid.cpp one version later, when the shop became a full
 * panel and the tabs moved inside it.
 *
 * The split is worth keeping: the tab SETS are a statement about the vendors, and they were
 * derived from the menus they replaced (including the multiplayer exclusions, which are inert in
 * single-player V1 but kept, because a tab that appears and then refuses is worse than a tab that
 * is not offered). The tab RECTS are a statement about one particular panel's layout.
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

} // namespace devilution::oracool
