/**
 * @file oracool/woods.h
 *
 * The Orcl Act's woods (user, 2026-10-13: "introduce 2-4 areas with 4 levels each ... open world grass and woods area as in
 * early Act 1 in Diablo 2 ... Going west of Pepin takes us to the four grass/woods levels"). The plan and its decisions are
 * the "Road to the Orcl Act" page.
 *
 * Four floors, each a SET LEVEL generated from a seed the way the rifts are (decision D1): SL_ORCL_WOODS_1..4, level type
 * DTYPE_WOODS. A woods floor is a dungeon in every rule - monsters, light, exploration - on the town tileset's data (its
 * pieces give the walking and solidity), but nothing of the tileset is drawn: the ground is the woods' true-colour
 * textures (ChatGPT, RfA-62/62b/62c), laid SCREEN-ALIGNED as painted and pinned to the world, and the trees are
 * true-colour sprites (RfA-63) sorted with the figures. A new layout every new game, the same one for the rest of that
 * game (D4).
 *
 * Milestone 1 (this file's first version): floor 1, The Greywood Verge - the road out of Tristram west of Pepin, the
 * generated woods, the way back. No monsters yet; the way on to floor 2 is not open yet.
 */
#pragma once

#include <cstdint>

#include "engine/point.hpp"
#include "levels/gendung.h"

namespace devilution {
struct Surface;
struct Player;
} // namespace devilution

namespace devilution::oracool {

/** @brief Forgets the woods' layouts and their monsters' levels: a new game draws new woods (decisions D3, D4). */
void ResetWoodsForNewGame();

/** @brief The woods generator, CreateDungeon's DTYPE_WOODS arm: the floor setlvlnum names, from @p seed. */
void CreateWoods(uint32_t seed, lvl_entry entry);

/** @brief LoadSetMap's arm for a woods floor: the floor from its seed, its tile data, its palette and its art. */
void BuildWoodsLevel(bool fresh);

/** @brief Where the hero stands in Tristram coming back out of the woods: on the west road, clear of the gate. */
Point WoodsTownReturnTile();

/**
 * @brief The town gate and the woods' edges, CheckTriggers' Orcl arm: in Tristram, the road out west of Pepin loads woods
 * floor 1; on a woods floor, its arrival road leads back. True when a level change was started.
 */
bool CheckWoodsEdges();

/** @brief The woods floor's own name, for the floor banner and the map. */
const char *WoodsFloorName(_setlevels level);

/** @brief True while the woods' true-colour ground and trees replace the tileset's drawing on @p out. */
bool WoodsArtActive(const Surface &out);

/**
 * @brief One woods tile's ground: the tile's diamond filled from the woods' textures, sampled where the tile lies in the
 * world, its material blended into its neighbours' along a noisy edge, lit by @p light (0 bright .. 15 black; -1 full brightness), or written
 * at full brightness when @p fullBright (the smooth floor pass darkens it afterwards).
 */
void DrawWoodsGroundTile(const Surface &out, Point tile, Point target, int light, bool fullBright);

/** @brief What stands on a woods tile, drawn in the figures' order: a tree, or a small thing on the ground. */
void DrawWoodsTileSprite(const Surface &out, Point tile, Point target, int light);

/** @brief Test hook: the ground material of a woods tile (0 grass, 1 dark grass, 2 dry grass, 3 path, 4 forest floor). */
uint8_t WoodsGroundAt(Point tile);
/** @brief Test hook: whether a woods tile holds a tree. */
bool WoodsTreeAt(Point tile);
/** @brief Test hook: the arrival and the way on of the floor last built. */
Point WoodsArrival();
Point WoodsWayOn();
/** @brief Test hook: loads the woods art as BuildWoodsLevel does outside headless mode. */
void LoadWoodsArtForTest();

} // namespace devilution::oracool
