/**
 * @file oracool/cursor_tooltip.h
 *
 * Oracool: user request - Phase 1 of the 960x720 HUD overhaul. Replaces the old fixed-position
 * black description box (control.cpp's removed DrawInfoBox) with a small box that follows the
 * mouse cursor instead, clamped to stay fully on-screen. Reuses InfoString/InfoColor
 * (control.h) exactly as before - every hover system (items, monsters, NPCs, players, gold,
 * held-item) already populates them correctly each frame via UpdateInfoString(); only the render
 * *location* changes here, not what triggers it or what text it shows.
 */
#pragma once

#include "engine/rectangle.hpp"
#include "engine/surface.hpp"

namespace devilution::oracool {

/**
 * @brief Draws InfoString/InfoColor (if non-empty) in a small box anchored near the mouse cursor.
 * No-op if InfoString is empty or chat input is active. Called late in the draw order (see
 * scrollrt.cpp's DrawAndBlit) - after the world/panel/popup passes, just before DrawCursor, so the
 * tooltip never renders underneath the cursor sprite.
 */
void DrawCursorTooltip(const Surface &out);

/**
 * @brief The screen rect DrawCursorTooltip drew to last frame, or an empty rect if nothing was
 * drawn. Used by scrollrt.cpp's DrawMain (the <=640-wide dirty-rect render path) to erase the
 * tooltip's previous position each frame, the same way PrevCursorRect tracks the cursor sprite -
 * see that variable's usage in scrollrt.cpp for the pattern this mirrors.
 */
Rectangle GetPrevCursorTooltipRect();

} // namespace devilution::oracool
