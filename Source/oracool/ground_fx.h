/**
 * @file oracool/ground_fx.h
 *
 * Two of the second graphics batch (user, 2026-10-11: "let's do 1,2,3,5,7,8"), both drawn on and over the floor:
 *
 *  - Floor marks: a battle leaves its trail. A monster that falls leaves a pool of blood (the undead a smear of bone
 *    dust), a fire explosion a scorch mark, a cold one a patch of frost, an acid one a green stain. Each is generated -
 *    a soft-edged isometric blot with a ragged rim from its own seed, no art - and fades after a minute or two. The
 *    marks blend with the floor already drawn and lit, so a mark in the dark is dark. Floor tiles only: never on a wall,
 *    lava or water. Cleared on a level change.
 *  - Loot beams: an item on the ground worth turning for (rare, unique, set, primal, runeword) stands in a beam of light
 *    in its colour, with a glow on the floor under it, and lights the floor around it in that colour (colour_light.h).
 *
 * Purely visual: own clock (real time, stopped while paused), own xorshift, no game state touched. Each is behind its
 * option; nothing is drawn on an 8-bit surface.
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include "engine/point.hpp"

namespace devilution {
struct Surface;
struct Missile;
struct Monster;
struct Item;
} // namespace devilution

namespace devilution::oracool {

enum class FloorMarkKind : uint8_t {
	Blood,
	BoneDust,
	Scorch,
	Frost,
	Acid,
};

/** @brief The pool's size: past it the oldest mark gives way. */
constexpr size_t MaxFloorMarks = 200;

/** @brief Leaves a mark of @p kind at the tile coordinates (@p x, @p y), @p radius tiles across its half-width. */
void AddFloorMark(float x, float y, FloorMarkKind kind, float radius);

/** @brief A fallen monster's mark: blood, or bone dust for the undead. From MonsterDeath. */
void AddDeathMark(const Monster &monster);

/** @brief An explosion's mark by its colour (fire, cold, acid, blood; lightning a small scorch). From AddMissileExplosion. */
void AddExplosionMark(const Missile &missile);

/** @brief The mark an explosion of colour @p rgb leaves, and its radius; false for none. Exposed for the tests. */
bool ExplosionMarkFor(uint32_t rgb, FloorMarkKind &kind, float &radius);

/** @brief Empties the pool (level change; DrawFloorMarks notices one by itself too). */
void ForgetFloorMarks();

/** @brief How many marks are alive. */
size_t FloorMarkCount();

/** @brief Test hook: moves the marks' clock on by @p seconds, dropping the marks that die. */
void AgeFloorMarks(float seconds);

/**
 * @brief Ages the marks and draws them onto the floor just drawn - after the floor's light passes and before anything
 * stands on it. Then the loot glows on the floor. Same start arguments as the colour pass (the first tile's 64x32 box
 * has its bottom-left at @p firstTileBuffer).
 */
void DrawFloorMarks(const Surface &out, Point firstTile, Point firstTileBuffer);

/** @brief The beams over the ground loot, drawn over the finished world (before the bloom, so they bloom). */
void DrawLootBeams(const Surface &out, Point firstTile, Point firstTileBuffer);

/** @brief How an item on the ground is marked out: its beam's colour (0 = none), height and strength (0..1). */
struct LootLook {
	uint32_t rgb;
	float height;
	float strength;
};
/** @brief The beam @p item earns, rgb 0 for none. */
LootLook LootLookFor(const Item &item);

/** @brief True when the ground item in ActiveItems slot @p item shows (landed, in light, its tile's). */
bool LootShows(const Item &item);

} // namespace devilution::oracool
