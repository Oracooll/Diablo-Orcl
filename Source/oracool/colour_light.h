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

#include <cstddef>
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

/**
 * Lit surface relief (user, 2026-10-11, "normal maps" in the second graphics batch): walls, objects and figures catch the
 * light from the side it comes from. Each pixel drawn over the floor takes its brightness as its height, and the light's
 * direction at its tile (every light's pull, weighted by falloff, plus a lean upward) against that slope brightens what
 * faces the light and shades what faces away, up to about a fifth either way. The floor is left as it is. Dungeon only,
 * 32-bit only, behind its option.
 */
bool SurfaceReliefActive(const Surface &out);
/** @brief Keeps the world as it stands before DrawTileContent: what changes after it is what the relief works on. */
void BeginSurfaceRelief(const Surface &out);
/** @brief The relief over what DrawTileContent drew; same start arguments as ApplySmoothFloorLight. */
void ApplySurfaceRelief(const Surface &out, Point firstTile, Point firstTileBuffer);
/** @brief Test hook: the relief's light direction at @p tile, screen space, 8.8 (256 = full strength). */
void ReliefDirectionAt(Point tile, int &dx, int &dy);

/**
 * Day and night in Tristram (user, 2026-10-11): a twenty-minute day on the town's own clock (stopped while paused). Dawn
 * is rose, the day untouched, dusk amber, night dark blue - and after dark the town is lit by the heroes' own light, a
 * lamp by each of the townsfolk, the portals and the loot, as tints over the finished town (the colour pass's loop).
 * Town only, 32-bit only, behind its option. Called every frame (the clock runs in the dungeon too).
 */
void ApplyTownDayNight(const Surface &out, Point firstTile, Point firstTileBuffer);
/** @brief The town's clock, 0..24 hours. */
float TownHour();
/** @brief Sets the town's clock (the timeofday debug command, the tests). */
void SetTownHour(float hour);
/** @brief Test hook: the ambient light at @p hour (256 = full day). */
LightTint TownAmbientAt(float hour);
/** @brief Test hook: the tint the last town pass gave @p tile. */
LightTint TownTintAt(Point tile);

// ----- Shared with the graphics card's passes (oracool/gpu_post.h, the GPU renderer plan's Stage 2): each pass split
// into its plan - everything worked out once a frame - and the per-pixel work, which the CPU or the card then does with
// the same numbers. ----------------------------------------------------------------------------------------------------

/** @brief A tint over a screen rectangle: the per-tile field (FieldW x FieldH, x major) and the floor map's origin. */
struct TintPlan {
	const LightTint *tints = nullptr;
	int left = 0, right = -1, top = 0, bottom = -1; // inclusive; empty when left > right
	int64_t u0 = 0, v0 = 0;                          // the GroundMap's origin, 16.16 tiles (steps are fixed)
};
/** @brief What ApplyLightColour would do this frame (the colour field and the flicker); false for nothing. */
bool PlanLightColour(const Surface &out, Point firstTile, Point firstTileBuffer, TintPlan &plan);
/** @brief What ApplyTownDayNight would do this frame (its clock not moved); false for nothing. */
bool PlanTownDayNight(const Surface &out, Point firstTile, Point firstTileBuffer, TintPlan &plan);
/** @brief Moves Tristram's clock on, as ApplyTownDayNight does each frame. */
void AdvanceTownDayNightClock();
/** @brief The CPU's per-pixel work for a plan. */
void ApplyTintPlan(const Surface &out, const TintPlan &plan);
/**
 * @brief One plan that does what @p colour then @p dayNight do (the GPU takes one tint): the two fields multiplied over the
 * day-and-night plan's rectangle. Both are on the same GroundMap - planned for the same frame.
 */
TintPlan CombineTintPlans(const TintPlan &colour, const TintPlan &dayNight);

/** @brief The relief's numbers: its direction field (FieldW x FieldH, x major, 8.8), the outline colours it leaves be. */
struct ReliefPlan {
	const int16_t *dirX = nullptr;
	const int16_t *dirY = nullptr;
	uint32_t outline[6] {};
	int64_t u0 = 0, v0 = 0;
	int slopeShift = 9;
	int maxDelta = 40;
	bool reliefPass = true; // false: only the mask (the outline colours, the floor map) for Stage 3's Lit Sprites
};
// ----- Stage 3's inputs (oracool/gpu_post.h) ------------------------------------------------------------------------
struct GpuLight;
struct GpuHeat;
/** @brief Up to @p max lights nearest @p centre: the dungeon's light sources, or Tristram's after dark. */
size_t GpuLightsNear(Point centre, GpuLight *out, size_t max);
/** @brief Up to @p max sources of heat nearest @p centre: fire spells, and the level's flames. */
size_t GpuHeatNear(Point centre, GpuHeat *out, size_t max);
/** @brief What stops light, a byte a tile (FieldW x FieldH, x major), kept with the level. */
const uint8_t *LightBlockingMap();
/** @brief True while Tristram is dim enough (dusk to dawn) for its lamps to count. */
bool TownIsDark();

/** @brief The relief's plan when it applies to @p out this frame. */
bool PlanSurfaceRelief(const Surface &out, Point firstTile, Point firstTileBuffer, ReliefPlan &plan);
/** @brief Lit Sprites' plan (Stage 3, the graphics card only): the relief's mask without its pass - in town as well. */
bool PlanLitSprites(const Surface &out, Point firstTile, Point firstTileBuffer, ReliefPlan &plan);
/** @brief The CPU's relief over @p out, against @p kept - the world as it was before DrawTileContent, @p out's size, packed. */
void ApplyReliefPlan(const Surface &out, const uint32_t *kept, const ReliefPlan &plan);
/** @brief Test hook: the colour multiplier the last rebuild gave @p tile. */
LightTint LightTintAt(Point tile);

/** @brief Test hook: the brightness the smooth pass uses for a darkness level, 0..256 (0 = black, 256 = full). */
uint16_t SmoothLevelBrightness(uint8_t level);

/** @brief Test hook: the colour a light with this id is given (0 = neutral), as found by the last rebuild. */
uint32_t LightColourForId(int lightId);

} // namespace devilution::oracool
