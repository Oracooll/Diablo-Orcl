/**
 * @file game_clock.h
 *
 * Oracool: a small real-world wall-clock readout (HH:MM, 24h) drawn just below the mini-map's
 * left edge - mirrors the LOG button, which sits in the same row below the mini-map's right edge.
 */
#pragma once

#include "engine/surface.hpp"

namespace devilution::oracool {

/** @brief Draws the clock if the Game Clock option is on. Call once per frame during gameplay. */
void DrawGameClock(const Surface &out);

} // namespace devilution::oracool
