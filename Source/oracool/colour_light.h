/**
 * @file oracool/colour_light.h
 *
 * Coloured and smooth dynamic lighting (user, 2026-10-10, inspired by DamnationX's dynamic lights).
 *
 * The engine's light stays exactly what it was: one darkness level per tile in dLight (0 lit .. 15 black), worked out
 * once a tick by ProcessLightList, deciding what the hero sees and how dark everything is drawn. Two passes over the
 * WORLD ONLY (scrollrt.cpp's DrawGame, before the zoom and before any of the HUD) are added on top of it:
 *
 *  - Smooth floors: with the option on, every floor tile is drawn at full brightness and this pass darkens it by the
 *    light level interpolated between tile centres, so a light fades across the floor instead of in tile-sized bands.
 *    Walls and sprites keep the per-tile light; a tile at level 15 is still drawn black, so the dark stays as dark.
 *  - Colour: every light gets a colour from what owns it (a fire missile orange, lightning blue-white, holy gold,
 *    poison green, cold cyan, a champion or unique the hue of its own recolour, torches and candles warm; the hero's
 *    own light stays neutral). A per-tile colour field is rebuilt with the light list, and the pass shifts the hue of
 *    every world pixel by the field under it, keeping its brightness.
 *
 * Town is never touched (it has no lighting), and neither is an 8-bit surface (the golden tests' world draws).
 */
#pragma once

#include <cstdint>

#include "engine/point.hpp"

namespace devilution {
struct Surface;
}

namespace devilution::oracool {

/** @brief Rebuilds the per-tile colour field from the live lights. Called at the end of every full ProcessLightList. */
void RebuildLightColourField();

/** @brief True when the floor pass applies to @p out: the option is on, the surface is 32-bit, and this is a lit dungeon. */
bool SmoothFloorLightActive(const Surface &out);

/**
 * @brief True when the floor pass runs at all: smooth floors, or wall shadows (user, 2026-10-10). The floor-pass objects
 * wait for it, and DrawCell must not redraw a floor triangle over it.
 */
bool FloorPassActive(const Surface &out);

/** @brief True when the colour pass applies to @p out (coloured lights or torch flicker on, a 32-bit surface, a lit dungeon). */
bool ColouredLightActive(const Surface &out);

/**
 * @brief Darkens the floor just drawn (drawn at full brightness, see SmoothFloorLightActive) by the light interpolated
 * between tile centres. @p firstTile and @p firstTileBuffer are DrawFloor's start: that tile's bottom-left in @p out.
 */
void ApplySmoothFloorLight(const Surface &out, Point firstTile, Point firstTileBuffer);

/** @brief Tints the finished world in @p out by the colour field; same start arguments as ApplySmoothFloorLight. */
void ApplyLightColour(const Surface &out, Point firstTile, Point firstTileBuffer);

/** @brief A per-channel colour multiplier, 8.8 fixed point (256 = unchanged). */
struct LightTint {
	uint16_t r;
	uint16_t g;
	uint16_t b;
};
/** @brief Test hook: the colour multiplier the last rebuild gave @p tile. */
LightTint LightTintAt(Point tile);

/** @brief Test hook: the brightness the smooth pass uses for a darkness level, 0..256 (0 = black, 256 = full). */
uint16_t SmoothLevelBrightness(uint8_t level);

/** @brief Test hook: the colour a light with this id is given (0 = neutral), as found by the last rebuild. */
uint32_t LightColourForId(int lightId);

} // namespace devilution::oracool
