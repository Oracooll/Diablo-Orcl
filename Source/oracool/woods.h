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
#include <vector>

#include "engine/point.hpp"
#include "levels/gendung.h"

namespace devilution {
struct Surface;
struct Player;
struct MonsterData;
struct Monster;
} // namespace devilution

namespace devilution::oracool {

/**
 * @brief The piece a woods tile is DRAWN with, where that differs from the one that decides its walking (dPiece): on the
 * Mixed look a painted tree's tile is solid, yet shows Tristram's grass under the tree. 0 where they are the same.
 */
extern DVL_API_FOR_TEST uint16_t WoodsLookPiece[MAXDUNX][MAXDUNY];

/**
 * @brief The houses' outsides (woods.cpp CottageShell): a Tristram cottage's town piece over each of a house's tiles, 0 for none;
 * the house it belongs to; and each house's state this frame - 0 gone (the hero is inside), 1 see-through (he is behind it),
 * 2 whole. UpdateWoodsShells sets the states, once a frame.
 */
extern DVL_API_FOR_TEST uint16_t WoodsShell[MAXDUNX][MAXDUNY];
extern DVL_API_FOR_TEST int8_t WoodsShellOwner[MAXDUNX][MAXDUNY];
extern DVL_API_FOR_TEST uint8_t WoodsShellState[2];

/** @brief The cottage's piece drawn on @p tile this frame, or 0. */
inline uint16_t WoodsShellAt(Point tile)
{
	if (leveltype != DTYPE_WOODS)
		return 0;
	const uint16_t shell = WoodsShell[tile.x][tile.y];
	const int owner = WoodsShellOwner[tile.x][tile.y];
	return shell != 0 && owner >= 0 && owner < 2 && WoodsShellState[owner] != 0 ? shell : 0;
}

/** @brief The cottage on @p tile is drawn see-through this frame. */
inline bool WoodsShellSeeThrough(Point tile)
{
	const int owner = WoodsShellOwner[tile.x][tile.y];
	return WoodsShellAt(tile) != 0 && WoodsShellState[owner] == 1;
}

/** @brief The piece the renderer draws for @p tile: a cottage's, else the woods' look piece where it has one, else dPiece. */
inline uint16_t WoodsDrawnPiece(Point tile)
{
	if (leveltype == DTYPE_WOODS) {
		if (const uint16_t shell = WoodsShellAt(tile); shell != 0)
			return shell;
		if (WoodsLookPiece[tile.x][tile.y] != 0)
			return WoodsLookPiece[tile.x][tile.y];
	}
	return dPiece[tile.x][tile.y];
}

/** @brief The cottages' states for this frame, from where the hero stands. Once a frame, before the floor is drawn. */
void UpdateWoodsShells();

/** @brief A whole cottage stands over @p tile, inside its house: what stands there is hidden under its roof. */
bool WoodsShellCovers(Point tile);

/** @brief The Mixed look is on for the floor last built: painted trees and ground details over Tristram's tiles. */
bool WoodsSpritesOverTiles(const Surface &out);

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

/** @brief After a woods floor's items are made: on its first visit, what its houses hold. */
void FinishWoodsLevel(bool fresh);

/** @brief Test hooks: the floor last generated's houses, a house's arch and a tile inside it. */
int WoodsHouseCountForTest();
Point WoodsHouseDoorForTest(int index);
Point WoodsHouseInsideForTest(int index);

/** @brief Woods floor @p floorIndex's rung, 0 until it is first entered in this game. */
int WoodsFloorRung(int floorIndex);

/** @brief The next woods floor built is entered from its cave: the hero stands by the cave's mouth. */
void EnterWoodsAtCaveMouthNext();

/** @brief The cave mouth's tiles are Tristram's own cels, drawn by DrawCell even on the painted look. */
bool WoodsTileUsesCels(Point tile);

/**
 * @brief The hover over the cave mouth at @p tile: its entrance and the text ("Down to The Mossbound Den").
 * @return false off the mouth
 */
bool WoodsCaveMouthHover(Point tile, Point &entrance, const char *&text);

/**
 * @brief The rung (1-16 inside the difficulty's block) the current woods floor's monsters and loot stand on: the hero's level
 * by the Nephalem Rift's rule (one rung per three levels, the user's pick of 2026-09-24), one rung deeper per woods floor, and
 * fixed the first time the floor is entered in a game (D3). 1 off the woods.
 */
int WoodsBandFloor();

/**
 * @brief A monster type the current woods floor may draw: one of the user's Mob Roster (the Woods Levels Designer's tab 2,
 * 2026-10-14), of its rung - or, where fewer than four of them live that deep, the four nearest it (ScaleWoodsMonster makes
 * up the difference). Never a boss or a summon.
 */
bool WoodsAcceptsMonster(const MonsterData &data);

/**
 * @brief The kind the woods' last floor's boss is made of this game: one of the user's four (the Skeleton King, the Butcher,
 * the Reaper, Diablo - Mob Roster notes "use as boss"), picked from the floor's seed, so it is rolled anew each new game and
 * holds still for the rest of it. -1 before the floor has a seed.
 */
int WoodsBossKind();

/** @brief @p monster is the woods' last floor's boss (its kind, on that floor). */
bool IsWoodsBoss(const Monster &monster);

/** @brief The size the woods boss is drawn at, so each stands as tall as Diablo (the user's notes); 0 when @p monster is not him. */
unsigned WoodsBossSizePercent(const Monster &monster);

/** @brief Where the woods boss waits: open ground inside the fence, a few steps in from the way on. */
Point WoodsBossSpot();

/**
 * @brief A woods or cave monster brought up to the floor's rung: a kind whose own floors end above it gains 12% life and
 * damage per floor between (the Hell-depth rungs, where none of the user's kinds lives); the boss is set to the rung from his
 * own floors either way - up or down - then given three times the life and a quarter more damage. Does nothing elsewhere.
 */
void ScaleWoodsMonster(Monster &monster);

/** @brief Some champion (a non-speaking unique row) is made of monster kind @p type: a Dread boss can wear it. */
bool IsChampionKind(int type);

/** @brief The seed of the floor's monster roster: the same on every build, so a revisit's saved monsters keep their types. */
uint32_t WoodsRosterSeed();

/** @brief Where the hero stands on a woods floor he has just entered: the arrival road, or the way on coming back up. */
Point WoodsEntryTile();

/** @brief The woods floor's own name, for the floor banner and the map. */
const char *WoodsFloorName(_setlevels level);

/** @brief Whether the Woods Painted Art option asks for the painted woods; off, a woods floor is built from Tristram's tiles. */
bool WoodsPaintedArtChosen();

/** @brief True while the woods' true-colour ground and trees replace the tileset's drawing on @p out. */
bool WoodsArtActive(const Surface &out);

/**
 * @brief One woods tile's ground: the tile's diamond filled from the woods' textures, sampled where the tile lies in the
 * world, its material blended into its neighbours' along a noisy edge, lit by @p light (0 bright .. 15 black; -1 full brightness), or written
 * at full brightness when @p fullBright (the smooth floor pass darkens it afterwards).
 */
void DrawWoodsGroundTile(const Surface &out, Point tile, Point target, int light, bool fullBright);

/**
 * @brief The roads' markers (user, 2026-10-13: "colour somehow the ground which acts as an entrance ... brighter green
 * tint ... make it clear to me where i need to take my hero"): over the floor of a gate tile, a pulsing bright-green glow -
 * Tristram's road out to the woods, and the woods' road back - or amber on a way on that is not open yet. Nothing on any
 * other tile.
 */
void DrawWoodsGateTint(const Surface &out, Point tile, Point target);

/**
 * @brief The Mixed look's ground, over the tile Tristram's cels just drew: the painted dirt road on and along the road, its
 * edge frayed by the same world-pinned noise as the painted look, and the rest of Tristram's grass graded toward the painted
 * trees' olive. Nothing on another look.
 */
void DrawWoodsMixedGround(const Surface &out, Point tile, Point target, int light, bool fullBright);

/**
 * @brief The Mixed look's floor tile from its cache - Tristram's cels, the grade and the road made once, copied with the light
 * each frame. False where the tile is drawn as it is (the river, the cave mouth, a cottage, a borrowed piece, another look).
 */
bool DrawWoodsMixedFloorCached(const Surface &out, Point tile, Point target, int light, bool fullBright);

/** @brief The Mixed floor's cached pictures dropped (a new floor). */
void ForgetWoodsMixedFloor();

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
/**
 * @brief Test hook (the Woods Levels Designer's exporter): stamp @p which as town pieces, rows of @p w - 0 the cottage, 1 the cave
 * mouth, 2..7 the river's parts (M1, M2, M3, M4, straight, bridge; one cell each). False past the last.
 */
bool WoodsStampForTest(int which, std::vector<uint16_t> &pieces, int &w, int &h);
/** @brief Test hook: -1 the seed decides whether a floor has a river, 0 none, 1 always. */
void SetWoodsRiverForTest(int mode);
/** @brief Test hook: the floor last generated has its river on @p tile. */
bool WoodsRiverAtForTest(Point tile);
/** @brief Test hook: the next floor generated has the cave mouth of woods floor @p floorIndex (-1: none). */
void SetWoodsCaveMouthFloorForTest(int floorIndex);
/** @brief Test hook: stamps the mouth into the floor last generated; its entrance, or {0,0} for none. */
Point StampWoodsCaveMouthForTest();
/** @brief Test hook: sets floor @p floorIndex's rung as its first visit would. */
void SetWoodsRungForTest(int floorIndex, int rung);
/** @brief Test hook: floor @p floorIndex's seed (0: not yet entered). */
void SetWoodsSeedForTest(int floorIndex, uint32_t seed);

} // namespace devilution::oracool
