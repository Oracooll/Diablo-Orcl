/**
 * @file oracool/drawn_fx.h
 *
 * Skill effects drawn by code instead of painted sheets - experiments the user judges from a render before any is wired
 * into the game (user, 2026-10-12: "can you suggest a game-drawn Ground Stomp animation for Barb?").
 */
#pragma once

#include "engine/displacement.hpp"
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

/** @brief How long the drawn Seismic Slam lasts, in seconds; its wave reaches the end of its path at SeismicTravelSeconds. */
constexpr float SeismicSlamSeconds = 1.8F;
constexpr float SeismicTravelSeconds = 0.6F;

/**
 * @brief The drawn Seismic Slam (user, 2026-10-12: "invent a good looking game drawn seismic slam effect and wave"): a wave of
 * earth rolling @p tiles tiles from @p foot along the tile direction (@p dirX, @p dirY) - a rupture tearing open behind a
 * ridge of heaving stone, chips and dust thrown up all along it, a burst at its end - at @p age seconds after the slam.
 * @p floorLayer as DrawGroundStompFx.
 */
void DrawSeismicSlamFx(const Surface &out, Point foot, int dirX, int dirY, int tiles, float age, bool floorLayer);

/** @brief The drawn Earthquake: four pulses a second apart, then a second for the fissures to cool. */
constexpr int EarthquakePulses = 4;
constexpr float EarthquakeSeconds = 5.0F;

/**
 * @brief The drawn Earthquake (user, 2026-10-12: "invent a proper earthquake animation"), round @p centre at @p age seconds:
 * on each pulse a shockwave to the 3-tile reach, the air rippling, stones jumping all over the area and the fissures
 * flaring; fissures out from under him growing with each pulse and arc cracks opening at each pulse's front, molten light
 * in them and a warm glow over the area; dust hanging over it. @p floorLayer as DrawGroundStompFx.
 */
void DrawEarthquakeFx(const Surface &out, Point centre, float age, bool floorLayer);
/** @brief The picture's shake at @p age: a jolt on each pulse, a tremor between (the game's screen shake would carry it). */
Displacement EarthquakeShake(float age);

/** @brief How long the Hammer's glowing cracks last, in seconds. */
constexpr float GlowCracksSeconds = 0.9F;

/**
 * @brief Hammer of the Ancients' cracks (the Skill Eye Candy page, 2026-10-12): eight molten cracks running out from where
 * the blow landed (@p centre, its floor point) at @p age seconds, cooling to dark scars and fading. On the floor only;
 * @p seed picks the cracks' shape, so no two blows crack alike.
 */
void DrawGlowCracksFx(const Surface &out, Point centre, float age, uint32_t seed);

} // namespace devilution::oracool
