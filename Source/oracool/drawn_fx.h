/**
 * @file oracool/drawn_fx.h
 *
 * Skill effects drawn by code instead of painted sheets - experiments the user judges from a render before any is wired
 * into the game (user, 2026-10-12: "can you suggest a game-drawn Ground Stomp animation for Barb?").
 */
#pragma once

#include "engine/point.hpp"

namespace devilution {
struct Surface;
} // namespace devilution

namespace devilution::oracool {

/** @brief How long the drawn Ground Stomp lasts, in seconds. */
constexpr float GroundStompSeconds = 1.4F;

/**
 * @brief The drawn Ground Stomp at @p age seconds after the foot came down at @p foot (the floor point under the hero).
 * @p floorLayer: the parts on the floor (cracks, the shockwave's light and dust, the air's ripple, the flash's pool),
 * drawn before the hero; otherwise the parts in the air (stone chips, dust clouds, the flash's core), drawn after him.
 */
void DrawGroundStompFx(const Surface &out, Point foot, float age, bool floorLayer);

} // namespace devilution::oracool
