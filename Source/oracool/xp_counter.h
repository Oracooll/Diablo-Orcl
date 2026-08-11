/**
 * @file xp_counter.h
 *
 * Oracool: a small readout of experience remaining until the next level, drawn just below the
 * mini-map, horizontally centered - alongside the LOG button (right) and Game Clock (left) in the
 * same row. Doubles as a button: press and hold it to see, in white, the total experience still
 * worth killing every monster currently alive on this level.
 */
#pragma once

#include "engine/point.hpp"
#include "engine/surface.hpp"

namespace devilution::oracool {

/** @brief Whether `mousePosition` is over the XP counter's strip. Used by diablo.cpp to decide
 * whether a click is UI or should fall through to the world - see IsPointOverHud. */
bool IsPointOverXpCounter(Point mousePosition);

/** @brief Draws the counter if the option is on and the player isn't at max level. */
void DrawXpCounter(const Surface &out);

/**
 * @brief Hit-tests the XP Counter's row against a click position; if hit, starts the "held" state
 * (see DrawXpCounter) and returns true, so the caller can let the click fall through otherwise.
 * The held state is cleared unconditionally on mouse-up (diablo.cpp's LeftMouseUp), regardless of
 * where the mouse ends up by then.
 */
bool CheckXpCounterButtonClick(Point mousePosition);

/** @brief Ends the "held" state started by CheckXpCounterButtonClick. Safe to call unconditionally. */
void ReleaseXpCounterButton();

} // namespace devilution::oracool
