/**
 * @file oracool/area_level.h
 *
 * Oracool: alvl, mlvl and ilvl - the three numbers Diablo II keeps separate and this fork did not.
 *
 * User request, 2026-08-19: "item levels-monster levels introduction and relation [...] i dont want
 * to find book of apocalips on Cathedral LVL1 in Normal."
 *
 * ## The three numbers
 *
 *  - **alvl**, AREA level. A property of the PLACE, fixed per floor per difficulty. 24 floors by 4
 *    difficulties is one clean ladder from 1 to 96.
 *  - **mlvl**, MONSTER level. What a monster is worth as a source of loot: the area's level, plus a
 *    little for a champion or a unique.
 *  - **ilvl**, ITEM level. Stamped on every item as it is generated, from the mlvl of whatever
 *    dropped it (or the alvl of the chest/floor/shop it came from). It gates which base items can
 *    appear at all - ItemData::iMinMLvl is the base's qlvl - and how good the roll may be.
 *
 * ## Why mlvl is NOT Monster::level()
 *
 * Monster::level() feeds thirteen call sites that have nothing to do with loot: to-hit, block
 * chance, experience, and some missile damage. Redefining it to the area ladder would be a combat
 * rebalance across all 96 floors wearing a loot change's clothes. So drops ask ItemLevelOf(monster)
 * and combat keeps the curve it has (user, 2026-08-19: "split now, revisit with telemetry").
 *
 * ## The ladder
 *
 * alvl = floor + 24 * difficulty. Cathedral 1-4, Catacombs 5-8, Caves 9-12, Hell 13-16, Nest 17-20,
 * Crypt 21-24, and the same six areas again one difficulty up.
 *
 *   Normal    1-24     Hell      49-72
 *   Nightmare 25-48    Torment   73-96
 *
 * The design target is that Hell/Hell - floors 13-16 of Hell difficulty, alvl 61-64 - is where the
 * last base item becomes available, so every piece of gear in the game is obtainable by then and
 * the eight areas past it are for better rolls rather than for new things.
 *
 * Note what this replaces: ItemsGetCurrlevel() folds Hellfire's Nest back to 9-12 and its Crypt to
 * 14-17, because in Hellfire those are a PARALLEL path to the Cathedral rather than a deeper one.
 * In this fork they are floors 17-24 and they are deeper, so the ladder uses the floor as it is.
 */
#pragma once

#include <cstdint>

#include "levels/gendung.h"

namespace devilution::oracool {

/** @brief Dungeon floors in the game: Cathedral 1-4 through Crypt 21-24. */
constexpr int AreaFloorCount = 24;

/** @brief Floors per area, and the six areas the floors group into. */
constexpr int FloorsPerArea = 4;
constexpr int AreaCount = 6;

/** @brief The top of the ladder: floor 24 of Torment. */
constexpr int MaxAreaLevel = AreaFloorCount * 4;

/**
 * @brief The alvl of @p floor on @p difficulty. Clamped to 1..MaxAreaLevel.
 *
 * Pure arithmetic on purpose - no table to fall out of step with the floor count.
 */
int AreaLevel(int floor, _difficulty difficulty);

/** @brief The alvl of wherever the player is standing right now, quest set-levels included. */
int CurrentAreaLevel();

/** @brief Which of the six areas @p floor belongs to (0 = Cathedral .. 5 = Crypt). */
int AreaIndexOfFloor(int floor);

/** @brief Display name of the area @p floor sits in - "Cathedral", "Crypt", and so on. */
const char *AreaNameOfFloor(int floor);

/**
 * @brief The item level a drop from this floor carries, before any per-monster bonus.
 *
 * Chests, floor spawns, quest rewards and shop stock all use this; monster kills add their own
 * bonus on top (see ItemLevelOfMonster in items.cpp).
 */
inline int CurrentDropItemLevel()
{
	return CurrentAreaLevel();
}

} // namespace devilution::oracool
