/**
 * @file oracool/levski_roar.h
 *
 * Oracool: Levski's Roar - the crafting monument in town, and the window it opens.
 *
 * Diablo III's Kanai's Cube, with Diablo I's furniture. The transmute area is a 3x4 grid - one row
 * deeper than Kanai's own 3x3, at the user's request, and the same twelve slots Diablo II's
 * Horadric Cube used. The Cube's three legendary-power slots below the grid are absent: legendary
 * powers are not built, and an empty row of slots promising a system that does not exist is worse
 * than no row at all.
 *
 * The monument has its own art as of 2026-08-20: objects\orclroar.cel, three town tiles wide, built
 * by tools/MonumentCel.cs from the user's painting. It is still an OBJ_STAND - the type is only the
 * carrier now, and the sprite is swapped onto the instance (ApplyLevskiRoarGraphics in objects.cpp)
 * so the Caves' actual rock stands are untouched. Before that it wore rockstan.cel outright, as an
 * openly-labelled placeholder.
 *
 * The window still wears the ordinary ornate border every other panel uses, and is still waiting on
 * art of its own.
 *
 * Items placed in the grid are NEVER persisted: closing the window returns them to the backpack.
 * That is what keeps a crafting station out of the save format entirely - there is no state to
 * write, so there is no format to break and nothing to recover if the game exits with the window
 * open.
 */
#pragma once

#include "engine/point.hpp"
#include "engine/rectangle.hpp"
#include "engine/surface.hpp"

namespace devilution {
struct Player;
} // namespace devilution

namespace devilution::oracool {

/** @brief The transmute grid, in cells - 3x4, the Horadric Cube's twelve slots. */
constexpr int LevskiGridColumns = 3;
constexpr int LevskiGridRows = 4;
constexpr int LevskiGridSlots = LevskiGridColumns * LevskiGridRows;

/** @brief Places the monument in town. Called on fresh town generation, like the stash chest. */
void AddLevskiRoarObject();

/** @brief Whether the monument's window is open. */
bool IsLevskiRoarOpen();
/** @brief Opens the window; closes it if already open. Called from the object's operate path. */
void ToggleLevskiRoar();
/** @brief Closes the window and returns everything in the grid to the backpack. */
void CloseLevskiRoar();

/** @brief Whether the recipe book popup is showing. Toggled from the window's own button. */
bool IsLevskiRecipeBookOpen();

/** @brief The window's screen rect - used for click-through rejection like every other panel. */
Rectangle GetLevskiRoarRect();
/** @brief The recipe book's screen rect, empty when closed. */
Rectangle GetLevskiRecipeBookRect();

/** @brief Draws the window and, over it, the recipe book. Call once per frame. */
void DrawLevskiRoar(const Surface &out);

/** @brief Routes a click. True when the click was consumed by the window. */
bool CheckLevskiRoarClick(Point mousePosition);

} // namespace devilution::oracool
