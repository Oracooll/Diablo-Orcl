/**
 * @file oracool/particles.h
 *
 * Particles (user, 2026-10-10): a small pool of world-space specks drawn over the finished world each frame.
 *
 *  - Embers rise from the level's flames (torches, candles, braziers, the burning crosses) near the hero.
 *  - Sparks fly from a landed blow and from a missile's impact: steel white-yellow for a physical hit, the element's
 *    colour for an elemental one.
 *  - Dust motes float slowly in the hero's light in the dungeon.
 *  - Ash drifts down in Hell.
 *
 * Purely visual: the pool lives in real time (SDL_GetTicks, a step clamped to 50 ms), has its own xorshift and never
 * touches the game's RNG or any game state, so an emitter hook in the rules (EmitHitSparks) cannot change an outcome.
 * Off with the option (the pool is emptied), and nothing is drawn on an 8-bit surface. Town gets embers only.
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include "engine/point.hpp"

namespace devilution {
struct Surface;
struct Missile;
enum class DamageType : uint8_t;
} // namespace devilution

namespace devilution::oracool {

/** @brief The pool's size: an emitter past it is ignored. */
constexpr size_t MaxParticles = 1024;

/** @brief A physical blow's sparks: steel white-yellow. */
constexpr uint32_t SteelSparkRgb = 0xFFF0C0;

/**
 * @brief Advances the pool by the real time since the last call, runs the ambient emitters and draws every particle onto
 * @p out. Once per drawn frame, after the world (and its light passes) and before the HUD; @p firstTile and
 * @p firstTileBuffer are the world draw's (its first tile's 64x32 box has its bottom-left at firstTileBuffer).
 */
void UpdateAndDrawParticles(const Surface &out, Point firstTile, Point firstTileBuffer);

/** @brief A burst of @p count sparks at @p tile's centre, chest high, in @p rgb (0xRRGGBB). */
void EmitHitSparks(Point tile, uint32_t rgb, int count);

/** @brief A burst of sparks where @p missile is, in its colour (MissileSparkRgb). */
void EmitMissileSparks(const Missile &missile, int count);

/** @brief The sparks' colour for a blow of @p element: SteelSparkRgb for physical. */
uint32_t SparkRgb(DamageType element);

/** @brief The sparks' colour for @p missile: its tint's hue, else its element's, else steel. */
uint32_t MissileSparkRgb(const Missile &missile);

/** @brief Empties the pool. On a level change (UpdateAndDrawParticles also notices one by itself). */
void ForgetParticles();

/** @brief How many particles are alive. */
size_t ParticleCount();

} // namespace devilution::oracool
