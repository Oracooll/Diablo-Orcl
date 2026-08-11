/**
 * @file oracool/ornate_border.h
 *
 * Oracool: user request - "there is a textbox_frame00 file in the game. looks nice. let's try using
 * it as a border for the minimap and the log."
 *
 * The art itself is a fixed 591x303 CEL, so it cannot frame a 306x175 mini-map or a log window
 * whose height depends on how many entries fit. What it actually IS, though, is a plain 3px bevel,
 * and that reproduces at any size: this module draws the same bevel procedurally, sampled pixel for
 * pixel from the original.
 *
 * Every index it uses comes from the shared upper half of the palette (128-255), which is identical
 * across town and all four dungeon tilesets - so the frame is the same colour everywhere, without
 * the level-specific recolouring the lower half would bring.
 */
#pragma once

#include "engine/rectangle.hpp"
#include "engine/surface.hpp"

namespace devilution::oracool {

/** @brief Thickness of the bevel, in pixels. Content should inset by at least this much. */
constexpr int OrnateBorderWidth = 3;

/**
 * @brief Draws textbox_frame00's bevel around (and just inside) @p rect.
 *
 * Replaces the 1px dashed placeholder the mini-map and event log shared. Clipped, so a rect that
 * runs off the surface is safe.
 */
void DrawOrnateBorder(const Surface &out, Rectangle rect);

} // namespace devilution::oracool
