/**
 * @file oracool/levski_cube_skin.h
 *
 * Levski's Cube (2026-09-20, RfA-20 batch 43b): the Cube's own window skin, ui\cube_bg.png, worn when
 * the window opens on the Cube host. The three shop hosts (Griswold's Forge, Ogden's table, Gillian's
 * hearth) keep the Roar's painting and its salvage block; see levski_roar_skin.h.
 *
 * MEASURED from the delivered art (cube_window.png, 385x280) by row and column scans - every rect
 * below is in WINDOW pixels. The layout the brief asked for, as GPT painted it: the twelve wells at
 * the Roar's grid origin, a 142x26 button recess under the grid, a recipe bezel on the right with
 * eight line positions at a 20 px pitch and a scroll track beside it. No salvage cells and no
 * recipe-book plate: the recipes are listed IN the bezel.
 */
#pragma once

#include "engine/point.hpp"
#include "engine/rectangle.hpp"
#include "engine/size.hpp"

namespace devilution::oracool::cube_skin {

constexpr const char *BackgroundAsset = "ui\cube_bg.png";
/** RfA-20 batch 43c: the shop's small button style, the word TRANSMUTE, at rest and pressed. Until it
 * lands the recess is labelled by the game's own gold text. */
constexpr const char *TransmuteButtonAsset = "ui\cube_button_transmute.png";
constexpr const char *TransmuteButtonPressedAsset = "ui\cube_button_transmute_pressed.png";

constexpr Size WindowSize { 385, 280 };
/** The wells: brass rims at x 25-26 / 53-54, interiors of 26 px on a 28 px pitch - the Roar's grid exactly. */
constexpr Point GridOrigin { 26, 106 };
/** The recess under the grid: rim at x 17-18 / 159-160 and y 229-232 / 257-258; the interior is 140x24. */
constexpr Rectangle TransmuteRect { { 18, 231 }, { 142, 26 } };
/** The bezel's interior: x 179..332, y 77..248; dividers at y 96, 116, ... 216. */
constexpr Rectangle RecipeListRect { { 179, 77 }, { 154, 172 } };
constexpr int RecipeLinePitch = 20;
constexpr int RecipeLines = 8;
/** The narrow track right of the bezel (x 344..348) where the scroll thumb runs. */
constexpr Rectangle ScrollTrackRect { { 344, 77 }, { 5, 172 } };
/** The game's red X, in the corner ornament's place (a 13 px brass cube sits there in the painting). */
constexpr Rectangle CloseRect { { 364, 3 }, { 18, 18 } };

} // namespace devilution::oracool::cube_skin
