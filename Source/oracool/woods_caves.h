/**
 * @file oracool/woods_caves.h
 *
 * The Orcl Act's caves (user, 2026-10-13: "cave entrances which leave to 1-2 underground level with monsters, like the caves in
 * D2 Act 1"). A cave is one or two set levels generated on the Caves tileset from a per-game seed, under a woods floor: its mouth
 * stands in the floor's glade (oracool/woods.h), its first floor's stairs up lead back out to it, a two-floor cave's down stairs
 * to its second floor, and its last floor has none. Its monsters stand a rung deeper than the woods floor above, a rung more per
 * floor down; the rung is fixed at the first visit in a game, as the woods' is.
 */
#pragma once

#include <cstdint>

#include "engine/point.hpp"
#include "levels/gendung.h"

namespace devilution::oracool {

/** @brief A new game: every cave's seed and rung forgotten. */
void ResetWoodsCavesForNewGame();

/** @brief The cave whose mouth stands on woods floor @p floorIndex (0-3), or SL_NONE. */
_setlevels WoodsCaveOnFloor(int floorIndex);

/** @brief The cave's name, for the automap, the hover and the waypoint-less level name. */
const char *WoodsCaveName(_setlevels level);

/** @brief Builds the current cave floor (setlvlnum) - its map, stairs, objects - from this game's seed for it. */
void BuildWoodsCaveLevel(bool fresh);

/** @brief The current cave floor's rung (1-16 inside the difficulty's block), 1 off the caves. */
int WoodsCaveRung();

/** @brief The seed of the current cave floor's monster roster: the same on every build. */
uint32_t WoodsCaveRosterSeed();

/**
 * @brief The hero standing on a cave's stairs: up to the floor above (the woods for a first floor), down to the next.
 * @return true when a level change started, or the hero stands on stairs this module owns (CheckTriggers must not act on them)
 */
bool CheckWoodsCaveExits();

/** @brief Into the cave on woods floor @p floorIndex, from its mouth. */
void EnterWoodsCaveFromMouth(int floorIndex);

/** @brief The hover text for the cave's stairs at @p tile, or nullptr. @p up is set to the stairs' way. */
const char *WoodsCaveStairsText(bool up);

} // namespace devilution::oracool
