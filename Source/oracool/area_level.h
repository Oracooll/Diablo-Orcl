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
 * SIXTEEN rungs a difficulty, sixty-four in all (user, 2026-09-12: "now we should have a total of
 * 4x16=64 area levels", "hell/crypt//torment being the hardest"):
 *
 *   Normal    1-16     Hell      33-48
 *   Nightmare 17-32    Torment   49-64
 *
 * Four areas hold those rungs - Cathedral 1-4, Catacombs 5-8, Caves 9-12, Hell 13-16 - and the other
 * two SHARE them: the Hive (floors 17-20) sits on the Caves' rungs and the Crypt (21-24) on Hell's.
 * They are a side-step rather than a descent ("hive = caves, crypt = hell [...] just an alternative
 * area for the same of diversity"), and their monsters keep Hellfire's own power for the same reason,
 * so what they pay now matches the fight they are. See LadderFloorOf in the .cpp.
 *
 * The deepest rung in the game is Torment's Hell - and its Crypt, which shares it - at 64.
 *
 * Note what this replaces: the ladder ran to 96 and gave the Hive and the Crypt eight rungs of their
 * own per difficulty, which made the two EASIEST areas past Hell the richest in the game. It also
 * moves the "every base item is obtainable" mark: the deepest base needs alvl 51, which now falls in
 * Torment's first rows rather than in Hell's last ones.
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

/**
 * @brief Rungs one difficulty holds: four areas of four floors. The Hive and the Crypt share the
 * Caves' and Hell's rungs rather than adding eight more, so twenty-four floors fit on sixteen rungs.
 */
constexpr int RungsPerDifficulty = FloorsPerArea * 4;

/** @brief The top of the ladder: Torment's Hell and Crypt, the hardest places in the game. */
constexpr int MaxAreaLevel = RungsPerDifficulty * 4;

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
