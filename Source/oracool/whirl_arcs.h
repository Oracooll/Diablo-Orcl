/**
 * @file oracool/whirl_arcs.h
 *
 * Whirlwind's arcs (user, 2026-10-12: "add lightning effects around my barb while he is whirlwinding? Some orange-ish
 * swirling arcs of magic revolving around him"): three jagged arcs of orange lightning circling the spinning Barbarian
 * at waist, chest and head height, each a bright core in a warm glow, forking now and then, with a flickering orange
 * light on the floor under him.
 *
 * Drawn as light: added onto the picture, or handed to the graphics card as splats on its path. The far half of the ring
 * goes behind him (before his body), the near half and the floor's light over him (the light pass).
 */
#pragma once

#include "engine/point.hpp"

namespace devilution {
struct Surface;
} // namespace devilution

namespace devilution::oracool {

/**
 * @brief The arcs round a spinner whose feet are at @p foot (DrawPlayer's target position), at @p seconds on the scene
 * clock. @p front draws the half of the ring on the camera's side, false the far half. @p floor adds the light on the
 * floor (with the front half). Splat-aware: on the graphics card's path the pixels go to its splats.
 */
void DrawWhirlArcs(const Surface &out, Point foot, float seconds, bool front, bool floor);

} // namespace devilution::oracool
