#pragma once

#include "engine/point.hpp"
#include "utils/stdcompat/optional.hpp"

namespace devilution {

/**
 * @brief The Torment picker (user, 2026-10-01): an 800x600 window over the difficulty screen, eight columns, one for each
 * of Hell's eight multipliers (x1.5 to x5). The big pentagram turns in the circle of the chosen column.
 *
 * Front-end rules: a first click on a column, or the arrow keys, only moves the pentagram; a second click on the same
 * column, Enter, or OK takes it. Cancel and Esc go back to the difficulty screen.
 *
 * @param currentTenths the multiplier the pentagram starts on, in tenths (Oracool.tormentDifficultyMultiplier).
 * @return the chosen multiplier in tenths, or nothing on Cancel.
 */
std::optional<int> UiTormentSelectDialog(int currentTenths);

/** @brief Column @p column's circle centre inside the 800x600 art - where its pentagram turns. For the preview render. */
Point TormentPickerCircleInArt(int column);

} // namespace devilution
