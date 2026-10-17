/**
 * @file oracool/woods_caves.cpp
 *
 * The Orcl Act's caves - see woods_caves.h.
 */
#include "oracool/woods_caves.h"

#include <algorithm>
#include <array>
#include <string>

#include <SDL.h>

#include "diablo.h"
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
	GeneratingCaveWithoutWayDown = cave.last;
	CreateDungeon(Game.seed[index], EnterFromBelow ? ENTRY_PREV : ENTRY_MAIN);
	GeneratingCaveWithoutWayDown = false;
	EnterFromBelow = false;
	LoadRndLvlPal(leveltype);
	if (cave.area == OrclArea::Desert)
		TintDesertCavePalette();
	else if (cave.area == OrclArea::Frost)
		TintFrostCavePalette();
	LoadLevelSOLData();
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
	if (fresh)
		InitObjects();
	currlevel = savedLevel;
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
