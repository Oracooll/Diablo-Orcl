/**
 * @file stairless.h
 *
 * Oracool: a generated level without stairs, for the single-floor rifts (user, 2026-09-24: "if a
 * rift is a single level make sure it has no stairs assets on the map. they are meaningless.").
 *
 * The DRLGs still PLACE their stairs, so the retry loops, the random stream and the layout are the
 * vanilla ones; each DRLG then hands the stairs' tiles back to what lay under them before the
 * megatiles become dPiece. Every call here is a no-op while GeneratingStairlessLevel is false, so a
 * normal floor is byte-identical.
 */
#pragma once

#include "engine/point.hpp"
#include "utils/attributes.h"

namespace devilution::oracool {

/**
 * @brief True only while a stairless level is being generated (rift.cpp sets it around
 * CreateDungeon). Every normal floor generates with it false.
 */
extern bool GeneratingStairlessLevel;

/**
 * @brief True only while the last floor of an Orcl Act cave is generated (woods_caves.cpp): the Caves DRLG places its up stairs
 * and no down stairs. Every other floor generates with it false.
 */
extern DVL_API_FOR_TEST bool GeneratingCaveWithoutWayDown;

/**
 * @brief Remembers dungeon[][] as it stands. A DRLG calls it right before it places its stairs.
 */
void RememberFloorUnderStairs();

/**
 * @brief Puts back every megatile the stairs changed since RememberFloorUnderStairs and protects
 * it, so the later decoration passes leave it as the stairs would have. A DRLG calls it once the
 * stairs are in and the retry loop has accepted them.
 * @return true when it acted (a stairless level), false on every normal floor
 */
bool TakeStairsBackOut();

/**
 * @brief The nearest tile to @p start that is open floor with open floor all round it, or
 * @p start when none lies within reach. Needs the level's SOL data loaded (LoadLevelSOLData).
 */
Point OpenFloorNear(Point start);

} // namespace devilution::oracool
