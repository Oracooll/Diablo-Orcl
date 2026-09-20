/**
 * @file oracool/stonegate_menu.h
 *
 * Oracool: the Stonegate's choice menu (user, 2026-09-20: "when i click on the rift monument a menu
 * asking me which portal i want to open should appear, not open directly gold portal").
 *
 * A small popup centred over the world with three lines: a Nephalem Rift (free, at the deepest
 * floor's tier), a Guardian Rift (turns the best keystone in the backpack; dimmed with none), and
 * closing the gate (or cancelling when it is dark). The window checklist: no click-through, a red X,
 * Escape closes it, closed with every other window.
 */
#pragma once

#include "engine/point.hpp"
#include "engine/rectangle.hpp"
#include "engine/surface.hpp"

namespace devilution::oracool {

bool IsStonegateMenuOpen();
/** @brief Opens the menu (a click on the gate). */
void OpenStonegateMenu();
void CloseStonegateMenu();
/** @brief The popup's screen rect, empty when closed - for click-through rejection. */
Rectangle GetStonegateMenuRect();
/** @brief Draws the popup. Call once per frame, after the world and the HUD. */
void DrawStonegateMenu(const Surface &out);
/** @brief Routes a click. True when the menu consumed it (a click outside it closes it and is consumed too). */
bool CheckStonegateMenuClick(Point mousePosition);
/** @brief Lets go of a pressed button (diablo.cpp's LeftMouseUp): the face springs back from its 2px sink. */
void ReleaseStonegateMenuButton();

} // namespace devilution::oracool
