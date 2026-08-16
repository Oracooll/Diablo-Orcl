/**
 * @file oracool/crafting_menu.h
 *
 * Oracool: Megaplan Phase 1 - the crafting window. Same self-contained pattern as the waypoint
 * list (own open state, own draw, own click handler, routed as a LeftPanelContent so the shared
 * left-panel machinery gives it click absorption and mutual exclusion for free). Opened from the
 * belt's burger menu; ESC and ClosePanels close it. Single-player only, like the recipes it hosts.
 */
#pragma once

#include "engine/point.hpp"
#include "engine/rectangle.hpp"
#include "engine/surface.hpp"

namespace devilution::oracool {

bool IsCraftingMenuOpen();
void OpenCraftingMenu();
void CloseCraftingMenu();

/** @brief Screen rect, exported for control.cpp's GetLeftPanelContentRect click routing. */
Rectangle GetCraftingMenuRect();

/** @brief Draws the recipe list: craftable rows in white with gold names, uncraftable dimmed. */
void DrawCraftingMenu(const Surface &out);

/** @brief Left-click while open: crafting a rowable recipe runs it and logs the result. */
void CheckCraftingMenuClick(Point mousePosition);

} // namespace devilution::oracool
