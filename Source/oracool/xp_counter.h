/**
 * @file xp_counter.h
 *
 * Oracool: a small readout of experience remaining until the next level, drawn just below the
 * mini-map, horizontally centered - alongside the LOG button (right) and Game Clock (left) in the
 * same row.
 */
#pragma once

#include "engine/surface.hpp"

namespace devilution::oracool {

/** @brief Draws the counter if the option is on and the player isn't at max level. */
void DrawXpCounter(const Surface &out);

} // namespace devilution::oracool
