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

/**
 * @brief Screen x of the centre of the clock's ':' separator.
 *
 * The level-up indicator hangs directly beneath it. Measured from the rendered text rather than
 * assumed, because the colon does not sit at a fixed offset: "20:31" puts it after two digits,
 * "8:31 PM" after one, so a hardcoded x would drift the moment the 12-hour option is toggled or
 * the hour rolls from 9 to 10.
 */
int GetClockColonCentreX();

} // namespace devilution::oracool
