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
 * @brief Screen origin of the sheet's CONTENT - the composed field boxes, labels and stat buttons.
 *
 * The composition is a fixed 320x352 block (charbg.clx's size), centred in the panel below the
 * title band. Everything positioned against the sheet goes through GetPanelPosition(
 * UiPanels::Character, ...), which returns this, so drawing, the stat buttons and their
 * hit-testing all move together and cannot disagree.
 */
Point GetCharacterContentOrigin();

extern OptionalOwnedClxSpriteList pChrButtons;

void DrawChr(const Surface &);
void LoadCharPanel();
void FreeCharPanel();

} // namespace devilution
