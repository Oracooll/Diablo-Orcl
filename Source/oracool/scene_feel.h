/**
 * @file oracool/scene_feel.h
 *
 * The fifth graphics batch (user, 2026-10-11: "do all 10 suggestions, an audit loop and let me test afterwards"), each
 * behind its own option, all purely visual - read the game, never write it, on the third batch's clock:
 *
 *  - Hit Flash: a monster struck flashes white for a moment.
 *  - Impact Lights: a spell or arrow striking home lights the floor and walls around it in its element's colour.
 *  - Area Atmosphere: dust motes in the Cathedral's and the Catacombs' light, drips in the Caves, ash and embers in Hell,
 *    wisps in the Crypt, spores in the Nest.
 *  - Lava Light: lava casts a flickering warm glow on what stands near it.
 *  - Smooth Camera: the view glides after a Teleport or a Leap instead of jumping, and leans in a little while a rift
 *    guardian is near.
 *  - Wet Floors: figures are faintly mirrored in the damp floors of the Caves and the Nest (scene_fx.cpp's reflections).
 *  - Drop Effects: a tumbling item casts a shadow, a rare one trails sparkles, and each lands in a puff of dust.
 *  - Death Effects: a corpse lands in a puff of dust; a hard blow or a kill sprays blood (bone chips from the undead).
 *  - Window Light: soft beams slant down from high on the Cathedral's walls, motes turning in them. Diablo's tilesets have
 *    no window pieces, so these are placed on walls that face a room - stylised, not traced.
 *  - Level Transition: a level change closes and opens like an iris around the middle of the screen instead of a flat
 *    fade (engine/dx.cpp asks IrisFadeAt).
 */
#pragma once

#include <cstdint>

#include "engine/clx_sprite.hpp"
#include "engine/displacement.hpp"
#include "engine/point.hpp"

namespace devilution {
struct Surface;
struct Monster;
} // namespace devilution

namespace devilution::oracool {

/** @brief Once a frame from DrawGame, beside NoteWorldView: the batch's bookkeeping (particles, drops, deaths, impacts). */
void TickSceneFeel(const Surface &visible, Point firstTile, Point firstTileBuffer);

/** @brief From M_StartHit: @p monster was struck for @p damage (64ths of a hit point). Visual only. */
void NoteMonsterHit(const Monster &monster, int damage);
/** @brief The white flash over a struck monster, right after its body is drawn. */
void DrawMonsterHitFlash(const Surface &out, const Monster &monster, Point spriteBottomLeft, ClxSprite sprite);

/** @brief Lava light, drop shadows, blood and dust: after the sprites, before the light passes. */
void DrawSceneFeelBodies(const Surface &out, Point firstTile, Point firstTileBuffer);
/** @brief Impact lights, atmosphere, sparkles and window beams: beside the loot beams (splat-aware). */
void DrawSceneFeelLights(const Surface &out, Point firstTile, Point firstTileBuffer);
/** @brief The guardian's lean-in, over the finished (zoomed) world, before the shake. */
void ApplySceneFeelAfterWorld(const Surface &world);
/** @brief The screen point @p screen aims at in the world as it would be drawn without the glide, shake and lean-in. */
Point WorldPointerFor(Point screen);
/** @brief The camera's glide after a jump, in pixels; scene_fx's ScreenShakeOffset adds it to the shake. */
Displacement CameraGlideOffset();

/**
 * @brief The level transition's iris: how much of a pixel at (@p x, @p y) of a @p w x @p h screen shows (0..256) at fade
 * level @p fade (0 black .. 256 clear), or -1 for the plain fade (the option off, or out of a game).
 */
int IrisFadeAt(int x, int y, int w, int h, int fade);
/** @brief A whole row of IrisFadeAt into @p out (w entries), for the present path - optimised even in Debug. */
void IrisFadeRow(int y, int w, int h, int fade, uint16_t *out);
/** @brief Whether the iris replaces the fade now. */
bool IrisFadeActive();

/** @brief Whether the Caves' and the Nest's floors mirror figures now (scene_fx.cpp asks). */
bool DampFloorsHere();

/** @brief Clears every per-level record; ForgetSceneFx calls it. */
void ForgetSceneFeel();

// ---- Test hooks -------------------------------------------------------------------------------------------------------

/** @brief The particles alive now. */
int SceneFeelParticleCount();
/** @brief The hit flash's strength for a monster struck @p age seconds ago (0..1). */
float HitFlashStrength(float age);

} // namespace devilution::oracool
