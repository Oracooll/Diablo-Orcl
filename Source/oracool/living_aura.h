/**
 * @file oracool/living_aura.h
 *
 * Living auras (user, 2026-10-12: "instead of using static asset and colorcycling can you draw and animate their static
 * assets thus infusing them with life? can you simulate one such experiment for me in a render?"): an aura's floor ring
 * drawn by code every frame instead of the painted ring and its colour cycle - flames that dance, frost that glints and
 * drifts, arcs that jump, runes that turn. An EXPERIMENT: drawn only by the preview (DISABLED_LivingAuras) until the user
 * picks; nothing in the game calls it yet.
 */
#pragma once

#include <cstdint>

#include "engine/point.hpp"

namespace devilution {
struct Surface;
} // namespace devilution

namespace devilution::oracool {

enum class LivingAuraKind : uint8_t {
	HolyFire,
	HolyFreeze,
	HolyShock,
	Might,
};

/**
 * @brief The living ring of @p kind round @p centre (the floor point under the hero), @p rx by @p ry pixels (the painted
 * ring's half-size), at @p seconds. Drawn as light, added onto the picture.
 */
void DrawLivingAura(const Surface &out, Point centre, int rx, int ry, LivingAuraKind kind, float seconds);

} // namespace devilution::oracool
