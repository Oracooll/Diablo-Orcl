/**
 * @file oracool/hero_stats_column.h
 *
 * Oracool: user request (2026-08-31) - "we need to add in the hero selection screen in the front end
 * a hero stats somewhere befitting".
 *
 * The character list sits in the button row's fourth zone and the animated figure stands in the
 * middle, which left the whole left band of that screen empty. This fills it with the focused
 * character's numbers, so the two columns frame the figure instead of the screen leaning right.
 *
 * It deliberately does NOT restore the old class portrait and its five stat rows: that block was
 * removed on 2026-08-13 to make room for the animated figure, and the figure keeps its space. This
 * is the other side of it.
 *
 * Drawn directly rather than pushed as UiItems, for the reason the figure is: it has nothing to
 * click, nothing to focus and no keyboard behaviour, and a dozen UiArtText widgets rebuilt on every
 * focus change would be a dozen more things to free correctly.
 */
#pragma once

#include "DiabloUI/diabloui.h"
#include "engine/rectangle.hpp"
#include "engine/surface.hpp"

namespace devilution::oracool {

/**
 * @brief Draws @p hero's stats into @p area, laid out as label/value rows.
 *
 * @p area is the whole column; the rows are centred vertically inside it, the way the character
 * list centres itself in the same band.
 */
void DrawHeroStatsColumn(const Surface &out, Rectangle area, const _uiheroinfo &hero);

} // namespace devilution::oracool
