#pragma once

#include "engine/clx_sprite.hpp"
#include "engine/surface.hpp"

namespace devilution {

/**
 * @brief Screen rect of the character sheet: 340x720, flush to the top-left corner.
 *
 * Its own rect rather than GetLeftPanel()'s, which is 320x352 and shared with the quest log and
 * stash - the same reason the inventory and waypoint list took theirs. Matches the waypoint list
 * and quest log so the four windows are one set.
 */
Rectangle GetCharacterPanelRect();

/**
 * @brief Screen origin of the sheet's CONTENT - the two-column row list and its stat buttons.
 *
 * The panel's left edge, at the top of the area below the title band's separator. Everything
 * positioned against the sheet goes through GetPanelPosition(UiPanels::Character, ...), which
 * returns this, so drawing, the stat buttons and their hit-testing all move together and cannot
 * disagree.
 */
Point GetCharacterContentOrigin();

/**
 * @brief Content-relative position of the Reset Stats button, paired with ResetStatsButtonSize.
 *
 * A function rather than a constant because the button is right-aligned on the "Points to
 * distribute" row, whose y depends on the measured row layout. Both charpanel.cpp's drawing and
 * control.cpp's hit-testing read it, so the two cannot disagree.
 */
Point GetResetStatsButtonPosition();

/**
 * @brief Screen rect of the scrolling row area - below the title separator, above the bottom margin.
 *
 * Hit-testing for anything inside the sheet MUST be gated on this. The + and RESET buttons move
 * with the scroll (see ScrollCharacterSheet), so a button scrolled off the top would otherwise
 * still accept clicks at its new position - under the title band, or off the panel entirely.
 */
Rectangle GetCharacterContentRect();

/**
 * @brief Scrolls the sheet by @p notches wheel steps, positive down. Clamped to the list's extent.
 *
 * The hidden stats below Mana make the list roughly twice the window's height. Scrolling also
 * rewrites the + and RESET buttons' hit rects, since those are positioned per-row.
 */
void ScrollCharacterSheet(int notches);

/** @brief Returns the sheet to the top. Called when the panel is opened. */
void ResetCharacterSheetScroll();

extern OptionalOwnedClxSpriteList pChrButtons;

void DrawChr(const Surface &);
void LoadCharPanel();
void FreeCharPanel();

} // namespace devilution
