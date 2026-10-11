/**
 * @file oracool/scene_life.h
 *
 * The fourth graphics batch (user, 2026-10-11: "proceed with all suggested"), each behind its own option, all purely
 * visual - they read the game and never write it, and run on the third batch's clock (scene_fx.h SceneSeconds, standing
 * still while the game is paused):
 *
 *  - Cloud shadows: soft shadows of clouds drift over Tristram by day.
 *  - Town life: fireflies over Tristram at night, crows on the ground by day that take off as the hero comes near, and
 *    leaves blowing through.
 *  - Puddles: rain leaves puddles on Tristram's ground, rippled while it still falls, drying slowly after it stops.
 *  - Footprints: the hero leaves prints in Tristram's snow and on its wet ground, fading behind him.
 *  - Torch smoke: wisps of smoke rise from the dungeons' torches, candles and burning crosses.
 *  - Swing trails: a melee swing leaves a bright arc, coloured by the weapon's fire or lightning.
 *  - Missile trails: arrows, bolts and spells in flight draw a fading streak behind them in their element's colour.
 *  - Shockwaves: big explosions, novas and impacts send a ripple across the picture that bends what lies under it.
 *  - The guardian's death moment: a rift guardian's fall flashes the screen, drains the colour from it a moment, shakes
 *    it, sends a shockwave out and shows "VANQUISHED" and his name.
 *  - Drop flares: a unique, set, primal or runeword item flares like a star as it lands.
 *  - Living portals: town portals, red portals and the rifts' portals swirl, glow on the floor and draw motes into them.
 *
 * Where each is drawn, and why there (the third batch's rule: anything changed after the world is kept for the relief and
 * Lit Sprites is read as a wall or a sprite):
 *  - ApplySceneLifeGround: puddles and footprints, on the floor with the weather's ground, before anything stands on it.
 *  - DrawSceneLifeBodies: cloud shadows, crows, leaves and smoke, over the drawn world and before its light passes, so
 *    day and night fall on them too.
 *  - DrawSceneLifeLights: fireflies, swing and missile trails, drop flares and the portals' glow - light, beside the
 *    loot beams (collected as splats when the graphics card finishes the frame).
 *  - SwirlPortal: a portal's own pixels turned about its middle, right after it is drawn.
 *  - ApplySceneLifeAfterWorld: shockwaves and the death moment's flash, over the finished (zoomed) world.
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include "engine/displacement.hpp"
#include "engine/point.hpp"

namespace devilution {
struct Surface;
struct Missile;
} // namespace devilution

namespace devilution::oracool {

/**
 * @brief Once a frame, from DrawGame before the world is drawn: where the world lands on the screen this frame (the
 * pre-zoom surface @p visible, which ZoomScale or the card stretches over the full width of @p fullOut by @p zoom), and
 * the batch's per-frame bookkeeping - the crows, the footprints, the drops landing, the explosions, a guardian's fall.
 */
void NoteWorldView(const Surface &visible, const Surface &fullOut, Point firstTile, Point firstTileBuffer, float zoom);

/** @brief Puddles and footprints, on the lit floor (beside ApplyWeatherGround). Tristram only. */
void ApplySceneLifeGround(const Surface &out, Point firstTile, Point firstTileBuffer);
/** @brief Cloud shadows, crows, leaves and torch smoke: after the sprites, before the light passes. */
void DrawSceneLifeBodies(const Surface &out, Point firstTile, Point firstTileBuffer);
/** @brief Fireflies, swing and missile trails, drop flares and the portals' glow: beside the loot beams (splat-aware). */
void DrawSceneLifeLights(const Surface &out, Point firstTile, Point firstTileBuffer);
/** @brief Whether @p missile is a portal that swirls (the town portal, the red portal, the rifts' two). */
bool IsLivingPortal(const Missile &missile);
/** @brief Turns the portal's middle about itself, right after DrawMissile drew it at @p targetBufferPosition. */
void SwirlPortal(const Surface &out, const Missile &missile, Point targetBufferPosition);
/** @brief The shockwaves and the death moment's flash and grey, over the finished world (before the third batch's shake). */
void ApplySceneLifeAfterWorld(const Surface &world);
/** @brief "VANQUISHED" and the fallen guardian's name, over the HUD (beside the entrance card). */
void DrawGuardianDeathCard(const Surface &out);

/** @brief A shockwave from the tile @p tile (moved by @p offset screen pixels), 1 = a big explosion. */
void AddShockwave(Point tile, Displacement offset, float strength);

/** @brief Clears every per-level record; ForgetSceneFx calls it (a new level, the clock's wrap). */
void ForgetSceneLife();

// ---- Test hooks -------------------------------------------------------------------------------------------------------

/** @brief How wet Tristram's ground is now (0 dry .. 1 soaked): rises while it rains, dries after. */
float GroundWetness();
/** @brief Sets the wetness at once (tests, and the debug commands `weather wet` and `weather dry`). */
void SetGroundWetness(float wetness);
/** @brief Advances the wetness by @p seconds of the weather now (tests). */
void AdvanceGroundWetness(float seconds);
/** @brief The footprints on record. */
size_t FootprintCount();
/** @brief The shockwaves under way. */
size_t ShockwaveCount();
/** @brief Whether the tile holds a puddle when the ground is wet through (a fixed scatter: about one tile in six). */
bool TileHoldsPuddle(Point tile);
/** @brief The trail colour of a missile type's element (0 = no trail). */
uint32_t MissileTrailColour(int missileId);

} // namespace devilution::oracool
