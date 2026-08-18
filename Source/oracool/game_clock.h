/**
 * @file game_clock.h
 *
 * Oracool: a small real-world wall-clock readout (HH:MM by default, 12-hour optional) pinned to
 * the screen's top-left corner, where nothing else competes for space. The level-up indicator
 * hangs beneath it - see GetLevelUpIconRect in hud_layout.
 */
#pragma once

#include "engine/surface.hpp"

namespace devilution::oracool {

/** @brief Draws the clock if the Game Clock option is on. Call once per frame during gameplay. */
void DrawGameClock(const Surface &out);

/** @brief Draws the game-speed readout in its band under the clock, per the Game Speed Readout
 * option. Call once per frame during gameplay, beside DrawGameClock. */
void DrawGameSpeedReadout(const Surface &out);

} // namespace devilution::oracool
