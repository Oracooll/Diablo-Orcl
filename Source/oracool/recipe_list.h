/**
 * @file oracool/recipe_list.h
 *
 * Oracool: the recipe list every crafting page draws.
 *
 * ONE implementation, three callers (user, 2026-09-22: "i want all three recipes tabs of cube,
 * gilian, ogden to behave identically. allign it"). They were three: the Cube listed names only, one
 * per line, on a fifteen-row bezel that scrolled by whole rows; Ogden's and Gillian's wrapped their
 * explanations and scrolled by pixels. Aligning them by editing three draw loops to match would have
 * lasted until the next change to one of them, so there is now nothing to keep in step.
 *
 * The RULES, all three pages: the recipe's name in gold, its explanation in white beneath it and
 * indented, wrapped to as many lines as it needs with no row limit, and the whole list scrolled by
 * the wheel with a thumb when it overflows.
 *
 * The caller owns the OPENING - each page's frame is in a different place and some have no painted
 * frame at all - and the dark layer under it, for the same reason.
 */
#pragma once

#include <vector>

#include "engine/rectangle.hpp"
#include "engine/surface.hpp"

namespace devilution::oracool {

/** @brief How far @p recipes may scroll inside @p opening, in pixels. Zero when they already fit. */
int RecipeListMaxScroll(Rectangle opening, const std::vector<int> &recipes);

/**
 * @brief The recipe whose block is under @p position, or -1.
 *
 * A recipe's block is its name line AND its wrapped explanation, so clicking anywhere in the
 * paragraph picks that recipe rather than only its title.
 *
 * Exists because the Cube's page is not only a reference: two recipes can take the same materials at
 * different costs, so which one runs has to be the player's choice (v1.9.18). Ogden's and Gillian's
 * pages ignore this and are pure reference - the same list, read-only.
 */
int RecipeListHitTest(Rectangle opening, const std::vector<int> &recipes, int scroll, Point position);

/**
 * @brief Draws @p recipes inside @p opening, scrolled by @p scroll.
 *
 * @p scroll is clamped in place, every frame: the content's height depends on how the explanations
 * wrap, so a scroll that was legal when it was set can be past the end by the time it is drawn.
 */
void DrawRecipeList(const Surface &out, Rectangle opening, const std::vector<int> &recipes, int &scroll, int selected = -1);

} // namespace devilution::oracool
