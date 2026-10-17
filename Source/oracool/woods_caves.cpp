/**
 * @file oracool/woods_caves.cpp
 *
 * The Orcl Act's caves - see woods_caves.h.
 */
#include "oracool/woods_caves.h"

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <vector>

#include <SDL.h>

#include "diablo.h"
#include "engine/path.h" // IsTileWalkable, IsTileSolid - the captive's spot and the walk to it
#include "engine/palette.h"
#include "engine/random.hpp"
#include "interfac.h"
#include "levels/themes.h"
#include "levels/trigs.h"
#include "objects.h"
#include "oracool/area_level.h"
#include "oracool/stairless.h"
#include "oracool/woods.h"
#include "player.h"
#include "utils/str_cat.hpp"

namespace devilution::oracool {

namespace {

struct CaveDef {
	_setlevels level;
	OrclArea area;  // the area whose floor its mouth stands on
	int woodsFloor; // that floor (0-3)
	int depth;      // 0 for the floor under the mouth
	bool last;      // no way further down
	const char *name;
};

/**
 * @brief The caves (2026-10-14): a one-floor den on the first woods floor, a two-floor cave under the third. The desert's the same
 * (2026-10-16), under its sand holes: the Sand Warrens below the Salt Road, the Buried Halls below the Glass Flats.
 */
constexpr std::array<CaveDef, 10> Caves { {
	{ SL_ORCL_CAVE_1, OrclArea::Woods, 0, 0, true, "The Mossbound Den" },
	{ SL_ORCL_CAVE_2, OrclArea::Woods, 2, 0, false, "The Underroot" },
	{ SL_ORCL_CAVE_3, OrclArea::Woods, 2, 1, true, "The Underroot Depths" },
	{ SL_ORCL_DESERT_CAVE_1, OrclArea::Desert, 0, 0, true, "The Sand Warrens" },
	{ SL_ORCL_DESERT_CAVE_2, OrclArea::Desert, 2, 0, false, "The Buried Halls" },
	{ SL_ORCL_DESERT_CAVE_3, OrclArea::Desert, 2, 1, true, "The Buried Halls Depths" },
	// The Frost Caves' frozen rivers (2026-10-17): one under every floor, a floor of its own, its way down a hole or ice stairs.
	{ SL_ORCL_FROST_RIVER_1, OrclArea::Frost, 0, 0, true, "The Frozen Stream" },
	{ SL_ORCL_FROST_RIVER_2, OrclArea::Frost, 1, 0, true, "Black Ice Run" },
	{ SL_ORCL_FROST_RIVER_3, OrclArea::Frost, 2, 0, true, "The Drowned Hall" },
	{ SL_ORCL_FROST_RIVER_4, OrclArea::Frost, 3, 0, true, "The Still River" },
} };

/** @brief One game's caves: each floor's seed and rung, rolled on its first visit (as the woods' are). */
struct CavesGame {
	std::array<uint32_t, Caves.size()> seed {};
	std::array<uint8_t, Caves.size()> rung {};
};
CavesGame Game;

/** @brief Each cave's seed has passed the landing walk this game (BuildWoodsCaveLevel): its later builds keep it. */
std::array<bool, Caves.size()> GameSettled {};

/** @brief The next cave floor built is entered from the one below: the hero stands by its down stairs. */
bool EnterFromBelow = false;

int IndexOf(_setlevels level)
{
	for (size_t i = 0; i < Caves.size(); i++) {
		if (Caves[i].level == level)
			return static_cast<int>(i);
	}
	return 0;
}

/** @brief The floor DRLG is told it builds: a Caves floor with both stairs and no town warp (9 has one). */
constexpr uint8_t GenerationFloor = 10;

} // namespace

void ResetWoodsCavesForNewGame()
{
	Game = {};
	GameSettled = {};
	EnterFromBelow = false;
}

_setlevels WoodsCaveOnFloor(int floorIndex, OrclArea area)
{
	for (const CaveDef &cave : Caves) {
		if (cave.area == area && cave.woodsFloor == floorIndex && cave.depth == 0)
			return cave.level;
	}
	return SL_NONE;
}

const char *WoodsCaveName(_setlevels level)
{
	return Caves[IndexOf(level)].name;
}

namespace {
void PlaceStillRiverCaptive(uint32_t seed); // after the build, below
int StillRiverReach = 0;            // the tiles its walk from the landing reached, for the tests
int RebuildsForTest = 0;
int FirstReachPercent = 0;            // how many builds the last floor took past its first (CaveRebuildsForTest)

/**
 * @brief Each tile's walking distance from the landing into @p steps (-1 where the hero cannot walk to), over the floor's tiles
 * alone - a barrel in a passage breaks; from every open tile round the landing (the hero may stand in the stairs' pocket), and
 * on the diagonals where one side is open, as the hero walks (the Caves' passages often meet only corner to corner). Returns how
 * many tiles it reached. The level's SOL data must be loaded.
 */
int WalkFromLanding(std::vector<int> &steps)
{
	steps.assign(static_cast<size_t>(MAXDUNX) * MAXDUNY, -1);
	std::vector<Point> queue;
	// The landing's own open tiles: those joined to the hero's tile inside the 7 x 7 round it (all of the 7 x 7 reached across a
	// wall two tiles thick into a pocket behind it, audit 2026-10-17); his tile itself may be the stairs'.
	{
		std::vector<Point> near { ViewPosition };
		std::array<bool, 49> seen {};
		seen[24] = true;
		for (size_t head = 0; head < near.size(); head++) {
			const Point at = near[head];
			if (InDungeonBounds(at) && !IsTileSolid(at)) {
				steps[static_cast<size_t>(at.y) * MAXDUNX + at.x] = 0;
				queue.push_back(at);
			} else if (at != ViewPosition) {
				continue;
			}
			for (const Displacement d : { Displacement { 1, 0 }, Displacement { -1, 0 }, Displacement { 0, 1 }, Displacement { 0, -1 } }) {
				const Point q = at + d;
				const int ix = q.x - ViewPosition.x + 3, iy = q.y - ViewPosition.y + 3;
				if (ix < 0 || iy < 0 || ix > 6 || iy > 6 || seen[static_cast<size_t>(iy * 7 + ix)])
					continue;
				seen[static_cast<size_t>(iy * 7 + ix)] = true;
				near.push_back(q);
			}
		}
	}
	for (size_t head = 0; head < queue.size(); head++) {
		const Point at = queue[head];
		for (const Displacement d : { Displacement { 1, 0 }, Displacement { -1, 0 }, Displacement { 0, 1 }, Displacement { 0, -1 },
		         Displacement { 1, 1 }, Displacement { -1, -1 }, Displacement { 1, -1 }, Displacement { -1, 1 } }) {
			const Point q = at + d;
			if (!InDungeonBounds(q) || IsTileSolid(q))
				continue;
			if (d.deltaX != 0 && d.deltaY != 0 && IsTileSolid(at + Displacement { d.deltaX, 0 }) && IsTileSolid(at + Displacement { 0, d.deltaY }))
				continue;
			int &s = steps[static_cast<size_t>(q.y) * MAXDUNX + q.x];
			if (s >= 0)
				continue;
			s = steps[static_cast<size_t>(at.y) * MAXDUNX + at.x] + 1;
			queue.push_back(q);
		}
	}
	return static_cast<int>(queue.size());
}

/** @brief The floor's open tiles, every one (IsTileSolid's complement in the dungeon's bounds). */
int OpenTiles()
{
	int open = 0;
	for (int y = 0; y < MAXDUNY; y++)
		for (int x = 0; x < MAXDUNX; x++)
			open += IsTileSolid({ x, y }) ? 0 : 1;
	return open;
}

} // namespace

void BuildWoodsCaveLevel(bool fresh)
{
	const int index = IndexOf(setlvlnum);
	const CaveDef &cave = Caves[index];
	if (Game.seed[index] == 0)
		Game.seed[index] = (static_cast<uint32_t>(AdvanceRndSeed()) ^ (SDL_GetTicks() * 2246822519U)) | 1U;
	if (Game.rung[index] == 0) {
		// A rung deeper than the woods floor above, one more per floor down; that floor's rung if it has one, else the hero's.
		int above = WoodsFloorRung(cave.woodsFloor, cave.area);
		if (above == 0) {
			const int heroLevel = MyPlayer != nullptr ? MyPlayer->_pLevel : 1;
			above = std::clamp(heroLevel / 3, 1, RungsPerDifficulty) + cave.woodsFloor;
		}
		Game.rung[index] = static_cast<uint8_t>(std::clamp(above + 1 + cave.depth, 1, RungsPerDifficulty));
	}
	// The DRLG reads currlevel for its floor-keyed rules, and InitObjectGFX and InitObjects for which objects exist: a plain Caves
	// floor for the whole build, as a rift's (rift.cpp BuildRiftLevel). The quests are off on any set level.
	const uint8_t savedLevel = currlevel;
	currlevel = GenerationFloor;
	const lvl_entry entry = EnterFromBelow ? ENTRY_PREV : ENTRY_MAIN;
	GeneratingCaveWithoutWayDown = cave.last;
	CreateDungeon(Game.seed[index], entry);
	LoadLevelSOLData();
	// A landing the floor cannot be walked to from (audit, 2026-10-17: a third of the first builds reached under four fifths of
	// their floor, some only the stairs' corner): the Caves generator checks its floor joins up before laying the lava rivers and
	// the fences, and one-tile lines of them can then wall the stairs' corner off from the rest. Such a floor is built again from
	// the next seed, kept as the floor's own, so a revisit builds the floor that was played.
	// The first build only (audit, 2026-10-17): a revisit restores the level saved on the floor that was played, so it must build
	// that floor again, whatever its walk says from the other stairs. If no try reaches four fifths, the best one is kept.
	std::vector<int> steps;
	RebuildsForTest = 0;
	FirstReachPercent = OpenTiles() > 0 ? WalkFromLanding(steps) * 100 / OpenTiles() : 0;
	if (fresh && !GameSettled[index]) {
		uint32_t seed = Game.seed[index];
		uint32_t bestSeed = seed;
		int bestShare = FirstReachPercent;
		for (int tries = 0; tries < 12 && bestShare < 80; tries++) {
			seed = (seed * 2654435761U + 0x9E3779B9U) | 1U;
			CreateDungeon(seed, entry);
			RebuildsForTest++;
			const int share = OpenTiles() > 0 ? WalkFromLanding(steps) * 100 / OpenTiles() : 0;
			if (share > bestShare) {
				bestShare = share;
				bestSeed = seed;
			}
		}
		if (seed != bestSeed)
			CreateDungeon(bestSeed, entry);
		Game.seed[index] = bestSeed;
	}
	GameSettled[index] = true;
	GeneratingCaveWithoutWayDown = false;
	EnterFromBelow = false;
	LoadRndLvlPal(leveltype);
	if (cave.area == OrclArea::Desert)
		TintDesertCavePalette();
	else if (cave.area == OrclArea::Frost)
		TintFrostCavePalette();
	InitL3Triggers(); // the stairs' triggers; CheckWoodsCaveExits says where they lead
	// No pack on the landing: a floor's stairs keep theirs through Freeupstairs, a set level has none.
	for (int dy = -3; dy <= 3; dy++) {
		for (int dx = -3; dx <= 3; dx++) {
			const Point tile = ViewPosition + Displacement { dx, dy };
			if (InDungeonBounds(tile))
				dFlags[tile.x][tile.y] |= DungeonFlag::Populated;
		}
	}
	// The floor's barrels, chests, shrines and the like, the same on every build; no theme rooms.
	SetRndSeed(Game.seed[index] ^ 0x5EEDU);
	numthemes = 0;
	if (!HeadlessMode)
		InitObjectGFX();
	if (fresh) {
		InitObjects();
		if (setlvlnum == SL_ORCL_FROST_RIVER_4)
			PlaceStillRiverCaptive(Game.seed[index]);
	}
	currlevel = savedLevel;
}

namespace {

/**
 * @brief The Still River's captive (RfA-85): a large chest of three real items, untrapped, marked as the captive, standing in the
 * open far from where the hero arrives - the tile the longest walk from the landing with its eight neighbours walkable and empty, ties
 * broken by a hash of the floor's seed. Placed once, on the floor's first build: the save keeps it.
 */
void PlaceStillRiverCaptive(uint32_t seed)
{
	const auto clear = [](Point p) {
		for (int dy = -1; dy <= 1; dy++)
			for (int dx = -1; dx <= 1; dx++) {
				const Point q = p + Displacement { dx, dy };
				if (!InDungeonBounds(q) || !IsTileWalkable(q) || dObject[q.x][q.y] != 0)
					return false;
			}
		return true;
	};
	// Walking distance from the landing, so the captive is never in a pocket the hero cannot reach.
	std::vector<int> steps;
	StillRiverReach = WalkFromLanding(steps);
	std::optional<Point> best;
	int bestScore = -1;
	for (int y = 2; y < MAXDUNY - 2; y++)
		for (int x = 2; x < MAXDUNX - 2; x++) {
			const Point p { x, y };
			const int walk = steps[static_cast<size_t>(y) * MAXDUNX + x];
			if (walk < 0 || !clear(p))
				continue;
			const int score = walk * 16 + static_cast<int>((seed ^ (static_cast<uint32_t>(x) * 73856093U) ^ (static_cast<uint32_t>(y) * 19349663U)) & 15U);
			if (score > bestScore) {
				bestScore = score;
				best = p;
			}
		}
	if (!best)
		return;
	Object *captive = AddObject(OBJ_CHEST3, *best);
	if (captive == nullptr)
		return;
	captive->_oVar1 = 3;  // three items
	captive->_oVar2 = 1;  // real ones, never only potions and scrolls
	captive->_oVar3 = CaptiveMark;
	captive->_oTrapFlag = false;
}

} // namespace

int StillRiverReachForTest()
{
	return StillRiverReach;
}

int CaveFirstReachPercentForTest()
{
	return FirstReachPercent;
}

int CaveRebuildsForTest()
{
	return RebuildsForTest;
}

int CaveLandingReachPercentForTest()
{
	std::vector<int> steps;
	const int open = OpenTiles();
	return open > 0 ? WalkFromLanding(steps) * 100 / open : 0;
}

Point StillRiverCaptiveForTest()
{
	for (int i = 0; i < ActiveObjectCount; i++) {
		const Object &object = Objects[ActiveObjects[i]];
		if (IsStillRiverCaptive(object._otype, object._oVar3))
			return object.position;
	}
	return {};
}

int WoodsCaveRung()
{
	if (!setlevel || !IsActCaveLevel(setlvlnum))
		return 1;
	const int rung = Game.rung[IndexOf(setlvlnum)];
	return rung == 0 ? 1 : rung;
}

uint32_t WoodsCaveRosterSeed()
{
	return Game.seed[IndexOf(setlvlnum)] ^ 0x0AC7U;
}

void EnterWoodsCaveFromMouth(int floorIndex, OrclArea area)
{
	const _setlevels cave = WoodsCaveOnFloor(floorIndex, area);
	if (cave == SL_NONE || MyPlayer == nullptr)
		return;
	EnterFromBelow = false;
	setlvltype = DTYPE_CAVES; // BEFORE StartNewLvl: the level loads its tileset from this
	StartNewLvl(*MyPlayer, WM_DIABSETLVL, static_cast<int>(cave));
}

bool CheckWoodsCaveExits()
{
	if (MyPlayer == nullptr || !setlevel || !IsActCaveLevel(setlvlnum))
		return false;
	Player &player = *MyPlayer;
	const CaveDef &cave = Caves[IndexOf(setlvlnum)];
	for (int i = 0; i < numtrigs; i++) {
		if (player.position.tile != trigs[i].position)
			continue;
		if (trigs[i]._tmsg == WM_DIABPREVLVL) {
			if (cave.depth == 0) {
				// Out by the mouth (or the sand hole), onto the floor it stands on.
				EnterWoodsAtCaveMouthNext();
				setlvltype = DTYPE_WOODS;
				StartNewLvl(player, WM_DIABSETLVL, static_cast<int>(OutdoorFloorLevel(cave.area, cave.woodsFloor)));
			} else {
				EnterFromBelow = true;
				setlvltype = DTYPE_CAVES;
				StartNewLvl(player, WM_DIABSETLVL, static_cast<int>(cave.level) - 1);
			}
			return true;
		}
		if (trigs[i]._tmsg == WM_DIABNEXTLVL) {
			if (!cave.last) {
				EnterFromBelow = false;
				setlvltype = DTYPE_CAVES;
				StartNewLvl(player, WM_DIABSETLVL, static_cast<int>(cave.level) + 1);
			}
			return true;
		}
		return true; // any other trigger here is this module's to ignore
	}
	return false;
}

const char *WoodsCaveStairsText(bool up)
{
	static std::string text;
	const CaveDef &cave = Caves[IndexOf(setlvlnum)];
	if (up)
		text = StrCat("Up to ", cave.depth == 0 ? WoodsFloorName(OutdoorFloorLevel(cave.area, cave.woodsFloor)) : Caves[IndexOf(setlvlnum) - 1].name);
	else
		text = StrCat("Down to ", cave.last ? cave.name : Caves[IndexOf(setlvlnum) + 1].name);
	return text.c_str();
}

} // namespace devilution::oracool
