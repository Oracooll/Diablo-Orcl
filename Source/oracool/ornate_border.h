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

#include "DiabloUI/ui_flags.hpp"
#include "engine/rectangle.hpp"
#include "engine/surface.hpp"
#include "utils/stdcompat/string_view.hpp"

namespace devilution::oracool {

/** @brief Thickness of the bevel, in pixels. Content should inset by at least this much. */
constexpr int OrnateBorderWidth = 3;

/**
 * @brief Half the bevel's width, for centring a rule ON a boundary rather than starting it there.
 *
 * A rule drawn AT a cell boundary puts its whole width inside the cell to the right, which shifts
 * that cell's visible interior and leaves anything centred in its rect looking off by half the
 * rule. Subtract this from the boundary coordinate to make rect centre and visual centre agree.
 */
constexpr int OrnateBorderWidthHalf = OrnateBorderWidth / 2;

/**
 * @brief Draws textbox_frame00's bevel around (and just inside) @p rect.
 *
 * Replaces the 1px dashed placeholder the mini-map and event log shared. Clipped, so a rect that
 * runs off the surface is safe.
 */
void DrawOrnateBorder(const Surface &out, Rectangle rect);

/**
 * @brief Draws a horizontal rule OrnateBorderWidth tall, in the bevel's own colours.
 *
 * For dividing a panel into sections - a title band from a list, say - so the divider belongs to
 * the same frame instead of looking like a line drawn on top of it.
 */
void DrawOrnateSeparator(const Surface &out, Point from, int width);

/** @brief DrawOrnateSeparator's vertical twin - a rule OrnateBorderWidth wide, @p height tall. */
void DrawOrnateSeparatorVertical(const Surface &out, Point from, int height);

/**
 * @brief Draws @p text with a hard black outline on all four sides - the panel-title treatment.
 *
 * The text renderer has no outline or shadow flag; control.cpp's DrawFlaskValues sets the
 * precedent with a single black draw offset up-left. A title over a half-transparent fill needs it
 * on all sides, since the fill takes the colour of whatever is behind the window.
 *
 * @p style is passed through verbatim, so the caller owns the colour, size and alignment.
 */
void DrawOutlinedString(const Surface &out, string_view text, Rectangle area, UiFlags style);

/**
 * @brief The theme's window fill: a half-transparent darkening of whatever is behind @p rect.
 *
 * @p passes controls how opaque it lands. The renderer is 8-bit palettized with no alpha, so
 * translucency means blending through paletteTransparencyLookup, and that table only gives one
 * strength - half. More opacity therefore means blending repeatedly: one pass is 1/2 toward black,
 * two is 3/4, three is 7/8. Used to sit item slots and the grid more solidly than the panel they
 * are on, so they read as recesses cut into it.
 */
void DrawThemedFill(const Surface &out, Rectangle rect, int passes = 1);

} // namespace devilution::oracool
