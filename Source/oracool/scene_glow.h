/**
 * @file oracool/scene_glow.h
 *
 * The sixth graphics batch (user, 2026-10-11: "do all 20 suggestions, an audit loop and let me test afterwards"), each
 * behind its own option, all purely visual - read the game, never write it, on the third batch's clock:
 *
 *  1. Level-Up Burst: a column of golden light, a ring across the floor and sparks rising when the hero gains a level.
 *  2. Casting Glow: the floor round a caster lights in the spell's colour while the spell is cast.
 *  3. Potion Effects: a big gain of life or mana in one go (a potion, a heal, a shrine) swirls red or blue round the hero.
 *  4. Pickup Glints: gold and magic items on the floor glint now and then; picking one up throws a few sparkles.
 *  5. Chest and Door Effects: a chest opens in a burst of gold sparks and light; a door opening or closing raises dust.
 *  6. Elite Auras: champions and uniques stand in a slowly turning ring on the floor, in their own colour.
 *  7. Hero Hurt Flash: a blow that hurts flashes the hero red and pulses red at the screen's edges.
 *  8. Shrine Glow: unused shrines and the fountains glow softly, motes rising from them.
 *  9. Lit Homes: after dark Tristram's homes show warm windows and light the ground before them.
 * 10. Lens Glints: the brightest flames and fire spells throw a short horizontal streak of light.
 * 11. Spell Charge: motes gather into the hero's hands while a spell is cast, and flash as it leaves.
 * 12. Status Lights: a burning monster lights the floor round it with flickering fire, a frozen one with cold.
 * 13. Blood Trails: a hard blow splashes blood on the floor; a badly wounded monster leaves drips where it walks.
 * 14. Lingering Marks: fire that stays on the ground (Fire Wall, Flame Wave, Inferno) scorches it; cold that strikes
 *     home leaves frost. Both through the floor marks (oracool/ground_fx.h).
 * 15. Boss Presence: a boss or rift guardian darkens the floor round him and the edges of the screen while he is near.
 * 16. Orb Slosh: the HUD orbs slosh when they change quickly, and a slow glint crosses their surface (hud_art.cpp asks).
 * 17. Exit Glow: stairs and level entrances breathe a faint light - cold going down, warm going up.
 * 18. Corpse Settle: a body cools toward grey over its first minute, so fresh kills stand out from old ones.
 * 19. Town Forge: Griswold's forge glows and throws sparks as he works.
 * 20. Armour Sparks: a melee blow on a skeleton, a knight, a golem or a gargoyle throws white-gold sparks.
 */
#pragma once

#include <cstdint>
#include <vector>

#include "engine/clx_sprite.hpp"
#include "engine/point.hpp"

namespace devilution {
struct Surface;
struct Monster;
struct Player;
} // namespace devilution

namespace devilution::oracool {

/** @brief Once a frame from DrawGame, after TickSceneFeel: the batch's bookkeeping. */
void TickSceneGlow(const Surface &visible, Point firstTile, Point firstTileBuffer);

/** @brief On the lit floor, before the figures: elite rings, a boss's darkness, the exits' floor glow. */
void DrawSceneGlowGround(const Surface &out, Point firstTile, Point firstTileBuffer);
/** @brief Dust and smoke (covering, not light): after the sprites, before the light passes. */
void DrawSceneGlowBodies(const Surface &out, Point firstTile, Point firstTileBuffer);
/** @brief The batch's light: beside the loot beams (splat-aware). */
void DrawSceneGlowLights(const Surface &out, Point firstTile, Point firstTileBuffer);
/** @brief The hurt pulse and the boss's darkness at the screen's edges: over the finished world, before the HUD. */
void ApplySceneGlowAfterWorld(const Surface &world);

/** @brief From M_StartHit: @p monster was struck for @p damage (64ths). Blood, sparks - visual only. */
void NoteMonsterHitGlow(const Monster &monster, int damage);
/** @brief Right after our hero's body is drawn: the red flash of a blow that hurt. */
void DrawHeroHurtFlash(const Surface &out, const Player &player, Point spriteBottomLeft, ClxSprite sprite);
/** @brief Right after a corpse is drawn on @p tile: its cooling toward grey. */
void DrawCorpseSettle(const Surface &out, Point tile, Point spriteBottomLeft, ClxSprite sprite, int lightTableIndex);

/** @brief A home lamp for Tristram's lights after dark (colour_light.cpp asks). Tile coordinates. */
struct HomeLamp {
	float x, y, reach, strength;
	uint32_t rgb;
};
/** @brief The homes' lamps and the forge's glow, for the town's light field; empty when their options are off. */
const std::vector<HomeLamp> &TownHomeLamps();

/** @brief The orbs' slosh (hud_art.cpp): extra surface height at column @p x for the orb keyed @p key, whose fill line
 * stands at @p revealTop. Call once per orb per frame with @p x < 0 first to note the line. */
float OrbSlosh(int key, int x, int revealTop);
/** @brief The glint across the orb's surface at column @p x of an orb @p width wide (0..1). */
float OrbGlint(int key, int x, int width);

/** @brief Clears every per-level record; ForgetSceneFx calls it. */
void ForgetSceneGlow();

// ---- Test hooks -------------------------------------------------------------------------------------------------------

/** @brief The particles alive now. */
int SceneGlowParticleCount();
/** @brief Whether @p monster counts as a boss (15) / an elite (6) / armoured (20). */
bool IsBossForGlow(const Monster &monster);
bool IsEliteForGlow(const Monster &monster);
bool IsArmouredForGlow(const Monster &monster);
/** @brief The corpse cooling's strength at @p age seconds (0..1). */
float CorpseCooling(float age);
/** @brief Seconds since the hero was last hurt, or a large number. */
float SecondsSinceHeroHurt();
/** @brief Seconds since the last level-up burst began, or a large number. */
float SecondsSinceLevelUp();

} // namespace devilution::oracool
