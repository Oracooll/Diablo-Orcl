/**
 * @file oracool/woods.cpp
 *
 * The Orcl Act's woods (oracool/woods.h): the generator, the way in and out, and the true-colour ground and trees.
 */
#include "oracool/woods.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <string>
#include <memory>
#include <unordered_map>
#include <vector>

#include <SDL.h>

#include "diablo.h"
#include "engine/palette.h"
#include "engine/render/dun_render.hpp" // RenderTile - the Mixed floor's cache is drawn by it once
#include "engine/random.hpp"
#include "engine/surface.hpp"
#include "init.h" // gbIsHellfire - the cave mouth is Hellfire's town art
#include "interfac.h"
#include "items.h" // CreateRndItem - what the houses hold
#include "levels/gendung.h"
#include "levels/trigs.h"
#include "monstdat.h"
#include "monster.h" // ScaleWoodsMonster, the boss's spot
#include "oracool/area_level.h"
#include "oracool/borrowed_tiles.h" // the Caves' fences on the woods floors
#include "oracool/oracool.h"       // AddGeneratedLevelWaypointSigil, EnsureWaypointGraphicsLoaded
#include "oracool/stairless.h"     // OpenFloorNear - the landing by the cave mouth
#include "oracool/waypoint_menu.h" // FirstWoodsWaypoint
#include "oracool/woods_caves.h"   // the caves under the floors
#include "lighting.h"
#include "options.h"
#include "oracool/colour_light.h"
#include "oracool/scene_fx.h" // ExplorationMemoryLight - the forest past the map's edge
#include "oracool/colour_light.h" // TownIsDark - the wisps and the ring brighter by night
#include "oracool/splats.h"       // the wisps' glow handed to the graphics card with the other lights
#include "objects.h"              // the vanilla objects along the roads
#include "engine/backbuffer_state.hpp" // RedrawComponent - the fairy ring's mana
#include "player.h"
#include "plrmsg.h"
#include "quests.h"
#include "utils/log.hpp"
#include "utils/png.h"

namespace devilution::oracool {

uint16_t WoodsLookPiece[MAXDUNX][MAXDUNY];
uint16_t WoodsShell[MAXDUNX][MAXDUNY];
int8_t WoodsShellOwner[MAXDUNX][MAXDUNY];
uint8_t WoodsShellState[2] = { 2, 2 };

namespace {

// ---------------------------------------------------------------- the per-game state

constexpr int WoodsFloors = SL_LAST_WOODS - SL_FIRST_WOODS + 1;

/** @brief One game's woods: each floor's seed, rolled on its first visit and kept for the rest of the game (D4). */
struct WoodsGame {
	std::array<uint32_t, WoodsFloors> seed {};
	/** @brief Each floor's rung (WoodsBandFloor), 0 until it is first entered. */
	std::array<uint8_t, WoodsFloors> rung {};
};
WoodsGame Game;

int FloorIndex(_setlevels level)
{
	return std::clamp(static_cast<int>(level) - static_cast<int>(SL_FIRST_WOODS), 0, WoodsFloors - 1);
}

// ---------------------------------------------------------------- the map of the floor last built

/** @brief The ground materials, in the order of the textures. */
enum Material : uint8_t {
	Grass,
	GrassDark,
	GrassDry,
	Path,
	Forest,
	MaterialCount,
};

/** @brief The tree pictures (RfA-63), and what each needs. */
enum TreeKind : uint8_t {
	NoTree,
	OakLarge,
	OakSmall,
	PineTall,
	PineSmall,
	DeadLarge,
	DeadSmall,
	TreeKindCount,
};

/**
 * @brief The floor's playable box in world tiles: 50 x 50 megatiles from tile 6 (user, 2026-10-14: "go with 100x100 for now";
 * it was the dungeon's 40 x 40 from 16), with trees all round it. Past the map's edge the forest is drawn mirrored
 * (DrawWoodsBeyondMap), so the six tiles left over are enough.
 */
constexpr int Lo = 6;
constexpr int Hi = 105;
/** @brief The box in megatiles (the woods' own cell grids, not the dungeon's 40 x 40 dungeon[][]). */
constexpr int WoodsCells = (Hi - Lo + 1) / 2;
static_assert(WoodsCells <= MaxPlayAreaCells && Lo % 2 == 0);

uint8_t Ground[MAXDUNX][MAXDUNY];
uint8_t Tree[MAXDUNX][MAXDUNY];
/** @brief A small thing on the ground: a scatter cell + 1 (RfA-62's sheet), 0 for none. */
uint8_t Scatter[MAXDUNX][MAXDUNY];
/** @brief How often each ground detail is laid, as the Orcl Testing Ground counted them (2026-10-14): weights. */
constexpr int ScatterWeight[12] = { 3, 3, 3, 3, 3, 3, 3, 3, 2, 4, 5, 2 };
/** @brief Each tree's small offset from its tile's centre, so the woods do not stand in rows. */
int8_t TreeJitterX[MAXDUNX][MAXDUNY];
int8_t TreeJitterY[MAXDUNX][MAXDUNY];
/**
 * @brief A tree that is there (it blocks) but is not drawn: deep in the forest, hidden behind the trees in front of it. The
 * user's play test, 2026-10-13: by the map's edge every tile was a tree and the frame rate fell; three tiles into solid forest
 * only one tree in four is drawn, and the canopies still meet.
 */
bool TreeHidden[MAXDUNX][MAXDUNY];

Point Arrival { 88, 48 };
/** @brief The glade's middle: its side path's end, the cave mouth's clearing, the fairy ring's place. */
Point GladeCentre {};
/** @brief The next woods floor built is entered from the deeper one: the hero stands at its way on, not its arrival. */
bool EnterAtWayOn = false;
/** @brief ... or from its cave: by the cave's mouth. */
bool EnterAtCaveMouth = false;

/**
 * @brief The cave mouth (oracool/woods_caves.h): Tristram's own way down to the Caves - the dark hole in the rock outcrop by the
 * river, town tiles (12..19, 66..73), copied as the town has them (OracoolPreview.DISABLED_WoodsMixedView prints the block).
 * It replaced Hellfire's Hive mound on 2026-10-14: the mound is a blighted 9 x 6 slab that read as a rectangle on the grass.
 * Rows of 8, y then x; its entrance - the tile the town's caves trigger stands on, (17, 69) - is (5, 3) into it.
 */
constexpr uint16_t MouthStamp[8][8] = {
	{ 223, 25, 230, 231, 226, 227, 219, 220 },
	{ 224, 225, 232, 233, 228, 229, 18, 19 },
	{ 219, 220, 221, 21, 1209, 1210, 28, 29 },
	{ 18, 19, 22, 23, 1211, 1212, 30, 31 },
	{ 223, 25, 1202, 1199, 1205, 1206, 217, 25 },
	{ 224, 225, 1203, 1204, 1207, 1208, 218, 27 },
	{ 230, 231, 222, 25, 230, 231, 223, 25 },
	{ 232, 233, 26, 27, 232, 233, 224, 225 },
};
constexpr int MouthWidth = 8;
constexpr int MouthHeight = 8;
constexpr Displacement MouthEntranceOffset { 5, 3 };
/**
 * @brief The fences (user, 2026-10-14: "outline levels borders with fences and maybe put a fence door to next area. we can also
 * have fenced areas with doors sporadically dispersed among the level as we have in levels 9-12"): the Caves' own fence
 * megatiles, by dungeon cell (the 40 x 40 grid of 2 x 2 tiles), 0 where there is none. Borrowed from the Caves tileset
 * (oracool/borrowed_tiles.h), each over the woods' own ground.
 */
uint8_t FenceCell[WoodsCells][WoodsCells];
/** @brief The minimap's type of each megatile (automap.cpp, WoodsAutomapCell): dungeon[][] is the vanilla 40 x 40. */
uint8_t MapCell[WoodsCells][WoodsCells];
/** @brief The Caves' fence megatiles (drlg_l3.cpp Fence): along x, along y, a post where two meet, the gates in each. */
constexpr uint8_t FenceAlongX[] = { 134, 136 };
constexpr uint8_t FenceAlongY[] = { 135, 137 };
constexpr uint8_t FencePost = 138;
constexpr uint8_t GateAlongX = 146;
constexpr uint8_t GateAlongY = 147;
/** @brief The Caves' gate leaves, closed to open (objects.cpp OperateL3Door): the woods' gates stand open. */
constexpr std::pair<uint16_t, uint16_t> GateOpen[] = { { 530, 537 }, { 533, 540 } };
/** @brief The ring's cells: one in from the dungeon's edge, so the playable box's outermost tiles are the forest beyond it. */
constexpr int RingLo = 1;
constexpr int RingHi = WoodsCells - 2;

/**
 * @brief The river (user, 2026-10-13: "grass, rivers, bridges"): Tristram's own stream, its cells as the town lays them
 * (OracoolPreview.DISABLED_WoodsMixedView traced the west river): a diagonal of two cells a row - M3 M4, then M1 M2 under them,
 * then the pair one cell east - with straight stretches of one cell a row between, and the town's wooden bridge in a straight
 * stretch where a road crosses. Each a 2 x 2 of town pieces: top, right, left, bottom.
 */
enum RiverPart : uint8_t {
	NoRiver,
	RiverM1,
	RiverM2,
	RiverM3,
	RiverM4,
	RiverStraight,
	RiverBridge,
};
constexpr uint16_t RiverPieces[7][4] = {
	{ 0, 0, 0, 0 },
	{ 277, 278, 279, 280 },
	{ 285, 286, 287, 288 },
	{ 281, 282, 283, 284 },
	{ 289, 290, 291, 292 },
	{ 261, 262, 263, 264 },
	{ 309, 310, 311, 312 }, // crossed along its bottom row (311, 312); 309 and 310 are its rail
};
uint8_t RiverCell[WoodsCells][WoodsCells];
/**
 * @brief The houses (user, 2026-10-13: "houses which can be entered and their roof fading away when in. chests inside. stone
 * walls"; then "walk-in building using wall assets from catacombs and gamedrawn roof, fade-away roof"): rooms of the Catacombs'
 * own wall megatiles (drlg_l2.cpp's patterns: 1 a wall on a cell's west edge, 2 on its north edge, 3 floor, 5 an arch in a
 * north-edge wall, 8 / 7 / 9 / 6 the north-west, north-east, south-west and south-east corners), borrowed onto the woods
 * (oracool/borrowed_tiles.h). Cells x0..x1, y0..y1 by dungeon cell, walls on the outer ring of them; the front walls' outer
 * halves stand on the woods' grass. The roof is the game's own, drawn over it (DrawWoodsRoofAfterTile).
 */
struct House {
	int x0, y0, x1, y1; // the ring of wall cells, inclusive
	int doorY;          // the arch, in the east wall (column x1)
};
constexpr int MaxHouses = 2;
std::array<House, MaxHouses> Houses;
int HouseCount = 0;
/** @brief The fenced pens, by cell (their fence ring inclusive) and the tile in their middle - what stands in them (PlanWoodsWorld). */
struct Pen {
	int mx0, my0, x1, y1;
	Point centre;
};
constexpr int MaxPens = 3;
std::array<Pen, MaxPens> Pens;
int PenCount = 0;

/**
 * @brief What a house looks like from outside (user, 2026-10-14: "Game drawn roofs dont look good. We remove them. We do have
 * vanilla roofs in tristram houses. Can we use those?"): Tristram's own hip-roofed cottage, its town tiles (36..43, 64..69) as
 * the town lays them - roof, walls, door and the rock beside it. Drawn in place of the house's tiles while the hero is outside
 * it, see-through while he stands behind it, gone while he is inside, where the Catacombs room under it shows. Its walls stand
 * on the room's ring: the cottage's walls are 38..42 by 64..68, its door on the east face at 66..67. Rows y, then x.
 */
constexpr uint16_t CottageShell[6][8] = {
	{ 230, 231, 476, 477, 218, 218, 484, 485 },
	{ 232, 233, 478, 479, 218, 218, 486, 487 },
	{ 465, 25, 218, 218, 218, 218, 480, 481 },
	{ 466, 467, 218, 218, 218, 218, 482, 483 },
	{ 457, 458, 468, 469, 472, 473, 453, 454 },
	{ 459, 460, 470, 471, 474, 475, 455, 456 },
};
/** @brief The cottage block's place in a house's 5 x 4 cells: two tiles in from the west, two from the north. */
constexpr Displacement CottageShellOffset { 2, 2 };

/** @brief -1: the seed decides whether a floor has a river; 0 or 1: the test hook's answer. */
int RiverForced = -1;

/** @brief The floor being generated has a cave mouth (BuildWoodsLevel sets it from WoodsCaveOnFloor). */
int MouthFloor = -1;
bool MouthOnFloor = false;
Point MouthOrigin {};
Point MouthEntrance {};

bool InRiver(Point tile)
{
	if (tile.x < Lo || tile.y < Lo || tile.x > Hi || tile.y > Hi)
		return false;
	return RiverCell[(tile.x - Lo) / 2][(tile.y - Lo) / 2] != NoRiver;
}

bool InMouth(Point tile)
{
	return MouthOnFloor && tile.x >= MouthOrigin.x && tile.x < MouthOrigin.x + MouthWidth && tile.y >= MouthOrigin.y && tile.y < MouthOrigin.y + MouthHeight;
}

/** @brief The river's town pieces over the floor's tiles. */
void StampRiver()
{
	for (int my = 0; my < WoodsCells; my++) {
		for (int mx = 0; mx < WoodsCells; mx++) {
			const uint8_t part = RiverCell[mx][my];
			if (part == NoRiver)
				continue;
			for (int t = 0; t < 4; t++) {
				const int x = Lo + 2 * mx + (t & 1);
				const int y = Lo + 2 * my + (t >> 1);
				dPiece[x][y] = RiverPieces[part][t];
				WoodsLookPiece[x][y] = 0;
			}
		}
	}
}

/** @brief The outcrop's town pieces over the floor's tiles. */
void StampCaveMouth()
{
	if (!MouthOnFloor)
		return;
	for (int dy = 0; dy < MouthHeight; dy++) {
		for (int dx = 0; dx < MouthWidth; dx++) {
			const Point tile = MouthOrigin + Displacement { dx, dy };
			dPiece[tile.x][tile.y] = MouthStamp[dy][dx];
			WoodsLookPiece[tile.x][tile.y] = 0;
		}
	}
}
Point WayOn { 22, 62 };

// ---------------------------------------------------------------- deterministic noise, independent of the game's stream

uint32_t Hash(uint32_t a, uint32_t b, uint32_t c)
{
	uint32_t h = a * 0x9E3779B1U ^ (b + 0x7F4A7C15U) * 0x85EBCA77U ^ (c + 0x165667B1U) * 0xC2B2AE3DU;
	h ^= h >> 15;
	h *= 0x2C1B3C6DU;
	h ^= h >> 12;
	h *= 0x297A2D39U;
	h ^= h >> 15;
	return h;
}

float Unit(uint32_t h)
{
	return static_cast<float>(h & 0xFFFFFF) / 16777216.0F;
}

/** @brief Smooth value noise over world tiles, 0..1, features about @p scale tiles across. */
float ValueNoise(int x, int y, float scale, uint32_t seed)
{
	const float fx = static_cast<float>(x) / scale;
	const float fy = static_cast<float>(y) / scale;
	const int ix = static_cast<int>(std::floor(fx));
	const int iy = static_cast<int>(std::floor(fy));
	const float tx = fx - static_cast<float>(ix);
	const float ty = fy - static_cast<float>(iy);
	const auto at = [&](int cx, int cy) { return Unit(Hash(static_cast<uint32_t>(cx), static_cast<uint32_t>(cy), seed)); };
	const float sx = tx * tx * (3 - 2 * tx);
	const float sy = ty * ty * (3 - 2 * ty);
	const float top = at(ix, iy) + (at(ix + 1, iy) - at(ix, iy)) * sx;
	const float bottom = at(ix, iy + 1) + (at(ix + 1, iy + 1) - at(ix, iy + 1)) * sx;
	return top + (bottom - top) * sy;
}

bool Inside(Point p)
{
	return p.x >= Lo && p.y >= Lo && p.x <= Hi && p.y <= Hi;
}

// ---------------------------------------------------------------- the generator

/** @brief A piece the town tileset makes solid: every tree tile and everything outside the box is it. Never drawn. */
uint16_t SolidPiece = 0;
/** @brief The four plain grass megatiles of the town tileset (DrlgTPass3 fills the closed Hell entrance with them). */
constexpr int GrassMegatiles = 4;

void FindSolidPiece()
{
	// Solid and missile-blocking but letting light through if the tileset has one, so a trunk does not cast a tile-wide
	// black shadow; any solid piece otherwise.
	uint16_t anySolid = 0;
	for (int i = 1; i < MAXTILES; i++) {
		if (!TileHasAny(i, TileProperties::Solid))
			continue;
		if (anySolid == 0)
			anySolid = static_cast<uint16_t>(i);
		if (TileHasAny(i, TileProperties::BlockMissile) && !TileHasAny(i, TileProperties::BlockLight)) {
			SolidPiece = static_cast<uint16_t>(i);
			return;
		}
	}
	SolidPiece = anySolid;
}

void FillMegatile(int x, int y, int megatile)
{
	const MegaTile mega = pMegaTiles[megatile - 1];
	dPiece[x + 0][y + 0] = SDL_SwapLE16(mega.micro1);
	dPiece[x + 1][y + 0] = SDL_SwapLE16(mega.micro2);
	dPiece[x + 0][y + 1] = SDL_SwapLE16(mega.micro3);
	dPiece[x + 1][y + 1] = SDL_SwapLE16(mega.micro4);
}

/** @brief Clears a round patch of trees, its ground set to @p material where it is not already a path. */
void Clearing(Point centre, int radius, Material material)
{
	for (int dy = -radius; dy <= radius; dy++) {
		for (int dx = -radius; dx <= radius; dx++) {
			const Point p = centre + Displacement { dx, dy };
			if (!Inside(p) || dx * dx + dy * dy > radius * radius)
				continue;
			Tree[p.x][p.y] = NoTree;
			if (Ground[p.x][p.y] != Path)
				Ground[p.x][p.y] = material;
		}
	}
}

/** @brief A wandering path from @p from to @p to, @p width tiles wide, cleared of trees. */
void LayPath(Point from, Point to, int width, uint32_t seed)
{
	float x = static_cast<float>(from.x);
	float y = static_cast<float>(from.y);
	const float tx = static_cast<float>(to.x);
	const float ty = static_cast<float>(to.y);
	for (int step = 0; step < 600; step++) {
		const float dx = tx - x;
		const float dy = ty - y;
		const float dist = std::sqrt(dx * dx + dy * dy);
		if (dist < 1.0F)
			break;
		// Toward the goal, bent by the noise so the path winds.
		const float bend = (ValueNoise(static_cast<int>(x), static_cast<int>(y), 7.0F, seed) - 0.5F) * 2.2F;
		const float ax = dx / dist - dy / dist * bend;
		const float ay = dy / dist + dx / dist * bend;
		const float al = std::sqrt(ax * ax + ay * ay);
		x += ax / al * 0.7F;
		y += ay / al * 0.7F;
		const Point here { static_cast<int>(std::lround(x)), static_cast<int>(std::lround(y)) };
		for (int oy = -width / 2; oy <= width / 2; oy++) {
			for (int ox = -width / 2; ox <= width / 2; ox++) {
				const Point p = here + Displacement { ox, oy };
				if (!Inside(p))
					continue;
				Ground[p.x][p.y] = Path;
				Tree[p.x][p.y] = NoTree;
			}
		}
		// Trees step back a tile beside the path.
		for (int oy = -width / 2 - 1; oy <= width / 2 + 1; oy++) {
			for (int ox = -width / 2 - 1; ox <= width / 2 + 1; ox++) {
				const Point p = here + Displacement { ox, oy };
				if (Inside(p))
					Tree[p.x][p.y] = NoTree;
			}
		}
	}
}

TreeKind PickTree(Material ground, uint32_t h)
{
	(void)ground;
	// The mix the user set on the Orcl Testing Ground (2026-10-14): large oak 10, tall pine 12, small oak 12, small pine 2, large
	// dead 12, small dead 11 - in the groves and in the open alike.
	const float u = Unit(h);
	return u < 0.169F ? OakLarge : u < 0.373F ? PineTall : u < 0.576F ? OakSmall : u < 0.610F ? PineSmall : u < 0.814F ? DeadLarge : DeadSmall;
}

/** @brief Every walkable tile reachable from the arrival; the rest of the open ground becomes thicket. */
void SealUnreachable()
{
	static bool seen[MAXDUNX][MAXDUNY];
	std::memset(seen, 0, sizeof(seen));
	std::vector<Point> stack { Arrival };
	seen[Arrival.x][Arrival.y] = true;
	while (!stack.empty()) {
		const Point p = stack.back();
		stack.pop_back();
		for (const Displacement d : { Displacement { 1, 0 }, Displacement { -1, 0 }, Displacement { 0, 1 }, Displacement { 0, -1 } }) {
			const Point n = p + d;
			if (!Inside(n) || seen[n.x][n.y] || Tree[n.x][n.y] != NoTree)
				continue;
			seen[n.x][n.y] = true;
			stack.push_back(n);
		}
	}
	for (int y = Lo; y <= Hi; y++) {
		for (int x = Lo; x <= Hi; x++) {
			if (Tree[x][y] == NoTree && !seen[x][y])
				Tree[x][y] = PineSmall;
		}
	}
}

/** @brief A dungeon cell's four tiles cleared for a fence: no tree, no ground detail. */
void ClearCell(int mx, int my)
{
	for (int t = 0; t < 4; t++) {
		const int x = Lo + 2 * mx + (t & 1);
		const int y = Lo + 2 * my + (t >> 1);
		Tree[x][y] = NoTree;
		Scatter[x][y] = 0;
	}
}

bool CellHasPath(int mx, int my)
{
	for (int t = 0; t < 4; t++) {
		if (Ground[Lo + 2 * mx + (t & 1)][Lo + 2 * my + (t >> 1)] == Path)
			return true;
	}
	return false;
}

bool CellOpen(int mx, int my)
{
	for (int dy = -1; dy <= 1; dy++)
		for (int dx = -1; dx <= 1; dx++)
			if (mx + dx >= 0 && my + dy >= 0 && mx + dx < WoodsCells && my + dy < WoodsCells && RiverCell[mx + dx][my + dy] != NoRiver)
				return false;
	for (int t = 0; t < 4; t++) {
		const int x = Lo + 2 * mx + (t & 1);
		const int y = Lo + 2 * my + (t >> 1);
		if (Tree[x][y] != NoTree || Ground[x][y] == Path || Ground[x][y] == Forest)
			return false;
	}
	return true;
}

/** @brief A road on any of the cell's four tiles. */
bool PathInCell(int mx, int my)
{
	if (mx < 0 || my < 0 || mx >= WoodsCells || my >= WoodsCells)
		return false;
	return CellHasPath(mx, my);
}

/**
 * @brief The river from the floor's top edge to its bottom (cell rows 0-39), drifting east as it goes: two diagonal rows,
 * then straight stretches. A straight stretch wherever the next diagonal cells hold a road, bridged where the road crosses.
 * Kept clear of both roads' ends, the glade's cave mouth and the ring's corners; tried again from another column when it
 * cannot be, and left out after 24 tries. Trees and ground details cleared off it and its banks.
 */
void PlanRiver(uint32_t seed)
{
	std::memset(RiverCell, 0, sizeof(RiverCell));
	const bool wanted = RiverForced >= 0 ? RiverForced != 0 : Hash(seed, 0x51BE, 3) % 10 < 7;
	if (!wanted)
		return;
	const int arrivalCell = (Arrival.x - Lo) / 2;
	const int wayOnCell = (WayOn.x - Lo) / 2;
	bool accepted = false;
	for (int attempt = 0; attempt < 24; attempt++) {
		std::memset(RiverCell, 0, sizeof(RiverCell));
		int c = wayOnCell + 5 + static_cast<int>(Hash(seed, static_cast<uint32_t>(attempt), 0x51BE) % 8U);
		bool ok = true;
		bool diagonalNext = true;
		int j = 0;
		while (j < WoodsCells && ok) {
			if (diagonalNext) {
				if (PathInCell(c, j) || PathInCell(c + 1, j) || PathInCell(c, j + 1) || PathInCell(c + 1, j + 1)) {
					ok = false; // a road on a diagonal: another column
					break;
				}
				RiverCell[c][j] = RiverM3;
				RiverCell[c + 1][j] = RiverM4;
				if (j + 1 < WoodsCells) {
					RiverCell[c][j + 1] = RiverM1;
					RiverCell[c + 1][j + 1] = RiverM2;
				}
				j += 2;
				diagonalNext = false;
				continue;
			}
			// After a diagonal: straight down column c + 1, or the next diagonal one cell east.
			const bool roadAhead = PathInCell(c + 1, j) || PathInCell(c + 2, j) || PathInCell(c + 1, j + 1) || PathInCell(c + 2, j + 1);
			const bool straight = roadAhead || Unit(Hash(seed ^ 0x57A1U, static_cast<uint32_t>(j), static_cast<uint32_t>(attempt))) < 0.45F;
			if (straight) {
				RiverCell[c + 1][j] = PathInCell(c + 1, j) ? RiverBridge : RiverStraight;
				j++;
			} else {
				c++;
				diagonalNext = true;
			}
			ok = c + 2 < arrivalCell - 3;
		}
		if (ok) {
			// Off the mouth (and a cell round it), never through a road's end.
			for (int my = 0; my < WoodsCells && ok; my++) {
				for (int mx = 0; mx < WoodsCells && ok; mx++) {
					if (RiverCell[mx][my] == NoRiver)
						continue;
					const Point centre { Lo + 2 * mx + 1, Lo + 2 * my + 1 };
					if (MouthOnFloor && centre.x >= MouthOrigin.x - 3 && centre.x < MouthOrigin.x + MouthWidth + 3 && centre.y >= MouthOrigin.y - 3
					    && centre.y < MouthOrigin.y + MouthHeight + 3)
						ok = false;
					if (centre.WalkingDistance(Arrival) < 8 || centre.WalkingDistance(WayOn) < 8)
						ok = false;
				}
			}
		}
		if (ok) {
			accepted = true;
			break;
		}
	}
	if (!accepted) {
		std::memset(RiverCell, 0, sizeof(RiverCell)); // no try fitted: this floor has no river
		return;
	}
	// Its banks: a tile either side of each river cell kept open.
	for (int my = 0; my < WoodsCells; my++) {
		for (int mx = 0; mx < WoodsCells; mx++) {
			if (RiverCell[mx][my] == NoRiver)
				continue;
			for (int y = Lo + 2 * my; y < Lo + 2 + 2 * my; y++) {
				for (int x = Lo - 1 + 2 * mx; x < Lo + 3 + 2 * mx; x++) {
					if (InDungeonBounds({ x, y })) {
						Tree[x][y] = NoTree;
						Scatter[x][y] = 0;
					}
				}
			}
		}
	}
}

/** @brief Ground a house can be built on: no road, grove or river on or beside it; trees may stand there, they are cleared. */
bool CellBuildable(int mx, int my)
{
	if (mx < 0 || my < 0 || mx >= WoodsCells || my >= WoodsCells)
		return false;
	for (int dy = -1; dy <= 1; dy++)
		for (int dx = -1; dx <= 1; dx++)
			if (mx + dx >= 0 && my + dy >= 0 && mx + dx < WoodsCells && my + dy < WoodsCells && RiverCell[mx + dx][my + dy] != NoRiver)
				return false;
	for (int t = 0; t < 4; t++) {
		const int x = Lo + 2 * mx + (t & 1);
		const int y = Lo + 2 * my + (t >> 1);
		if (Ground[x][y] == Path || Ground[x][y] == Forest)
			return false;
	}
	return true;
}

/**
 * @brief One or two houses on open ground, away from the roads, the glade, the mouth, the waypoint and the pens: four or five
 * cells by four, an arch in the south wall, open ground in front of it.
 */
void PlanHouses(uint32_t seed)
{
	HouseCount = 0;
	const Point keepAway[] = { Arrival, WayOn, MouthOnFloor ? MouthEntrance : Arrival, Arrival + Displacement { -2, -2 } };
	const int want = 1 + static_cast<int>(Hash(seed, 9, 9) % 2);
	// Every position, from a seeded start: random spots seldom missed both roads, the groves and the river at once.
	constexpr int W = 5; // the cottage's block, in cells
	constexpr int D = 4;
	const int positions = (WoodsCells - 12) * (WoodsCells - 12);
	const int start = static_cast<int>(Hash(seed, 0x40E, 1) % static_cast<uint32_t>(positions));
	for (int k = 0; k < positions && HouseCount < want; k++) {
		const int at = (start + k * 7919) % positions; // a stride prime to the count: every position once, scattered
		const int bx = 4 + at % (WoodsCells - 12);
		const int by = 4 + at / (WoodsCells - 12);
		if (bx + W + 2 >= WoodsCells - 3 || by + D + 1 >= WoodsCells - 3)
			continue;
		bool fits = true;
		// The block, a cell round it, and one more in front of the east door: no road, grove, river or fence there.
		for (int my = by - 1; my <= by + D && fits; my++)
			for (int mx = bx - 1; mx <= bx + W + 1 && fits; mx++)
				fits = CellBuildable(mx, my) && FenceCell[mx][my] == 0;
		const Point centre { Lo + 2 * bx + W, Lo + 2 * by + D };
		for (const Point p : keepAway)
			fits = fits && centre.WalkingDistance(p) > 10;
		if (MouthOnFloor)
			fits = fits && std::abs(centre.x - (MouthOrigin.x + MouthWidth / 2)) + std::abs(centre.y - (MouthOrigin.y + MouthHeight / 2)) > 14;
		for (int i = 0; i < HouseCount && fits; i++)
			fits = std::abs(Houses[i].x0 - bx) > W + 3 || std::abs(Houses[i].y0 - by) > D + 3;
		if (!fits)
			continue;
		// The ring under the cottage's walls (38..42 by 64..68 of its block 34..43 by 62..69): cells 2..4 by 1..3 of the block.
		Houses[HouseCount++] = { bx + 2, by + 1, bx + 4, by + 3, by + 2 };
		for (int my = by - 1; my <= by + D; my++)
			for (int mx = bx - 1; mx <= bx + W + 1; mx++)
				if (mx >= 0 && my >= 0 && mx < WoodsCells && my < WoodsCells)
					ClearCell(mx, my);
	}
}

/** @brief The Catacombs megatile of a house's cell, or 0 off it. @p outside is set for the front walls' cells. */
int HouseMegatile(const House &house, int mx, int my, bool &outside)
{
	outside = false;
	if (mx < house.x0 || mx > house.x1 || my < house.y0 || my > house.y1)
		return 0;
	const bool west = mx == house.x0;
	const bool east = mx == house.x1;
	const bool north = my == house.y0;
	const bool south = my == house.y1;
	outside = east || south;
	if (north && west)
		return 8;
	if (north && east)
		return 7;
	if (south && west)
		return 9;
	if (south && east)
		return 6;
	if (east)
		return my == house.doorY ? 4 : 1; // the arch where the cottage's door is
	if (west)
		return 1;
	if (north || south)
		return 2;
	return 3;
}

/**
 * @brief The fence ring round the floor with a gate where each road crosses it, and two or three fenced pens with a gate each
 * on open grass, away from the roads, the glade, the cave mouth and the waypoint.
 */
void PlanFences(uint32_t seed)
{
	std::memset(FenceCell, 0, sizeof(FenceCell));
	const auto pick = [seed](const uint8_t(&from)[2], int mx, int my) { return from[Hash(static_cast<uint32_t>(mx), static_cast<uint32_t>(my), seed ^ 0xFE7CU) & 1]; };
	for (int m = RingLo; m <= RingHi; m++) {
		FenceCell[m][RingLo] = pick(FenceAlongX, m, RingLo);
		FenceCell[m][RingHi] = pick(FenceAlongX, m, RingHi);
		FenceCell[RingLo][m] = pick(FenceAlongY, RingLo, m);
		FenceCell[RingHi][m] = pick(FenceAlongY, RingHi, m);
	}
	for (const int a : { RingLo, RingHi })
		for (const int b : { RingLo, RingHi })
			FenceCell[a][b] = FencePost;
	// Where a road crosses the ring: the gate on the road's middle cell, the ring open where the road's other tiles run.
	const auto gateAt = [](int mx, int my, bool alongY) {
		for (int d = -2; d <= 2; d++) {
			const int cx = alongY ? mx : mx + d;
			const int cy = alongY ? my + d : my;
			if (cx < 0 || cy < 0 || cx >= WoodsCells || cy >= WoodsCells)
				continue;
			if (d != 0 && CellHasPath(cx, cy))
				FenceCell[cx][cy] = 0;
		}
		FenceCell[mx][my] = alongY ? GateAlongY : GateAlongX;
	};
	gateAt(RingHi, (Arrival.y - Lo) / 2, true);
	gateAt(RingLo, (WayOn.y - Lo) / 2, true);
	for (int my = 0; my < WoodsCells; my++)
		for (int mx = 0; mx < WoodsCells; mx++) {
			if (RiverCell[mx][my] != NoRiver)
				FenceCell[mx][my] = 0; // the river runs under the ring
			if (FenceCell[mx][my] != 0)
				ClearCell(mx, my);
		}

	// The pens.
	const Point keepAway[] = { Arrival, WayOn, MouthOnFloor ? MouthEntrance : Arrival, Arrival + Displacement { -2, -2 } };
	int pens = 0;
	PenCount = 0;
	const int want = 2 + static_cast<int>(Hash(seed, 7, 7) % 2);
	for (int attempt = 0; attempt < 60 && pens < want; attempt++) {
		const uint32_t h = Hash(seed, static_cast<uint32_t>(attempt), 0xBE2U);
		const int w = 3 + static_cast<int>(h % 2);
		const int d = 3 + static_cast<int>((h >> 3) % 2);
		const int mx0 = 4 + static_cast<int>((h >> 6) % static_cast<uint32_t>(WoodsCells - 8 - w));
		const int my0 = 4 + static_cast<int>((h >> 14) % static_cast<uint32_t>(WoodsCells - 8 - d));
		bool fits = true;
		for (int my = my0 - 1; my <= my0 + d && fits; my++)
			for (int mx = mx0 - 1; mx <= mx0 + w && fits; mx++)
				fits = CellOpen(mx, my) && FenceCell[mx][my] == 0;
		const Point centre { Lo + 2 * mx0 + w, Lo + 2 * my0 + d };
		for (const Point p : keepAway)
			fits = fits && centre.WalkingDistance(p) > 12;
		if (MouthOnFloor)
			fits = fits && std::abs(centre.x - (MouthOrigin.x + MouthWidth / 2)) + std::abs(centre.y - (MouthOrigin.y + MouthHeight / 2)) > 16;
		if (!fits)
			continue;
		const int x1 = mx0 + w - 1;
		const int y1 = my0 + d - 1;
		for (int mx = mx0; mx <= x1; mx++) {
			FenceCell[mx][my0] = pick(FenceAlongX, mx, my0);
			FenceCell[mx][y1] = pick(FenceAlongX, mx, y1);
		}
		for (int my = my0; my <= y1; my++) {
			FenceCell[mx0][my] = pick(FenceAlongY, mx0, my);
			FenceCell[x1][my] = pick(FenceAlongY, x1, my);
		}
		for (const int a : { mx0, x1 })
			for (const int b : { my0, y1 })
				FenceCell[a][b] = FencePost;
		// One gate, mid-side, on a side picked by the seed.
		switch ((h >> 20) % 4) {
		case 0: FenceCell[mx0 + w / 2][my0] = GateAlongX; break;
		case 1: FenceCell[mx0 + w / 2][y1] = GateAlongX; break;
		case 2: FenceCell[mx0][my0 + d / 2] = GateAlongY; break;
		default: FenceCell[x1][my0 + d / 2] = GateAlongY; break;
		}
		for (int my = my0; my <= y1; my++)
			for (int mx = mx0; mx <= x1; mx++)
				ClearCell(mx, my);
		if (PenCount < MaxPens)
			Pens[PenCount++] = { mx0, my0, x1, y1, centre };
		pens++;
	}
	PlanHouses(seed);
}

/** @brief The planned houses laid over the built floor: each cell the Catacombs' pieces, grass under the front walls' outer halves. */
void LayHouses()
{
	std::memset(WoodsShell, 0, sizeof(WoodsShell));
	std::memset(WoodsShellOwner, -1, sizeof(WoodsShellOwner));
	for (int i = 0; i < HouseCount; i++) {
		const House &house = Houses[i];
		for (int my = house.y0; my <= house.y1; my++) {
			for (int mx = house.x0; mx <= house.x1; mx++) {
				bool outside = false;
				const int mega = HouseMegatile(house, mx, my, outside);
				uint16_t pieces[4];
				if (mega == 0 || !BorrowedMegatilePieces(DTYPE_CATACOMBS, mega, pieces))
					continue;
				for (int t = 0; t < 4; t++) {
					const int x = Lo + 2 * mx + (t & 1);
					const int y = Lo + 2 * my + (t >> 1);
					const uint16_t id = BorrowPiece(DTYPE_CATACOMBS, pieces[t]);
					if (id == 0)
						continue;
					// A front wall's open half is outside the house: the woods' ground. Inside, the Catacombs' stone floor.
					if (outside && !TileHasAny(id, TileProperties::Solid) && WoodsLookPiece[x][y] == 0)
						WoodsLookPiece[x][y] = dPiece[x][y];
					else if (!outside)
						WoodsLookPiece[x][y] = 0;
					dPiece[x][y] = id;
				}
			}
		}
		// The cottage over it: its block from two tiles west and north of the ring's west and north walls.
		const Point origin { Lo + 2 * (house.x0 - 2) + CottageShellOffset.deltaX, Lo + 2 * (house.y0 - 1) + CottageShellOffset.deltaY };
		for (int ry = 0; ry < 6; ry++) {
			for (int rx = 0; rx < 8; rx++) {
				const Point tile = origin + Displacement { rx, ry };
				if (!InDungeonBounds(tile))
					continue;
				WoodsShell[tile.x][tile.y] = CottageShell[ry][rx];
				WoodsShellOwner[tile.x][tile.y] = static_cast<int8_t>(i);
			}
		}
	}
}

/**
 * @brief The planned fences laid over the built floor: each tile the Caves' fence piece for walking and its upright, the woods'
 * own ground for its floor (WoodsLookPiece), a gate's leaf swapped for the open one.
 */
void LayFences()
{
	for (int my = 0; my < WoodsCells; my++) {
		for (int mx = 0; mx < WoodsCells; mx++) {
			const uint8_t mega = FenceCell[mx][my];
			uint16_t pieces[4];
			if (mega == 0 || !BorrowedMegatilePieces(DTYPE_CAVES, mega, pieces))
				continue;
			for (int t = 0; t < 4; t++) {
				const int x = Lo + 2 * mx + (t & 1);
				const int y = Lo + 2 * my + (t >> 1);
				uint16_t native = pieces[t];
				for (const auto &[closed, open] : GateOpen) {
					if (native == closed)
						native = open;
				}
				const uint16_t id = BorrowPiece(DTYPE_CAVES, native);
				if (id == 0)
					continue;
				if (WoodsLookPiece[x][y] == 0)
					WoodsLookPiece[x][y] = dPiece[x][y]; // the ground under the fence
				dPiece[x][y] = id;
			}
		}
	}
}

/** @brief Fills TreeHidden: a tree three tiles or more from any open tile keeps only the 2 x 2 lattice's one tree in four. */
void HideDeepForest()
{
	// Distance in tiles (8-neighbour) to the nearest tile without a tree, by two sweeps.
	static uint8_t depth[MAXDUNX][MAXDUNY];
	for (int y = 0; y < MAXDUNY; y++)
		for (int x = 0; x < MAXDUNX; x++)
			depth[x][y] = Tree[x][y] == NoTree ? 0 : 255;
	for (int y = 0; y < MAXDUNY; y++)
		for (int x = 0; x < MAXDUNX; x++)
			for (const Displacement d : { Displacement { -1, 0 }, Displacement { 0, -1 }, Displacement { -1, -1 }, Displacement { 1, -1 } }) {
				const Point n = Point { x, y } + d;
				if (InDungeonBounds(n))
					depth[x][y] = static_cast<uint8_t>(std::min<int>(depth[x][y], depth[n.x][n.y] + 1));
			}
	for (int y = MAXDUNY - 1; y >= 0; y--)
		for (int x = MAXDUNX - 1; x >= 0; x--)
			for (const Displacement d : { Displacement { 1, 0 }, Displacement { 0, 1 }, Displacement { 1, 1 }, Displacement { -1, 1 } }) {
				const Point n = Point { x, y } + d;
				if (InDungeonBounds(n))
					depth[x][y] = static_cast<uint8_t>(std::min<int>(depth[x][y], depth[n.x][n.y] + 1));
			}
	for (int y = 0; y < MAXDUNY; y++)
		for (int x = 0; x < MAXDUNX; x++)
			// Three tiles in, one tree in four (the 2 x 2 lattice); seven in, one in nine (the 3 x 3). At the Testing Ground's 29-35 %
			// (v1.201.019) the canopies still meet; one in two drew hundreds of them by any edge (the play test of 2026-10-14).
			TreeHidden[x][y] = (depth[x][y] >= 7 && (x % 3 != 0 || y % 3 != 0)) || (depth[x][y] >= 3 && ((x | y) & 1) != 0);
}

void Generate(uint32_t seed)
{
	std::memset(Ground, Grass, sizeof(Ground));
	std::memset(Tree, NoTree, sizeof(Tree));
	std::memset(Scatter, 0, sizeof(Scatter));
	std::memset(TreeJitterX, 0, sizeof(TreeJitterX));
	std::memset(TreeJitterY, 0, sizeof(TreeJitterY));

	// The arrival on the town's side of the floor (east on screen: high x, low y), the way on at the far side (west on
	// screen: low x, high y), each at a seeded spot along its edge.
	const int span = Hi - Lo;
	Arrival = { Hi - 4, Lo + span / 4 + static_cast<int>(Unit(Hash(seed, 1, 0)) * static_cast<float>(span) / 2) };
	WayOn = { Lo + 4, Lo + span / 4 + static_cast<int>(Unit(Hash(seed, 2, 0)) * static_cast<float>(span) / 2) };

	for (int y = 0; y < MAXDUNY; y++) {
		for (int x = 0; x < MAXDUNX; x++) {
			const Point p { x, y };
			if (!Inside(p)) {
				// The wood beyond the map: every tile a tree, of every kind, so its edge is a forest and not a fence.
				Tree[x][y] = PickTree(Forest, Hash(static_cast<uint32_t>(x), static_cast<uint32_t>(y), seed ^ 0x0E7EU));
				Ground[x][y] = Forest;
				continue;
			}
			// Moisture picks the grass; groves are their own noise.
			const float moist = ValueNoise(x, y, 11.0F, seed ^ 0x3331U);
			// The grasses as the user shared them on the Testing Ground (53 grass, 44 dark, 15 dry): about half, four in ten, one in ten.
			Ground[x][y] = moist > 0.56F ? GrassDark : moist < 0.30F ? GrassDry : Grass;
			const float grove = ValueNoise(x, y, 8.0F, seed ^ 0x6A7EU);
			const int edge = std::min({ x - Lo, y - Lo, Hi - x, Hi - y });
			// Thinner than at first (user, 2026-10-14: "reduce number of trees on woods levels, and outline levels borders
			// with fences"): a band of one to three tiles inside the fence, the forest proper beyond it.
			const float borderDepth = 1.0F + 2.0F * ValueNoise(x, y, 5.0F, seed ^ 0xB0DEU);
			const uint32_t h = Hash(static_cast<uint32_t>(x), static_cast<uint32_t>(y), seed ^ 0x7EE5U);
			if (static_cast<float>(edge) < borderDepth) {
				Ground[x][y] = Forest;
				Tree[x][y] = PickTree(Forest, h);
			} else if (grove > 0.64F) {
				Ground[x][y] = Forest;
				if (Unit(h) < 0.17F) // a grove you can see into (the play test: at 0.42 the canopies were one carpet; 0.22 then)
					Tree[x][y] = PickTree(Forest, h >> 3);
			} else if (Unit(h) < 0.015F) {
				Tree[x][y] = PickTree(static_cast<Material>(Ground[x][y]), h >> 3); // a lone tree in the grass
			}
		}
	}

	LayPath(Arrival, WayOn, 3, seed ^ 0x9A7EU);
	// A side path to a clearing, so the floor is more than one road.
	const Point glade { Lo + span / 2 + static_cast<int>((Unit(Hash(seed, 3, 0)) - 0.5F) * 20), Lo + span / 2 + static_cast<int>((Unit(Hash(seed, 4, 0)) - 0.5F) * 30) };
	Clearing(glade, 6, Grass);
	GladeCentre = glade;
	LayPath(glade, { (Arrival.x + WayOn.x) / 2, (Arrival.y + WayOn.y) / 2 }, 2, seed ^ 0x51DEU);
	// The cave mouth on the glade's far side, its entrance facing into the glade; trees and ground details kept off it.
	MouthOnFloor = MouthFloor >= 0;
	if (MouthOnFloor) {
		MouthOrigin = glade + Displacement { -9, -4 }; // the entrance (5, 3) in, facing east into the glade
		MouthEntrance = MouthOrigin + MouthEntranceOffset;
		for (int y = MouthOrigin.y - 2; y < MouthOrigin.y + MouthHeight + 2; y++) {
			for (int x = MouthOrigin.x - 2; x < MouthOrigin.x + MouthWidth + 3; x++) {
				if (!Inside({ x, y }))
					continue;
				Tree[x][y] = NoTree;
				if (Ground[x][y] == Forest)
					Ground[x][y] = Grass;
			}
		}
	}
	Clearing(Arrival, 4, Path);
	Clearing(WayOn, 4, Path);
	// The roads off the map's edge: back toward Tristram and on into the woods.
	for (int x = Arrival.x; x <= Hi; x++)
		for (int y = Arrival.y - 1; y <= Arrival.y + 1; y++) {
			Ground[x][y] = Path;
			Tree[x][y] = NoTree;
		}
	for (int x = Lo; x <= WayOn.x; x++)
		for (int y = WayOn.y - 1; y <= WayOn.y + 1; y++) {
			Ground[x][y] = Path;
			Tree[x][y] = NoTree;
		}
	SealUnreachable();

	for (int y = 0; y < MAXDUNY; y++) {
		for (int x = 0; x < MAXDUNX; x++) {
			const uint32_t h = Hash(static_cast<uint32_t>(x), static_cast<uint32_t>(y), seed ^ 0x5CA7U);
			if (Tree[x][y] != NoTree) {
				TreeJitterX[x][y] = static_cast<int8_t>(static_cast<int>(h & 15) - 8);
				TreeJitterY[x][y] = static_cast<int8_t>(static_cast<int>((h >> 4) & 7) - 4);
			} else if (Inside({ x, y }) && Ground[x][y] != Path && Unit(h >> 8) < 0.07F) {
				// Weighted as the Testing Ground counted them (ScatterWeight).
				int total = 0;
				for (const int w : ScatterWeight)
					total += w;
				int pick = static_cast<int>((h >> 16) % static_cast<uint32_t>(total));
				int cell = 0;
				while (cell < 11 && pick >= ScatterWeight[cell])
					pick -= ScatterWeight[cell++];
				Scatter[x][y] = static_cast<uint8_t>(cell + 1);
			}
		}
	}
	// Each road's end past the fence ring kept clear of trees: the forest beyond the ring stands in front of it on screen and
	// hid the gate and the way out (preview, 2026-10-14).
	for (const Point end : { Point { Hi, Arrival.y }, Point { Lo, WayOn.y } }) {
		for (int dy = -6; dy <= 6; dy++) {
			for (int dx = -6; dx <= 6; dx++) {
				const Point p = end + Displacement { dx, dy };
				if (InDungeonBounds(p) && dx * dx + dy * dy <= 36)
					Tree[p.x][p.y] = NoTree;
			}
		}
	}
	PlanRiver(seed);
	PlanFences(seed);
	HideDeepForest();
}

// ---------------------------------------------------------------- the art

struct Picture {
	int w = 0;
	int h = 0;
	std::vector<uint32_t> px; // 0x00RRGGBB, with the top byte 0xFF where opaque in a sprite
	/** @brief A sprite's opaque columns per row, [first, last); empty rows have first == last. */
	std::vector<int16_t> spanFirst;
	std::vector<int16_t> spanLast;
	int footX = 0;
	int footY = 0;
};

constexpr const char *GroundFiles[MaterialCount] = {
	"levels\\woods\\grass_a.png",
	"levels\\woods\\grass_b.png",
	"levels\\woods\\grass_dry.png",
	"levels\\woods\\dirt_path.png",
	"levels\\woods\\forest_floor.png",
};

struct TreeFile {
	const char *path;
	int footX;
	int footY;
};
constexpr TreeFile TreeFiles[TreeKindCount] = {
	{ nullptr, 0, 0 },
	{ "levels\\woods\\oak_large.png", 160, 432 },
	{ "levels\\woods\\oak_small.png", 112, 306 },
	{ "levels\\woods\\pine_tall.png", 96, 464 },
	{ "levels\\woods\\pine_small.png", 72, 274 },
	{ "levels\\woods\\dead_large.png", 144, 400 },
	{ "levels\\woods\\dead_small.png", 96, 274 },
};

std::array<Picture, MaterialCount> GroundArt;
std::array<Picture, TreeKindCount> TreeArt;
std::array<Picture, 12> ScatterCells; // the sheet's twelve cells, each at its own scale
bool ArtLoaded = false;
bool ArtTried = false;
/** @brief The floor last built was built from Tristram's own tiles (the Woods Painted Art option off). */
bool BuiltVanilla = true;
/** @brief ... and its trees and ground details are the painted ones, over Tristram's ground (the Mixed look). */
bool BuiltMixed = false;

/** @brief A PNG from the archives as 32-bit RGBA, or nullptr (logged). The caller frees it. */
SDL_Surface *LoadRgba(const char *path)
{
	SDL_Surface *png = LoadPNG(path);
	if (png == nullptr) {
		LogWarn("Woods art missing: {}", path);
		return nullptr;
	}
	SDL_Surface *rgba = SDL_ConvertSurfaceFormat(png, SDL_PIXELFORMAT_ABGR8888, 0);
	SDL_FreeSurface(png);
	return rgba;
}

/**
 * @brief The rectangle (@p rx, @p ry, @p rw, @p rh) of a loaded painting at @p percent of its size, each pixel the average of
 * the block it covers (an area resample, any ratio). A sprite pixel is opaque where at least half its block was.
 */
void ResampleRgba(const SDL_Surface *rgba, int rx, int ry, int rw, int rh, Picture &out, bool sprite, int percent)
{
	out.w = std::max(1, (rw * percent + 50) / 100);
	out.h = std::max(1, (rh * percent + 50) / 100);
	out.px.assign(static_cast<size_t>(out.w) * out.h, 0);
	const auto *bytes = static_cast<const uint8_t *>(rgba->pixels);
	for (int y = 0; y < out.h; y++) {
		const int sy0 = ry + y * rh / out.h;
		const int sy1 = std::max(sy0 + 1, ry + (y + 1) * rh / out.h);
		for (int x = 0; x < out.w; x++) {
			const int sx0 = rx + x * rw / out.w;
			const int sx1 = std::max(sx0 + 1, rx + (x + 1) * rw / out.w);
			int r = 0, g = 0, b = 0, n = 0;
			for (int sy = sy0; sy < sy1; sy++) {
				for (int sx = sx0; sx < sx1; sx++) {
					const uint8_t *q = bytes + static_cast<size_t>(sy) * rgba->pitch + static_cast<size_t>(sx) * 4;
					if (sprite && q[3] < 128)
						continue;
					r += q[0];
					g += q[1];
					b += q[2];
					n++;
				}
			}
			// The ground is always opaque.
			if (n == 0 || (sprite && 2 * n < (sx1 - sx0) * (sy1 - sy0)))
				continue;
			out.px[static_cast<size_t>(y) * out.w + x] = 0xFF000000U | (static_cast<uint32_t>(r / n) << 16) | (static_cast<uint32_t>(g / n) << 8) | static_cast<uint32_t>(b / n);
		}
	}
	out.spanFirst.assign(static_cast<size_t>(out.h), 0);
	out.spanLast.assign(static_cast<size_t>(out.h), 0);
	for (int y = 0; y < out.h; y++) {
		const uint32_t *row = &out.px[static_cast<size_t>(y) * out.w];
		int first = 0;
		while (first < out.w && (row[first] & 0xFF000000U) == 0)
			first++;
		int last = out.w;
		while (last > first && (row[last - 1] & 0xFF000000U) == 0)
			last--;
		out.spanFirst[y] = static_cast<int16_t>(first);
		out.spanLast[y] = static_cast<int16_t>(last);
	}
}

/** @brief A PNG from the archives at @p percent of its painted size (ResampleRgba over the whole of it). */
bool LoadScaled(const char *path, Picture &out, bool sprite, int percent)
{
	SDL_Surface *rgba = LoadRgba(path);
	if (rgba == nullptr)
		return false;
	ResampleRgba(rgba, 0, 0, rgba->w, rgba->h, out, sprite, percent);
	SDL_FreeSurface(rgba);
	return true;
}

/**
 * @brief The ground details, each its own scale (the Orcl Testing Ground's second save, 2026-10-14), in percent of the painting:
 * the sheet's twelve 64 px cells, 4 x 3, cut apart and each scaled on its own.
 */
constexpr int ScatterPercent[12] = { 73, 88, 72, 79, 120, 120, 65, 43, 93, 85, 120, 50 };

void LoadArt()
{
	if (ArtTried)
		return;
	ArtTried = true;
	bool ok = true;
	// The scales the user chose on the Orcl Testing Ground (its settings/current, saved 2026-10-14 15:19), in percent of the
	// painted art: the grasses at 13-14, the forest floor at 16, the dirt road at 25; each tree its own.
	constexpr int GroundPercent[MaterialCount] = { 14, 13, 14, 25, 16 }; // Grass, GrassDark, GrassDry, Path, Forest
	constexpr int TreePercent[TreeKindCount] = { 0, 29, 29, 35, 28, 17, 25 }; // -, oak large, oak small, pine tall, pine small, dead large, dead small
	for (int m = 0; m < MaterialCount; m++)
		ok = LoadScaled(GroundFiles[m], GroundArt[m], false, GroundPercent[m]) && ok;
	for (int t = 1; t < TreeKindCount; t++) {
		ok = LoadScaled(TreeFiles[t].path, TreeArt[t], true, TreePercent[t]) && ok;
		TreeArt[t].footX = TreeFiles[t].footX * TreePercent[t] / 100;
		TreeArt[t].footY = TreeFiles[t].footY * TreePercent[t] / 100;
	}
	if (SDL_Surface *sheet = LoadRgba("levels\\woods\\scatter_sheet.png"); sheet != nullptr) {
		const int cell = sheet->w / 4;
		for (int c = 0; c < 12; c++) {
			ResampleRgba(sheet, (c % 4) * cell, (c / 4) * cell, cell, cell, ScatterCells[c], true, ScatterPercent[c]);
			// Each stands at its cell's bottom centre, as on the sheet (32, 56 of 64).
			ScatterCells[c].footX = ScatterCells[c].w / 2;
			ScatterCells[c].footY = ScatterCells[c].h * 7 / 8;
		}
		SDL_FreeSurface(sheet);
	} else {
		ok = false;
	}
	ArtLoaded = ok;
}

/** @brief A world-pinned table of small offsets, in tiles, that makes the edges between materials irregular. */
constexpr int NoiseSize = 64;
std::array<std::array<int8_t, 2>, NoiseSize * NoiseSize> EdgeNoise {};
bool EdgeNoiseBuilt = false;

void BuildEdgeNoise()
{
	if (EdgeNoiseBuilt)
		return;
	EdgeNoiseBuilt = true;
	for (int y = 0; y < NoiseSize; y++) {
		for (int x = 0; x < NoiseSize; x++) {
			// Tileable: sampled on a torus of the value noise's period.
			const float a = ValueNoise(x, y, 8.0F, 0xED6EU) + 0.5F * ValueNoise(x, y, 3.0F, 0x5EEDU);
			const float b = ValueNoise(x + 101, y + 37, 8.0F, 0xED6FU) + 0.5F * ValueNoise(x + 13, y + 71, 3.0F, 0x5EEEU);
			EdgeNoise[static_cast<size_t>(y) * NoiseSize + x] = { static_cast<int8_t>((a / 1.5F - 0.5F) * 110), static_cast<int8_t>((b / 1.5F - 0.5F) * 110) };
		}
	}
}

/** @brief @p c at @p brightness of 256: red and blue in one multiply, green in another, never pure black (black is "nothing"). */
inline uint32_t Shade(uint32_t c, int brightness)
{
	const uint32_t b = static_cast<uint32_t>(brightness);
	return ((((c & 0xFF00FFU) * b) >> 8) & 0xFF00FFU) | ((((c & 0x00FF00U) * b) >> 8) & 0x00FF00U) | 0x010101U;
}

int BrightnessFor(int light)
{
	if (light < 0)
		return 256; // full brightness: the previews, which have no light tables
	if (light >= LightsMax)
		return 0;
	return SmoothLevelBrightness(static_cast<uint8_t>(std::max(light, 0)));
}

} // namespace

// ---------------------------------------------------------------- the game's calls

void ResetWoodsForNewGame()
{
	Game = {};
	EnterAtWayOn = false;
	EnterAtCaveMouth = false;
	ResetWoodsCavesForNewGame();
}

namespace {

/**
 * Tristram's own tiles for the woods (user, 2026-10-13: "generate Woods level 1 using vanilla assets ... Using tristram
 * textures"), found by OracoolPreview.DISABLED_TownSceneryMegatiles: the megatiles the town's sectors use past its walk
 * limit, and its grass. Ids as FillTile takes them (1-based).
 */
constexpr int VanillaGrass[] = { 1, 2, 3, 4 };
constexpr int VanillaWornGrass[] = { 5, 6, 7, 8 }; // grass with patches of bare earth
constexpr int VanillaDirt[] = { 306, 307 };
constexpr int VanillaBushes[] = { 27, 28, 29, 30 };
constexpr int VanillaSmallRocks[] = { 57, 58, 59, 60 };
constexpr int VanillaTrees[] = { 34, 41, 44, 47 };
constexpr int VanillaBigRocks[] = { 61, 62 };
constexpr int VanillaRockMass = 63; // all four pieces solid: the forest's edge

template <size_t N>
int Pick(const int (&from)[N], uint32_t h)
{
	return from[h % N];
}

/** @brief Tristram's tree-leaf overlays for the pieces that carry them (town.cpp InitTownPieces). */
void VanillaLeaves()
{
	constexpr std::pair<uint16_t, int8_t> Leaves[] = { { 359, 1 }, { 357, 2 }, { 128, 6 }, { 129, 7 }, { 127, 8 }, { 116, 9 }, { 156, 10 },
		{ 157, 11 }, { 155, 12 }, { 161, 13 }, { 159, 14 }, { 213, 15 }, { 211, 16 }, { 216, 17 }, { 215, 18 } };
	for (int y = 0; y < MAXDUNY; y++) {
		for (int x = 0; x < MAXDUNX; x++) {
			for (const auto &[piece, special] : Leaves) {
				if (dPiece[x][y] == piece) {
					dSpecial[x][y] = special;
					break;
				}
			}
		}
	}
}

/**
 * @brief The floor from Tristram's tiles: each megatile of the 40 x 40 dungeon from the woods' own map of it - the road in
 * dirt edged with worn grass, the forest's edge in the rock mass, groves in Tristram's trees, bushes and rocks, open ground in
 * grass with a few bushes and stones - and the rock mass all round outside.
 */
void BuildVanilla(uint32_t seed, bool paintedTrees)
{
	for (int y = 0; y < MAXDUNY; y += 2) {
		for (int x = 0; x < MAXDUNX; x += 2) {
			if (x < Lo || y < Lo || x > Hi || y > Hi) {
				FillMegatile(x, y, VanillaRockMass);
				if (paintedTrees && pMegaTiles != nullptr) {
					// The Mixed look: the forest beyond the map stands on grass, as inside - its trees are drawn one in four
					// (TreeHidden), and between them the rock mass showed as dark blue stone (preview, 2026-10-14). Still solid.
					const MegaTile grass = pMegaTiles[Pick(VanillaGrass, Hash(static_cast<uint32_t>(x), static_cast<uint32_t>(y), seed)) - 1];
					WoodsLookPiece[x][y] = SDL_SwapLE16(grass.micro1);
					WoodsLookPiece[x + 1][y] = SDL_SwapLE16(grass.micro2);
					WoodsLookPiece[x][y + 1] = SDL_SwapLE16(grass.micro3);
					WoodsLookPiece[x + 1][y + 1] = SDL_SwapLE16(grass.micro4);
				}
			}
		}
	}
	const auto cellHasPath = [](int mx, int my) {
		if (mx < 0 || my < 0 || mx >= WoodsCells || my >= WoodsCells)
			return false;
		for (int t = 0; t < 4; t++) {
			const int x = Lo + 2 * mx + (t & 1);
			const int y = Lo + 2 * my + (t >> 1);
			if (Tree[x][y] == NoTree && Ground[x][y] == Path)
				return true;
		}
		return false;
	};
	for (int my = 0; my < WoodsCells; my++) {
		for (int mx = 0; mx < WoodsCells; mx++) {
			int trees = 0;
			int dry = 0;
			for (int t = 0; t < 4; t++) {
				const int x = Lo + 2 * mx + (t & 1);
				const int y = Lo + 2 * my + (t >> 1);
				trees += Tree[x][y] != NoTree ? 1 : 0;
				dry += Ground[x][y] == GrassDry ? 1 : 0;
			}
			const uint32_t h = Hash(static_cast<uint32_t>(mx), static_cast<uint32_t>(my), seed ^ 0x7A11U);
			const int edge = std::min({ mx, my, WoodsCells - 1 - mx, WoodsCells - 1 - my });
			const float u = Unit(h);
			int mega;
			if (paintedTrees) {
				// The Mixed look: Tristram's grass only, worn by the road and in the dry patches. The road itself is the painted
				// dirt, drawn over it (DrawWoodsMixedGround); the trees and the ground details are the painted ones.
				const bool byPath = cellHasPath(mx, my) || cellHasPath(mx + 1, my) || cellHasPath(mx - 1, my) || cellHasPath(mx, my + 1) || cellHasPath(mx, my - 1);
				mega = byPath || dry >= 2 ? Pick(VanillaWornGrass, h >> 8) : Pick(VanillaGrass, h >> 8);
			} else if (cellHasPath(mx, my)) {
				mega = Pick(VanillaDirt, h >> 8);
			} else if (trees >= 3) {
				if (edge <= 1)
					mega = u < 0.85F ? VanillaRockMass : Pick(VanillaBigRocks, h >> 8);
				else
					mega = u < 0.55F ? Pick(VanillaTrees, h >> 8) : u < 0.75F ? Pick(VanillaBushes, h >> 8) : u < 0.90F ? Pick(VanillaBigRocks, h >> 8) : VanillaRockMass;
			} else if (trees >= 1) {
				mega = u < 0.55F ? Pick(VanillaTrees, h >> 8) : Pick(VanillaBushes, h >> 8);
			} else {
				const bool byPath = cellHasPath(mx + 1, my) || cellHasPath(mx - 1, my) || cellHasPath(mx, my + 1) || cellHasPath(mx, my - 1);
				mega = byPath || dry >= 2 ? Pick(VanillaWornGrass, h >> 8) : Pick(VanillaGrass, h >> 8);
				const float scatter = Unit(h >> 4);
				if (scatter < 0.035F)
					mega = Pick(VanillaSmallRocks, h >> 12);
				else if (scatter < 0.085F)
					mega = Pick(VanillaBushes, h >> 12);
			}
			// The minimap marks the forest as Tristram's rock mass, whichever way it is drawn.
			MapCell[mx][my] = static_cast<uint8_t>(std::min(paintedTrees && trees >= 3 ? VanillaRockMass : mega, 255));
			FillMegatile(Lo + 2 * mx, Lo + 2 * my, mega);
			if (paintedTrees) {
				// A painted tree's tile blocks as a trunk does, and is drawn as the grass it stands on.
				for (int t = 0; t < 4; t++) {
					const int x = Lo + 2 * mx + (t & 1);
					const int y = Lo + 2 * my + (t >> 1);
					if (Tree[x][y] == NoTree)
						continue;
					WoodsLookPiece[x][y] = dPiece[x][y];
					dPiece[x][y] = SolidPiece;
				}
			}
		}
	}
	if (!paintedTrees)
		VanillaLeaves();
}

} // namespace

bool WoodsPaintedArtChosen()
{
	return *sgOptions.Oracool.woodsPaintedArt;
}

void CreateWoods(uint32_t seed, lvl_entry /*entry*/)
{
	// The tile data before the generator: it picks its solid piece from it (LoadGameLevel loads it after LoadSetMap).
	LoadLevelSOLData();
	FindSolidPiece();
	Generate(seed);
	BuiltVanilla = !WoodsPaintedArtChosen();
	BuiltMixed = BuiltVanilla && *sgOptions.Oracool.woodsPaintedTrees;
	std::memset(WoodsLookPiece, 0, sizeof(WoodsLookPiece));
	if (!BuiltMixed && BuiltVanilla) {
		// Tristram's trees: the painted trees' map is not used for drawing, so it holds nothing.
		std::memset(Scatter, 0, sizeof(Scatter));
	}
	if (BuiltVanilla) {
		BuildVanilla(seed, BuiltMixed);
		if (BuiltMixed) {
			// The painted ground details only where Tristram's ground is open, and half as many: they are a second voice here.
			for (int y = 0; y < MAXDUNY; y++)
				for (int x = 0; x < MAXDUNX; x++)
					if (Scatter[x][y] != 0 && (TileHasAny(dPiece[x][y], TileProperties::Solid) || (Hash(static_cast<uint32_t>(x), static_cast<uint32_t>(y), seed) & 1) != 0))
						Scatter[x][y] = 0;
		}
		std::memset(dLight, 3, sizeof(dLight));
		ViewPosition = Arrival;
		return;
	}
	for (int y = 0; y < MAXDUNY; y += 2) {
		for (int x = 0; x < MAXDUNX; x += 2)
			FillMegatile(x, y, 1 + static_cast<int>(Hash(static_cast<uint32_t>(x), static_cast<uint32_t>(y), seed) % GrassMegatiles));
	}
	for (int y = 0; y < MAXDUNY; y++) {
		for (int x = 0; x < MAXDUNX; x++) {
			// Outside the box too, a tree or none: the road ends' clearings reach the map's edge since the 100 x 100 floors, and
			// the hero walked out past the way's strip there (audit round 1).
			if (Tree[x][y] != NoTree || !Inside({ x, y }))
				dPiece[x][y] = SolidPiece;
		}
	}
	// The minimap's map (automap.cpp reads MapCell through WoodsAutomapCell): each megatile 1 open ground, 2 the forest's edge, 3 road, 4 deep
	// forest. Before this the woods kept Tristram's dungeon[][] and the minimap drew the town (the play test, 2026-10-13).
	for (int my = 0; my < WoodsCells; my++) {
		for (int mx = 0; mx < WoodsCells; mx++) {
			int trees = 0;
			bool road = false;
			for (int t = 0; t < 4; t++) {
				const int x = Lo + 2 * mx + (t & 1);
				const int y = Lo + 2 * my + (t >> 1);
				trees += Tree[x][y] != NoTree ? 1 : 0;
				road = road || (Tree[x][y] == NoTree && Ground[x][y] == Path);
			}
			uint8_t kind = trees >= 3 ? 4 : road ? 3 : 1;
			if (kind == 4) {
				// The edge: deep forest beside open ground.
				for (int d = 0; d < 4 && kind == 4; d++) {
					const int nx = mx + (d == 0 ? 1 : d == 1 ? -1 : 0);
					const int ny = my + (d == 2 ? 1 : d == 3 ? -1 : 0);
					if (nx < 0 || ny < 0 || nx >= WoodsCells || ny >= WoodsCells)
						continue;
					int open = 0;
					for (int t = 0; t < 4; t++)
						open += Tree[Lo + 2 * nx + (t & 1)][Lo + 2 * ny + (t >> 1)] == NoTree ? 1 : 0;
					if (open >= 2)
						kind = 2;
				}
			}
			MapCell[mx][my] = kind;
		}
	}
	// Outdoors under the sky: the ground is lit all round, not dark beyond the hero's torch (Tristram's light is 0).
	std::memset(dLight, 3, sizeof(dLight));
	ViewPosition = Arrival;
}


// ---------------------------------------------------------------- the world along the roads

/** @brief objects.cpp's: loads object sheet @p ofile for this level if it is not loaded yet. */
void EnsureObjectGraphicsLoaded(object_graphic_id ofile, uint16_t animWidth);

namespace {

/**
 * @brief What stands along the woods' roads (user, 2026-10-14: "we need to introduce health shrines, mana shrines, lanterns along the
 * roads, and other world enriching assets in the woods act", then "go with your first and second batches of suggestion. no new
 * assets for now"): vanilla objects - the Blood Fountain, the Purifying Spring, a goat shrine or a cauldron, sarcophagi, chests,
 * barrels, the headless dead, the tortured, a skull pile, the burning cross - and what the game draws itself: will-o'-wisps
 * that lead off the road to a chest, a fairy ring, the cottages' lamps by night. Planned from the floor's own generation on
 * every build, so a revisit finds its wisps and its ring where its saved objects stand; the objects are added once, fresh.
 */
struct WorldPiece {
	_object_id type;
	Point at;
};
std::vector<WorldPiece> WorldPieces;
struct Wisp {
	Point road;
	Point chest;
	float phase;
};
std::array<Wisp, 2> Wisps;
int WispCount = 0;
bool HasFairyRing = false;
/** @brief The candle stands by the cottages' doors: lit by night (WoodsNightLamps). */
std::vector<Point> CandleStands;
bool Taken[MAXDUNX][MAXDUNY];
bool InFairyRingBefore = false;
uint32_t FairyRingTicks = 0;

bool InHouseBounds(Point p, int margin)
{
	for (int i = 0; i < HouseCount; i++) {
		const House &house = Houses[i];
		if (p.x >= Lo + 2 * house.x0 - margin && p.x <= Lo + 2 * house.x1 + 1 + margin && p.y >= Lo + 2 * house.y0 - margin
		    && p.y <= Lo + 2 * house.y1 + 1 + margin)
			return true;
	}
	return false;
}

/** @brief A tile an object may stand on: open ground off the road, the river, the mouth and the houses, not in anything's way. */
bool WorldLoose(Point p)
{
	if (!InDungeonBounds(p) || !Inside(p) || Taken[p.x][p.y])
		return false;
	if (TileHasAny(dPiece[p.x][p.y], TileProperties::Solid) || Tree[p.x][p.y] != NoTree || InRiver(p) || InMouth(p))
		return false;
	return true;
}

/** @brief And with room round it: the eight tiles about it walkable, the roads' ends, the waypoint and the mouth kept clear. */
bool WorldOpen(Point p)
{
	if (!WorldLoose(p) || Ground[p.x][p.y] == Path || InHouseBounds(p, 1))
		return false;
	for (const Point keep : { Arrival, WayOn, Arrival + Displacement { -2, -2 } }) {
		if (p.WalkingDistance(keep) < 4)
			return false;
	}
	if (MouthOnFloor && p.WalkingDistance(MouthEntrance) < 5)
		return false;
	if (HasFairyRing && p.WalkingDistance(GladeCentre) < 5)
		return false;
	for (int dy = -1; dy <= 1; dy++) {
		for (int dx = -1; dx <= 1; dx++) {
			const Point q = p + Displacement { dx, dy };
			if (!InDungeonBounds(q) || TileHasAny(dPiece[q.x][q.y], TileProperties::Solid))
				return false;
		}
	}
	return true;
}

void Take(Point p, int radius)
{
	for (int dy = -radius; dy <= radius; dy++)
		for (int dx = -radius; dx <= radius; dx++)
			if (InDungeonBounds(p + Displacement { dx, dy }))
				Taken[p.x + dx][p.y + dy] = true;
}

/** @brief The first tile @p minD..@p maxD from @p from (the ring of each, from a place @p h picks) that @p ok takes. */
template <typename Ok>
std::optional<Point> NearOff(Point from, int minD, int maxD, uint32_t h, Ok ok)
{
	for (int d = minD; d <= maxD; d++) {
		std::vector<Point> ring;
		for (int dy = -d; dy <= d; dy++)
			for (int dx = -d; dx <= d; dx++)
				if (std::max(std::abs(dx), std::abs(dy)) == d)
					ring.push_back(from + Displacement { dx, dy });
		const size_t start = h % ring.size();
		for (size_t k = 0; k < ring.size(); k++) {
			const Point p = ring[(start + k) % ring.size()];
			if (ok(p))
				return p;
		}
	}
	return std::nullopt;
}

void Put(_object_id type, std::optional<Point> at, int spacing = 2)
{
	if (!at)
		return;
	WorldPieces.push_back({ type, *at });
	Take(*at, spacing);
}

void PlanWoodsWorld(uint32_t seed, int floor)
{
	WorldPieces.clear();
	CandleStands.clear();
	WispCount = 0;
	std::memset(Taken, 0, sizeof(Taken));
	// The road, from the arrival to the way on and the glade's path: its tiles by their distance from the arrival.
	std::vector<Point> road;
	for (int y = Lo; y <= Hi; y++)
		for (int x = Lo; x <= Hi; x++)
			if (Ground[x][y] == Path && Tree[x][y] == NoTree && !InRiver({ x, y }))
				road.push_back({ x, y });
	std::sort(road.begin(), road.end(), [](Point a, Point b) {
		const int da = a.WalkingDistance(Arrival), db = b.WalkingDistance(Arrival);
		return da != db ? da < db : (a.y != b.y ? a.y < b.y : a.x < b.x);
	});
	const auto along = [&](int percent) { return road[std::min(road.size() - 1, road.size() * static_cast<size_t>(percent) / 100)]; };
	const auto open = [](Point p) { return WorldOpen(p); };
	const auto loose = [](Point p) { return WorldLoose(p); };
	// The fairy ring in the glade's middle, where nothing else stands.
	HasFairyRing = WorldLoose(GladeCentre) && !TileHasAny(dPiece[GladeCentre.x][GladeCentre.y], TileProperties::Solid);
	if (HasFairyRing)
		Take(GladeCentre, 3);
	if (road.empty())
		return;
	// The health shrine by the road a third of the way along; the mana shrine by the river, or two thirds along.
	Put(OBJ_BLOODFTN, NearOff(along(35), 2, 5, Hash(seed, 1, 0xF0), open), 3);
	std::optional<Point> spring;
	{
		std::vector<Point> bank;
		for (int y = Lo; y <= Hi; y++)
			for (int x = Lo; x <= Hi; x++) {
				if (!InRiver({ x, y }))
					continue;
				for (const Displacement d : { Displacement { 2, 0 }, Displacement { -2, 0 }, Displacement { 0, 2 }, Displacement { 0, -2 } })
					bank.push_back(Point { x, y } + d);
			}
		const size_t start = bank.empty() ? 0 : Hash(seed, 2, 0xF1) % bank.size();
		for (size_t k = 0; k < bank.size() && !spring; k++)
			if (WorldOpen(bank[(start + k) % bank.size()]))
				spring = bank[(start + k) % bank.size()];
		if (!spring)
			spring = NearOff(along(65), 3, 6, Hash(seed, 3, 0xF2), open);
	}
	Put(OBJ_PURIFYINGFTN, spring, 3);
	// A shrine in the first pen - the goat shrine on even floors, the cauldron on odd - or near the glade.
	const _object_id shrine = floor % 2 == 0 ? OBJ_GOATSHRINE : OBJ_CAULDRON;
	if (PenCount > 0 && WorldLoose(Pens[0].centre))
		Put(shrine, Pens[0].centre);
	else
		Put(shrine, NearOff(GladeCentre, 5, 8, Hash(seed, 4, 0xF3), open));
	// The second pen a little graveyard below the first floor, a chest's otherwise; the others a chest each.
	const _object_id chest = floor <= 1 ? OBJ_CHEST1 : floor == 2 ? OBJ_CHEST2 : OBJ_CHEST3;
	for (int i = 1; i < PenCount; i++) {
		const Point c = Pens[i].centre;
		if (i == 1 && floor >= 1) {
			Put(OBJ_SARC, WorldLoose(c) ? std::optional<Point>(c) : std::nullopt, 0);
			const Point second = c + Displacement { 1, 0 };
			if (WorldLoose(second) && !TileHasAny(dPiece[second.x + 1][second.y], TileProperties::Solid))
				Put(OBJ_SARC, second, 0);
		} else if (WorldLoose(c)) {
			Put(chest, c);
		}
	}
	// Each cottage: a candle stand either side of its door, a barrel near it.
	for (int i = 0; i < HouseCount; i++) {
		const House &house = Houses[i];
		const int doorX = Lo + 2 * house.x1 + 1;
		const int archY0 = Lo + 2 * house.doorY;
		for (const Point candle : { Point { doorX, archY0 - 1 }, Point { doorX, archY0 + 2 } }) {
			if (WorldLoose(candle)) {
				Put(OBJ_CANDLE2, candle, 0);
				CandleStands.push_back(candle);
			}
		}
		Put(Hash(seed, 5, static_cast<uint32_t>(i)) % 4 == 0 ? OBJ_BARRELEX : OBJ_BARREL, NearOff({ doorX, archY0 }, 3, 5, Hash(seed, 6, static_cast<uint32_t>(i)), open));
	}
	// Barrels by the road, one in four of them the kind that bursts.
	for (const int percent : { 20, 50, 80 })
		Put(Hash(seed, 7, static_cast<uint32_t>(percent)) % 4 == 0 ? OBJ_BARRELEX : OBJ_BARREL, NearOff(along(percent), 2, 3, Hash(seed, 8, static_cast<uint32_t>(percent)), open));
	// The headless dead by the road, to be searched.
	for (const int percent : { 15, 75 })
		Put(OBJ_DECAP, NearOff(along(percent), 2, 3, Hash(seed, 9, static_cast<uint32_t>(percent)), open));
	// The grim ones at the gates: the tortured by the way on, a skull pile by the way back.
	const _object_id tortured[] = { OBJ_TNUDEM1, OBJ_TNUDEM2, OBJ_TNUDEM3, OBJ_TNUDEM4 };
	for (uint32_t k = 0; k < 2; k++)
		Put(tortured[Hash(seed, 10, k) % 4], NearOff(WayOn + Displacement { 5, 0 }, 2, 5, Hash(seed, 11, k), open));
	Put(OBJ_SKPILE, NearOff(Arrival + Displacement { -5, 0 }, 2, 5, Hash(seed, 12, 0), open));
	// And on the last floor, at the end of the path, the burning cross.
	if (floor == WoodsFloors - 1)
		Put(OBJ_BCROSS, NearOff(WayOn + Displacement { 3, 0 }, 1, 4, Hash(seed, 13, 0), open), 2);
	// Two will-o'-wisps, each over a chest in a pocket of the forest six to ten tiles off the road.
	for (const int percent : { 30, 70 }) {
		if (WispCount >= static_cast<int>(Wisps.size()))
			break;
		const Point from = along(percent);
		const uint32_t h = Hash(seed, 14, static_cast<uint32_t>(percent));
		std::optional<Point> pocket = NearOff(from, 6, 10, h, [](Point p) { return WorldLoose(p) && Ground[p.x][p.y] == Forest && !InHouseBounds(p, 2); });
		if (!pocket)
			pocket = NearOff(from, 6, 10, h, open);
		if (!pocket)
			continue;
		Put(OBJ_CHEST2, pocket, 2);
		Wisps[WispCount++] = { from, *pocket, static_cast<float>(h % 1000) / 1000.0F };
	}
	(void)loose;
}

void EnsureWoodsWorldGraphics()
{
	for (const _object_id type : { OBJ_BLOODFTN, OBJ_PURIFYINGFTN, OBJ_GOATSHRINE, OBJ_CAULDRON, OBJ_SARC, OBJ_CHEST1, OBJ_CHEST2, OBJ_CHEST3,
	         OBJ_CANDLE2, OBJ_BARREL, OBJ_BARRELEX, OBJ_DECAP, OBJ_TNUDEM1, OBJ_SKPILE, OBJ_BCROSS })
		EnsureObjectGraphicsLoaded(AllObjects[type].ofindex, AllObjects[type].animWidth);
}

void PlaceWoodsWorldObjects()
{
	for (const WorldPiece &piece : WorldPieces) {
		if (dObject[piece.at.x][piece.at.y] != 0 || TileHasAny(dPiece[piece.at.x][piece.at.y], TileProperties::Solid))
			continue;
		AddObject(piece.type, piece.at);
	}
}

/** @brief A wisp's place now, in tiles: drifting between the road and its chest and back, swaying as it goes. */
void WispAt(const Wisp &wisp, float &x, float &y)
{
	const float t = static_cast<float>(SDL_GetTicks() % 3600000U) / 1000.0F;
	const float u = 0.5F - 0.5F * std::cos(6.2831853F * (t / 18.0F + wisp.phase));
	const float dx = static_cast<float>(wisp.chest.x - wisp.road.x);
	const float dy = static_cast<float>(wisp.chest.y - wisp.road.y);
	const float len = std::max(1.0F, std::sqrt(dx * dx + dy * dy));
	const float sway = 0.45F * std::sin(t * 1.7F + wisp.phase * 6.0F);
	x = static_cast<float>(wisp.road.x) + dx * u - dy / len * sway;
	y = static_cast<float>(wisp.road.y) + dy * u + dx / len * sway;
}

/** @brief The wisp's chest still shut: once it is opened, its wisp is gone. */
bool WispLives(const Wisp &wisp)
{
	const int id = dObject[wisp.chest.x][wisp.chest.y];
	if (id == 0)
		return false;
	const Object &object = Objects[std::abs(id) - 1];
	return object._oSelFlag != 0;
}

} // namespace

const std::vector<WoodsLamp> &WoodsNightLamps()
{
	static std::vector<WoodsLamp> lamps;
	lamps.clear();
	if (leveltype != DTYPE_WOODS || !setlevel || !IsWoodsLevel(setlvlnum))
		return lamps;
	// The cottages' lamps behind their doors, and their candle stands outside.
	for (int i = 0; i < HouseCount; i++) {
		const House &house = Houses[i];
		lamps.push_back({ static_cast<float>(Lo + 2 * house.x1 - 1), static_cast<float>(Lo + 2 * house.doorY + 1), 5.0F, 0.9F, 0xFFB464U });
	}
	for (const Point candle : CandleStands)
		lamps.push_back({ static_cast<float>(candle.x), static_cast<float>(candle.y), 3.5F, 0.8F, 0xFFC878U });
	for (int i = 0; i < WispCount; i++) {
		if (!WispLives(Wisps[i]))
			continue;
		float x, y;
		WispAt(Wisps[i], x, y);
		lamps.push_back({ x, y, 3.0F, 0.7F, 0xA0D2FFU });
	}
	if (HasFairyRing)
		lamps.push_back({ static_cast<float>(GladeCentre.x), static_cast<float>(GladeCentre.y), 3.5F, 0.6F, 0x78F0C8U });
	return lamps;
}

void DrawWoodsGlows(const Surface &out, Point firstTile, Point firstTileBuffer)
{
	if (leveltype != DTYPE_WOODS || !setlevel || !IsWoodsLevel(setlvlnum) || out.isIndexed() || out.w() <= 0 || out.h() <= 0)
		return;
	std::vector<Splat> *splats = SplatCollection();
	const auto add = [&](int px, int py, int r, int g, int b) {
		if (px < 0 || py < 0 || px >= out.w() || py >= out.h() || (r | g | b) <= 0)
			return;
		r = std::min(r, 255);
		g = std::min(g, 255);
		b = std::min(b, 255);
		if (splats != nullptr) {
			splats->push_back({ static_cast<float>(px), static_cast<float>(py), static_cast<float>(r) / 255.0F, static_cast<float>(g) / 255.0F, static_cast<float>(b) / 255.0F, 0.0F });
			return;
		}
		uint32_t &c = *out.at<uint32_t>(px, py);
		const int cr = std::min(255, static_cast<int>((c >> 16) & 0xFF) + r);
		const int cg = std::min(255, static_cast<int>((c >> 8) & 0xFF) + g);
		const int cb = std::min(255, static_cast<int>(c & 0xFF) + b);
		c = (c & 0xFF000000U) | (static_cast<uint32_t>(cr) << 16) | (static_cast<uint32_t>(cg) << 8) | static_cast<uint32_t>(cb);
	};
	const auto glow = [&](float cx, float cy, int radius, uint32_t rgb, float k) {
		if (cx < -radius || cy < -radius || cx > out.w() + radius || cy > out.h() + radius || k <= 0.01F)
			return;
		const int x0 = static_cast<int>(cx);
		const int y0 = static_cast<int>(cy);
		for (int dy = -radius; dy <= radius; dy++) {
			for (int dx = -radius; dx <= radius; dx++) {
				const float d = std::sqrt(static_cast<float>(dx * dx + dy * dy)) / static_cast<float>(radius);
				if (d >= 1.0F)
					continue;
				const float f = (1.0F - d) * (1.0F - d) * k;
				add(x0 + dx, y0 + dy, static_cast<int>(static_cast<float>((rgb >> 16) & 0xFF) * f), static_cast<int>(static_cast<float>((rgb >> 8) & 0xFF) * f),
				    static_cast<int>(static_cast<float>(rgb & 0xFF) * f));
			}
		}
	};
	const auto screen = [&](float tx, float ty, float &sx, float &sy) {
		const float ux = tx - static_cast<float>(firstTile.x);
		const float uy = ty - static_cast<float>(firstTile.y);
		sx = static_cast<float>(firstTileBuffer.x) + 32.0F + (ux - uy) * 32.0F;
		sy = static_cast<float>(firstTileBuffer.y) - 15.0F + (ux + uy) * 16.0F;
	};
	const float t = static_cast<float>(SDL_GetTicks() % 3600000U) / 1000.0F;
	// Brighter by night; by day a faint shimmer.
	const float night = TownIsDark() ? 1.0F : 0.4F;
	for (int i = 0; i < WispCount; i++) {
		if (!WispLives(Wisps[i]))
			continue;
		float x, y, sx, sy;
		WispAt(Wisps[i], x, y);
		screen(x, y, sx, sy);
		sy -= 34.0F + 6.0F * std::sin(t * 2.3F + Wisps[i].phase * 9.0F); // it floats at a man's chest
		const float flicker = 0.85F + 0.15F * std::sin(t * 11.0F + Wisps[i].phase * 4.0F);
		glow(sx, sy, 18, 0x6EA8FFU, 0.55F * night * flicker);
		glow(sx, sy, 5, 0xE6F4FFU, 1.1F * night * flicker);
	}
	if (HasFairyRing) {
		for (int k = 0; k < 11; k++) {
			const float a = 6.2831853F * static_cast<float>(k) / 11.0F;
			float sx, sy;
			screen(static_cast<float>(GladeCentre.x) + 2.4F * std::cos(a), static_cast<float>(GladeCentre.y) + 2.4F * std::sin(a), sx, sy);
			const float pulse = 0.55F + 0.45F * std::sin(t * 1.3F + static_cast<float>(k) * 1.7F);
			glow(sx, sy - 3.0F, 9, 0x50E6B4U, 0.45F * night * pulse);
			glow(sx, sy - 3.0F, 3, 0xD2FFF0U, 0.9F * night * pulse);
		}
	}
}

void TickWoods()
{
	if (leveltype != DTYPE_WOODS || !setlevel || !IsWoodsLevel(setlvlnum) || MyPlayer == nullptr || !HasFairyRing)
		return;
	Player &player = *MyPlayer;
	const bool in = player._pHitPoints > 0 && player.position.tile.WalkingDistance(GladeCentre) <= 2;
	if (in && !InFairyRingBefore)
		EventPlrMsg("The fairy ring hums. Your mana flows faster here.", UiFlags::ColorWhite);
	InFairyRingBefore = in;
	if (!in || ++FairyRingTicks % 20 != 0)
		return;
	// Three percent of the hero's mana a second while he stands in it.
	const int add = player._pMaxMana * 3 / 100;
	if (add <= 0 || player._pMana >= player._pMaxMana)
		return;
	player._pMana = std::min(player._pMana + add, player._pMaxMana);
	player._pManaBase = std::min(player._pManaBase + add, player._pMaxManaBase);
	RedrawComponent(PanelDrawComponent::Mana);
}

void BuildWoodsLevel(bool fresh)
{
	const int floor = FloorIndex(setlvlnum);
	if (Game.seed[floor] == 0)
		Game.seed[floor] = (static_cast<uint32_t>(AdvanceRndSeed()) ^ (SDL_GetTicks() * 2654435761U)) | 1U;
	if (Game.rung[floor] == 0) {
		// D3: the floor's depth is fixed the first time it is entered in a game - by the hero who enters it.
		const int heroLevel = MyPlayer != nullptr ? MyPlayer->_pLevel : 1;
		Game.rung[floor] = static_cast<uint8_t>(std::clamp(std::clamp(heroLevel / 3, 1, RungsPerDifficulty) + floor, 1, RungsPerDifficulty));
	}
	MouthFloor = WoodsCaveOnFloor(floor) != SL_NONE ? floor : -1;
	ForgetWoodsMixedFloor(); // the last floor's ground pictures
	CreateDungeon(Game.seed[floor], ENTRY_SETLVL);
	MouthFloor = -1;
	if (MouthOnFloor) {
		for (int y = MouthOrigin.y; y < MouthOrigin.y + MouthHeight; y++)
			for (int x = MouthOrigin.x; x < MouthOrigin.x + MouthWidth; x++)
				Scatter[x][y] = 0;
	}
	StampRiver();
	StampCaveMouth();
	LayFences();
	LayHouses();
	ViewPosition = EnterAtWayOn ? Point { WayOn.x + 2, WayOn.y } : Arrival;
	EnterAtWayOn = false;
	InitNoTriggers();
	LoadRndLvlPal(leveltype);
	LoadLevelSOLData();
	if (EnterAtCaveMouth && MouthOnFloor) {
		// Out of the cave: open ground in front of the mouth, never its entrance (that would send the hero straight back).
		Point landing = OpenFloorNear(MouthEntrance + Displacement { 4, 3 });
		if (landing.WalkingDistance(MouthEntrance) < 2)
			landing = MouthEntrance + Displacement { 4, 3 };
		ViewPosition = landing;
	}
	EnterAtCaveMouth = false;
	// No pack on either road's end: the hero arrives at one, and may come back by the other.
	for (const Point end : { Arrival, WayOn, MouthOnFloor ? MouthEntrance : Arrival }) {
		for (int dy = -5; dy <= 5; dy++) {
			for (int dx = -5; dx <= 5; dx++) {
				const Point tile = end + Displacement { dx, dy };
				if (InDungeonBounds(tile))
					dFlags[tile.x][tile.y] |= DungeonFlag::Populated;
			}
		}
	}
	// The floor's waypoint (D9): in the arrival's clearing, two tiles up the screen from where the hero lands. A revisit's comes
	// back with the level's save; its graphic is loaded on every build, or a restored sigil draws from nothing.
	const Point sigil = Arrival + Displacement { -2, -2 };
	Scatter[sigil.x][sigil.y] = 0;
	if (fresh)
		AddGeneratedLevelWaypointSigil(sigil, FirstWoodsWaypoint + floor);
	else
		EnsureWaypointGraphicsLoaded();
	// What stands along the roads (PlanWoodsWorld): planned on every build, its objects added on the first.
	PlanWoodsWorld(Game.seed[floor], floor);
	EnsureWoodsWorldGraphics();
	if (fresh)
		PlaceWoodsWorldObjects();
	if (!HeadlessMode) {
		LoadArt();
		BuildEdgeNoise();
	}
}

namespace {

/**
 * @brief The road out of Tristram: at the end of the cleared path below Pepin's house, on the map's south-west edge (user,
 * 2026-10-13: "Going west of Pepin takes us to the four grass/woods levels"; then, with a screenshot, "i want the entrance to
 * the woods to be where the green circle is"). Standing on it loads woods floor 1.
 */
constexpr int GateX0 = 56;
constexpr int GateX1 = 59;
constexpr int GateY0 = 94;
constexpr int GateY1 = 95;

bool OnTownGate(Point tile)
{
	return tile.x >= GateX0 && tile.x <= GateX1 && tile.y >= GateY0 && tile.y <= GateY1;
}

/** @brief Whether the hero last stood in the woods' way-on strip, so its message shows once per visit to it. */
bool AtWayOnBefore = false;

} // namespace

Point WoodsTownReturnTile()
{
	return { 57, GateY0 - 2 }; // on the cleared path, two rows above the entrance
}

bool CheckWoodsEdges()
{
	if (MyPlayer == nullptr)
		return false;
	if (setlevel && IsWoodsCaveLevel(setlvlnum))
		return MyPlayer->_pmode == PM_STAND && !MyPlayer->_pLvlChanging && CheckWoodsCaveExits();
	Player &player = *MyPlayer;
	if (player._pmode != PM_STAND || player._pLvlChanging)
		return false;
	const Point tile = player.position.tile;
	if (leveltype == DTYPE_TOWN) {
		if (!OnTownGate(tile))
			return false;
		setlvltype = DTYPE_WOODS; // BEFORE StartNewLvl: the level loads its tileset from this
		StartNewLvl(player, WM_DIABSETLVL, SL_ORCL_WOODS_1);
		return true;
	}
	if (leveltype != DTYPE_WOODS || !setlevel)
		return false;
	const int floor = FloorIndex(setlvlnum);
	if (MouthOnFloor && tile == MouthEntrance) {
		EnterWoodsCaveFromMouth(floor);
		return true;
	}
	// The road back: the last tiles of the arrival road, at the map's edge - to Tristram from the first floor, to the way on
	// of the floor before from the others (milestone 2: four linked floors).
	if (tile.x >= Hi - 1 && std::abs(tile.y - Arrival.y) <= 1) {
		if (floor == 0) {
			StartNewLvl(player, WM_DIABRTNLVL, GetMapReturnLevel());
		} else {
			setlvltype = DTYPE_WOODS;
			EnterAtWayOn = true;
			StartNewLvl(player, WM_DIABSETLVL, static_cast<int>(SL_FIRST_WOODS) + floor - 1);
		}
		return true;
	}
	const bool atWayOn = tile.x <= Lo + 1 && std::abs(tile.y - WayOn.y) <= 1;
	if (atWayOn && floor + 1 < WoodsFloors) {
		setlvltype = DTYPE_WOODS;
		EnterAtWayOn = false;
		StartNewLvl(player, WM_DIABSETLVL, static_cast<int>(SL_FIRST_WOODS) + floor + 1);
		return true;
	}
	if (atWayOn && !AtWayOnBefore)
		EventPlrMsg("The path ends here, at the heart of the woods.", UiFlags::ColorWhite);
	AtWayOnBefore = atWayOn;
	return false;
}

namespace {

/** @brief A house's footprint in world tile corners: west and north walls' lines to the east and south walls'. */
void HouseCorners(const House &house, int &wx0, int &wy0, int &wx1, int &wy1)
{
	wx0 = Lo + 2 * house.x0;
	wy0 = Lo + 2 * house.y0;
	wx1 = Lo + 2 * house.x1;
	wy1 = Lo + 2 * house.y1;
}

bool InsideHouse(const House &house, Point tile)
{
	int wx0, wy0, wx1, wy1;
	HouseCorners(house, wx0, wy0, wx1, wy1);
	return tile.x > wx0 && tile.x < wx1 && tile.y > wy0 && tile.y < wy1;
}

} // namespace

void UpdateWoodsShells()
{
	for (int i = 0; i < MaxHouses; i++)
		WoodsShellState[i] = 2;
	if (leveltype != DTYPE_WOODS || !setlevel || MyPlayer == nullptr || !MyPlayer->isOnActiveLevel())
		return;
	const Point hero = MyPlayer->position.tile;
	for (int i = 0; i < HouseCount; i++) {
		const House &house = Houses[i];
		int wx0, wy0, wx1, wy1;
		HouseCorners(house, wx0, wy0, wx1, wy1);
		if (InsideHouse(house, hero))
			WoodsShellState[i] = 0; // the cottage gone: the room under it
		else if (hero.x >= wx0 - 6 && hero.x <= wx1 + 1 && hero.y >= wy0 - 6 && hero.y <= wy1 + 1 && (hero.x <= wx0 || hero.y <= wy0))
			WoodsShellState[i] = 1; // behind it: see-through, as Tristram's own walls are
	}
}

bool WoodsShellCovers(Point tile)
{
	if (leveltype != DTYPE_WOODS || WoodsShellAt(tile) == 0)
		return false;
	const int owner = WoodsShellOwner[tile.x][tile.y];
	return owner >= 0 && owner < HouseCount && WoodsShellState[owner] == 2 && InsideHouse(Houses[owner], tile);
}

void FinishWoodsLevel(bool fresh)
{
	if (!fresh || !setlevel || !IsWoodsLevel(setlvlnum))
		return;
	// What each house holds on the first visit: three things on its floor, one of them a good one.
	for (int i = 0; i < HouseCount; i++) {
		const House &house = Houses[i];
		for (int k = 0; k < 3; k++) {
			const Point at { Lo + 2 * house.x0 + 2 + static_cast<int>(Hash(static_cast<uint32_t>(i), static_cast<uint32_t>(k), 0x100DU) % static_cast<uint32_t>(2 * (house.x1 - house.x0) - 3)),
				Lo + 2 * house.y0 + 2 + static_cast<int>(Hash(static_cast<uint32_t>(k), static_cast<uint32_t>(i), 0x100EU) % static_cast<uint32_t>(2 * (house.y1 - house.y0) - 3)) };
			if (!IsTileSolid(at))
				CreateRndItem(at, /*onlygood=*/k == 0, /*sendmsg=*/false, /*delta=*/false);
		}
	}
}

int WoodsHouseCountForTest()
{
	return HouseCount;
}

Point WoodsHouseDoorForTest(int index)
{
	if (index < 0 || index >= HouseCount)
		return { 0, 0 };
	const House &house = Houses[index];
	return { Lo + 2 * house.x1 + 1, Lo + 2 * house.doorY + 1 }; // outside the east arch
}

Point WoodsHouseInsideForTest(int index)
{
	if (index < 0 || index >= HouseCount)
		return { 0, 0 };
	const House &house = Houses[index];
	return { Lo + 2 * house.x1 - 1, Lo + 2 * house.doorY + 1 }; // inside it
}

int WoodsFloorRung(int floorIndex)
{
	return Game.rung[std::clamp(floorIndex, 0, WoodsFloors - 1)];
}

void EnterWoodsAtCaveMouthNext()
{
	EnterAtCaveMouth = true;
}

bool WoodsTileUsesCels(Point tile)
{
	return leveltype == DTYPE_WOODS && (InMouth(tile) || InRiver(tile) || WoodsShellAt(tile) != 0);
}

bool WoodsCaveMouthHover(Point tile, Point &entrance, const char *&text)
{
	if (leveltype != DTYPE_WOODS || !setlevel || !InMouth(tile))
		return false;
	static std::string hover;
	hover = std::string("Down to ") + WoodsCaveName(WoodsCaveOnFloor(FloorIndex(setlvlnum)));
	entrance = MouthEntrance;
	text = hover.c_str();
	return true;
}

int WoodsBandFloor()
{
	if (setlevel && IsWoodsCaveLevel(setlvlnum))
		return WoodsCaveRung();
	if (!setlevel || !IsWoodsLevel(setlvlnum))
		return 1;
	const int rung = Game.rung[FloorIndex(setlvlnum)];
	return rung == 0 ? 1 : rung;
}

namespace {

/**
 * @brief The user's Mob Roster (Woods Levels Designer, 2026-10-14: "If i tick a mob, you can use it on Woods Act"): the 40 kinds
 * ticked there that are not bosses. Only these walk the woods and their caves.
 */
constexpr _monster_id WoodsKinds[] = {
	MT_NZOMBIE,
	MT_RFALLSP, MT_DFALLSP, MT_YFALLSP, MT_BFALLSP,
	MT_WSKELAX, MT_TSKELAX, MT_RSKELAX, MT_XSKELAX,
	MT_RFALLSD, MT_DFALLSD, MT_YFALLSD, MT_BFALLSD,
	MT_NSCAV, MT_BSCAV, MT_WSCAV, MT_YSCAV,
	MT_WSKELBW, MT_TSKELBW, MT_RSKELBW, MT_XSKELBW,
	MT_WSKELSD, MT_TSKELSD, MT_RSKELSD, MT_XSKELSD,
	MT_LRDSAYTR,
	MT_NGOATMC, MT_BGOATMC, MT_RGOATMC, MT_GGOATMC,
	MT_NGOATBW, MT_BGOATBW, MT_RGOATBW, MT_GGOATBW,
	MT_FAT, MT_TOAD,
	MT_HORNED, MT_MUDRUN, MT_FROSTC, MT_OBLORD,
	MT_SUCCUBUS,
};

/**
 * @brief The bosses of the Mob Roster's notes ("use as boss. scaled up to be diablo size"), each with the size that makes him
 * Diablo's height - the designer's sprite heights: Diablo 151 px, the King 118, the Butcher 69, the Reaper 86. The fifth note,
 * oldboned, is on a kind left unticked (and Hellfire's unused one, with no sounds of its own), so it is not among them.
 */
struct WoodsBossRow {
	_monster_id kind;
	unsigned sizePercent;
};
constexpr WoodsBossRow WoodsBosses[] = {
	{ MT_SKING, 128 },
	{ MT_CLEAVER, 219 },
	{ MT_REAPER, 176 },
	{ MT_DIABLO, 100 },
};

bool WoodsKindAvailable(const MonsterData &data)
{
	if (data.availability == MonsterAvailability::Never)
		return false;
	return !(gbIsSpawn && data.availability == MonsterAvailability::Retail);
}

/**
 * @brief A kind's own floors on the ladder (1-16): the Hive's and the Crypt's eight below their number (LadderFloorOf), as their
 * monsters stand in a rift; Diablo's row says 26 to keep him out of the rosters, and his floor is 16 (audit round 1: the boss
 * scaled from 26 was a flat quarter on most rungs, and the Satyr Lord - 21-22 - came only on rung 16).
 */
int LadderMin(const MonsterData &data)
{
	return &data == &MonstersData[MT_DIABLO] ? 16 : LadderFloorOf(data.minDunLvl);
}
int LadderMax(const MonsterData &data)
{
	return &data == &MonstersData[MT_DIABLO] ? 16 : LadderFloorOf(data.maxDunLvl);
}

/** @brief How many floors @p data's own band stands from @p floor: 0 inside it. */
int BandGap(const MonsterData &data, int floor)
{
	if (floor < LadderMin(data))
		return LadderMin(data) - floor;
	if (floor > LadderMax(data))
		return floor - LadderMax(data);
	return 0;
}

/** @brief 100 * 1.12^@p gap, in whole percent - a floor's worth of life and damage per step. */
int FloorStepPercent(int gap)
{
	return static_cast<int>(std::lround(100.0 * std::pow(1.12, static_cast<double>(gap))));
}

} // namespace

bool WoodsAcceptsMonster(const MonsterData &data)
{
	if (!WoodsKindAvailable(data))
		return false;
	bool listed = false;
	for (const _monster_id kind : WoodsKinds)
		listed = listed || &data == &MonstersData[kind];
	if (!listed)
		return false;
	// The nearest gap that gives eight kinds to choose from (dev note, 2026-10-14: "mix and diversify mobs on the levels. they
	// are now all the same" - four of one rung were often four goatmen): ScaleWoodsMonster brings each to the rung.
	const int floor = WoodsBandFloor();
	int reach = 0;
	for (;; reach++) {
		int kinds = 0;
		for (const _monster_id kind : WoodsKinds)
			kinds += WoodsKindAvailable(MonstersData[kind]) && BandGap(MonstersData[kind], floor) <= reach ? 1 : 0;
		if (kinds >= 8 || reach >= 32)
			break;
	}
	return BandGap(data, floor) <= reach;
}

int WoodsBossKind()
{
	const uint32_t seed = Game.seed[WoodsFloors - 1];
	if (seed == 0)
		return -1;
	// The seed's high bits: its low ones also build the floor, and a pick tied to them would tie the boss to the map. Only the
	// kinds this game has the sprites of (the shareware has no Reaper and no Diablo): the next one on where the pick is not.
	const size_t first = (seed >> 20) % std::size(WoodsBosses);
	for (size_t k = 0; k < std::size(WoodsBosses); k++) {
		const WoodsBossRow &row = WoodsBosses[(first + k) % std::size(WoodsBosses)];
		const MonsterData &data = MonstersData[row.kind];
		if (!(gbIsSpawn && data.availability == MonsterAvailability::Retail))
			return row.kind;
	}
	return -1;
}

bool IsWoodsBoss(const Monster &monster)
{
	if (!setlevel || setlvlnum != SL_LAST_WOODS || monster.isPlayerMinion())
		return false;
	return monster.type().type == WoodsBossKind();
}

int WoodsFamilyOf(int kind)
{
	// The sprite's folder, by its first three letters: "falspear" and "falsword" are both the Fallen, "skelaxe" and "skelbow"
	// both skeletons, "goatmace" and "goatbow" both goatmen.
	const char *path = MonstersData[kind].assetsSuffix;
	if (path == nullptr)
		return kind;
	int family = 0;
	for (int i = 0; i < 3 && path[i] != '\0'; i++)
		family = family * 256 + static_cast<unsigned char>(path[i]);
	return family;
}

unsigned WoodsBossSizePercent(const Monster &monster)
{
	if (!IsWoodsBoss(monster))
		return 0;
	for (const WoodsBossRow &row : WoodsBosses) {
		if (row.kind == monster.type().type)
			return row.sizePercent;
	}
	return 0;
}

std::optional<Point> WoodsBossSpot()
{
	// The open tile nearest six steps in from the way on: inside the fence ring (its cells' tiles and the box's edge outside it
	// are off - audit round 1: the scan began at the map's edge, west of the fence), off the river, the cave mouth and the
	// houses. He guards the way out of the woods.
	const int fenceIn = Lo + 2 * (RingLo + 1);    // the first tile inside the ring
	const int fenceOut = Lo + 2 * RingHi - 1;     // the last
	const Point aim = WayOn + Displacement { 6, 0 };
	for (int radius = 0; radius <= 40; radius++) {
		for (int dx = -radius; dx <= radius; dx++) {
			for (int dy = -radius; dy <= radius; dy++) {
				if (std::max(std::abs(dx), std::abs(dy)) != radius)
					continue;
				const Point p = aim + Displacement { dx, dy };
				if (p.x < fenceIn || p.y < fenceIn || p.x > fenceOut || p.y > fenceOut || IsTileSolid(p) || dMonster[p.x][p.y] != 0
				    || dPlayer[p.x][p.y] != 0 || dObject[p.x][p.y] != 0 || InRiver(p) || InMouth(p))
					continue;
				bool inHouse = false;
				for (int i = 0; i < HouseCount && !inHouse; i++)
					inHouse = InsideHouse(Houses[i], p) || WoodsShellAt(p) != 0;
				if (!inHouse)
					return p;
			}
		}
	}
	return std::nullopt;
}

int WoodsBossDamagePercent(const Monster &monster)
{
	if (!IsWoodsBoss(monster))
		return 100;
	return std::max(25, FloorStepPercent(WoodsBandFloor() - LadderMax(monster.data()))) * 5 / 4;
}

void ScaleWoodsMonster(Monster &monster)
{
	if (!setlevel || !IsOrclActLevel(setlvlnum) || monster.isPlayerMinion() || monster.type().type == MT_GOLEM)
		return;
	const int floor = WoodsBandFloor();
	const bool boss = IsWoodsBoss(monster);
	const MonsterData &data = monster.data();
	int pct;
	int lifePct;
	int damagePct;
	if (boss) {
		// Set to the rung from his own floors, down as well as up (Diablo is a floor-16 monster, the Butcher a floor-one), not
		// below a quarter; then a boss's share on top.
		const int gap = floor - LadderMax(data);
		pct = std::max(25, FloorStepPercent(gap));
		lifePct = pct * 3;
		damagePct = pct * 5 / 4;
	} else {
		// Up for a kind whose floors end above the rung, down (not below half) for one whose floors start below it: the roster
		// reaches either way for its eight kinds.
		const int gap = floor > LadderMax(data) ? floor - LadderMax(data) : floor < LadderMin(data) ? floor - LadderMin(data) : 0;
		if (gap == 0)
			return;
		pct = std::max(50, FloorStepPercent(gap));
		lifePct = pct;
		damagePct = pct;
	}
	const auto scale = [](int value, int percent) { return static_cast<int>(static_cast<int64_t>(value) * percent / 100); };
	monster.maxHitPoints = std::max(64, scale(monster.maxHitPoints, lifePct));
	monster.hitPoints = monster.maxHitPoints;
	ScaleMonsterDamage(monster, damagePct, damagePct);
	if (pct > 100)
		monster.armorClass = static_cast<uint8_t>(std::min(255, monster.armorClass + (pct - 100) / 6));
}

bool IsChampionKind(int type)
{
	for (size_t u = 0; UniqueMonstersData[u].mtype != -1; u++) {
		const UniqueMonsterData &row = UniqueMonstersData[u];
		if (row.mtype == type && row.mtalkmsg == TEXT_NONE)
			return true;
	}
	return false;
}

uint32_t WoodsRosterSeed()
{
	if (setlevel && IsWoodsCaveLevel(setlvlnum))
		return WoodsCaveRosterSeed();
	return Game.seed[FloorIndex(setlvlnum)] ^ 0x0AC7U;
}

Point WoodsEntryTile()
{
	return ViewPosition;
}

const char *WoodsFloorName(_setlevels level)
{
	constexpr const char *Names[WoodsFloors] = { "The Greywood Verge", "Thornmoor", "The Hanging Wood", "Witchbark Hollow" };
	return Names[FloorIndex(level)];
}

bool WoodsSpritesOverTiles(const Surface &out)
{
	return leveltype == DTYPE_WOODS && BuiltMixed && ArtLoaded && !out.isIndexed();
}

bool WoodsArtActive(const Surface &out)
{
	return leveltype == DTYPE_WOODS && !BuiltVanilla && ArtLoaded && !out.isIndexed();
}

namespace {

/** @brief DrawWoodsGroundTile's body: the materials of @p tile and its neighbours, the texture pinned where @p world lies. */
void DrawGroundAt(const Surface &out, Point tile, Point world, Point target, int light, bool fullBright)
{
	const int brightness = (fullBright && light < LightsMax) || light < 0 ? 256 : BrightnessFor(light);
	// The tile's diamond: 31 rows ending on the tile's bottom line, as dun_render draws a floor's two triangles.
	const int originX = (world.x - world.y) * 32; // where the tile lies in the world, in pixels
	const int originY = (world.x + world.y) * 16;
	const int centreX = target.x + 32;
	const int centreY = target.y - 15;
	for (int k = 0; k < 31; k++) {
		const int y = target.y - k;
		if (y < 0 || y >= out.h())
			continue;
		const int inset = k < 16 ? 30 - 2 * k : 2 * (k - 15);
		const int x0 = std::max(target.x + inset, 0);
		const int x1 = std::min(target.x + 63 - inset, out.w() - 1);
		if (x0 > x1)
			continue;
		uint32_t *row = out.at<uint32_t>(0, y);
		const int wy = originY + (y - target.y) + (1 << 20);
		const int dy = y - centreY;
		for (int x = x0; x <= x1; x++) {
			const int wx = originX + (x - target.x) + (1 << 20);
			// Which material this pixel takes: its own tile's, or a neighbour's where the world-pinned noise pushes the
			// edge across - so the grass, the path and the forest floor meet along an irregular line, not the tile grid.
			const auto &n = EdgeNoise[static_cast<size_t>((wy >> 3) & (NoiseSize - 1)) * NoiseSize + ((wx >> 3) & (NoiseSize - 1))];
			const int dx = x - centreX;
			// Tile-space position of the pixel relative to the tile's centre, in 1/64 tile, plus the noise.
			const int fu = dx + 2 * dy + n[0];
			const int fv = 2 * dy - dx + n[1];
			const int tu = std::clamp(tile.x + (fu >= 32 ? 1 : fu < -32 ? -1 : 0), 0, MAXDUNX - 1);
			const int tv = std::clamp(tile.y + (fv >= 32 ? 1 : fv < -32 ? -1 : 0), 0, MAXDUNY - 1);
			const Picture &tex = GroundArt[Ground[tu][tv]];
			const uint32_t c = tex.px[static_cast<size_t>(wy % tex.h) * tex.w + (wx % tex.w)];
			row[x] = brightness >= 256 ? (c & 0xFFFFFF) | 0x010101 : Shade(c, brightness);
		}
	}
}

} // namespace


namespace {

/** @brief One floor tile's finished picture: the 31 rows of the diamond, 64 wide, bottom row first; 0 off the diamond. */
struct FloorPicture {
	std::array<uint32_t, 64 * 31> px {};
};

/**
 * @brief The Mixed look's floor, made once and kept (the user's frame-rate complaint of 2026-10-14; DISABLED_WoodsFrameBenchmark
 * timed the per-frame grade and road at 72 ms and Tristram's floor cels under them at 13 ms, a Debug 1920 x 1080 frame): a tile's
 * ground never changes, so each is drawn whole - its cels at full light, the grade, the road - the first time it is seen, then
 * copied with its light. Grass far from the road by its piece (one picture serves every tile of that piece); a tile within two of
 * the road by itself, since the road's blend is its own. Rebuilt for a new floor and when the palette changes.
 */
std::unordered_map<uint16_t, std::unique_ptr<FloorPicture>> FloorByPiece;
std::unordered_map<uint32_t, std::unique_ptr<FloorPicture>> FloorByTile;
uint32_t FloorCachePalette = 0;
/** @brief Each tile's picture once it has been looked up (null: not yet, or the tile is not drawn from the cache). */
const FloorPicture *FloorOfTile[MAXDUNX][MAXDUNY];
/** @brief The tile was looked up and is drawn as it is (the river, the mouth, a cottage, a borrowed piece). */
bool FloorNotCached[MAXDUNX][MAXDUNY];
/**
 * @brief The Painted Art look's ground, a picture per tile once it is seen (user, 2026-10-14: the painted look for real, "so i can
 * test it in real life what we achieved with the size calibration"; DISABLED_WoodsFrameBenchmark timed its per-pixel blend at
 * 60 ms a Debug frame). At most PaintedCacheCap of them (8 KB each); past it the cache starts again.
 */
std::unique_ptr<FloorPicture> PaintedOfTile[MAXDUNX][MAXDUNY];
size_t PaintedCount = 0;
/**
 * @brief No cap: room for every tile of the map, about 100 MB at the most (audit round 3: a cap of 6000 under a 4K screen's ~8000
 * tiles cleared and rebuilt each frame). Freed with the floor, and when the hero leaves the woods (LoadGameLevel).
 */

bool BlitFloorPicture(const Surface &out, const FloorPicture &pic, Point target, int light, bool fullBright);

bool TileNearRoad(Point tile)
{
	if (InRiver(tile))
		return false;
	for (int dy = -2; dy <= 2; dy++)
		for (int dx = -2; dx <= 2; dx++) {
			const Point p = tile + Displacement { dx, dy };
			if (InDungeonBounds(p) && Ground[p.x][p.y] == Path && Tree[p.x][p.y] == NoTree)
				return true;
		}
	return false;
}

} // namespace

void ForgetWoodsMixedFloor()
{
	for (auto &column : PaintedOfTile)
		for (auto &picture : column)
			picture.reset();
	PaintedCount = 0;
	FloorByPiece.clear();
	FloorByTile.clear();
	std::memset(FloorOfTile, 0, sizeof(FloorOfTile));
	std::memset(FloorNotCached, 0, sizeof(FloorNotCached));
}

bool DrawWoodsMixedFloorCached(const Surface &out, Point tile, Point target, int light, bool fullBright)
{
	if (leveltype != DTYPE_WOODS || !BuiltMixed || out.isIndexed() || GroundArt[Path].px.empty() || pDungeonCels == nullptr)
		return false;
	if (FloorCachePalette != PaletteRgbGeneration) {
		ForgetWoodsMixedFloor();
		FloorCachePalette = PaletteRgbGeneration;
	}
	// A cottage comes and goes as the hero enters it: asked every frame. The rest is looked up once a floor.
	if (WoodsShellAt(tile) != 0 || FloorNotCached[tile.x][tile.y])
		return false;
	if (const FloorPicture *known = FloorOfTile[tile.x][tile.y]; known != nullptr)
		return BlitFloorPicture(out, *known, target, light, fullBright);
	if (InMouth(tile) || InRiver(tile) || IsBorrowedPiece(WoodsDrawnPiece(tile))) {
		FloorNotCached[tile.x][tile.y] = true; // drawn as they are: the water's colours cycle, the rest is not plain ground
		return false;
	}
	const uint16_t piece = WoodsDrawnPiece(tile);
	const bool own = TileNearRoad(tile);
	std::unique_ptr<FloorPicture> &slot = own ? FloorByTile[static_cast<uint32_t>(tile.y) * MAXDUNX + static_cast<uint32_t>(tile.x)] : FloorByPiece[piece];
	if (slot == nullptr) {
		// Drawn once: the piece's two floor micros at full light, then the grade and the road, into a tile-sized picture.
		OwnedSurface canvas = OwnedSurface::Rgb(64, 32);
		const Surface surface = canvas;
		for (int y = 0; y < 32; y++)
			std::fill_n(surface.at<uint32_t>(0, y), 64, 0U);
		const uint8_t *full = LightTables[0].data();
		for (int side = 0; side < 2; side++) {
			const LevelCelBlock block { DPieceMicros[piece].mt[side] };
			if (block.hasValue())
				RenderTile(surface, { side * 32, 31 }, block, MaskType::Solid, full);
		}
		DrawWoodsMixedGround(surface, tile, { 0, 31 }, -1, true);
		slot = std::make_unique<FloorPicture>();
		for (int k = 0; k < 31; k++) {
			const int inset = k < 16 ? 30 - 2 * k : 2 * (k - 15);
			const uint32_t *row = surface.at<uint32_t>(0, 31 - k);
			for (int x = inset; x <= 63 - inset; x++)
				slot->px[static_cast<size_t>(k) * 64 + x] = (row[x] & 0xFFFFFFU) | 0x01000000U; // the top byte marks the diamond
		}
	}
	FloorOfTile[tile.x][tile.y] = slot.get();
	return BlitFloorPicture(out, *slot, target, light, fullBright);
}

namespace {

bool BlitFloorPicture(const Surface &out, const FloorPicture &pic, Point target, int light, bool fullBright)
{
	const int brightness = (fullBright && light < LightsMax) ? 256 : light < 0 ? 256 : BrightnessFor(light);
	for (int k = 0; k < 31; k++) {
		const int y = target.y - k;
		if (y < 0 || y >= out.h())
			continue;
		const int inset = k < 16 ? 30 - 2 * k : 2 * (k - 15);
		const int x0 = std::max(inset, -target.x);
		const int x1 = std::min(63 - inset, out.w() - 1 - target.x);
		uint32_t *row = out.at<uint32_t>(0, y) + target.x;
		const uint32_t *src = &pic.px[static_cast<size_t>(k) * 64];
		if (brightness >= 256) {
			for (int x = x0; x <= x1; x++)
				row[x] = src[x] & 0xFFFFFFU;
		} else {
			// Red and blue in one multiply, green in another (the per-pixel Shade call was most of the copy in a Debug build).
			const uint32_t b = static_cast<uint32_t>(brightness);
			for (int x = x0; x <= x1; x++) {
				const uint32_t p = src[x];
				row[x] = ((((p & 0xFF00FFU) * b) >> 8) & 0xFF00FFU) | ((((p & 0x00FF00U) * b) >> 8) & 0x00FF00U) | 0x010101U;
			}
		}
	}
	return true;
}

} // namespace

void DrawWoodsGroundTile(const Surface &out, Point tile, Point target, int light, bool fullBright)
{
	if (out.isIndexed() || !InDungeonBounds(tile)) {
		DrawGroundAt(out, tile, tile, target, light, fullBright);
		return;
	}
	std::unique_ptr<FloorPicture> &slot = PaintedOfTile[tile.x][tile.y];
	if (slot == nullptr) {
		// Drawn once at full light into a tile-sized picture, then copied with the light every frame.
		OwnedSurface canvas = OwnedSurface::Rgb(64, 32);
		const Surface surface = canvas;
		for (int y = 0; y < 32; y++)
			std::fill_n(surface.at<uint32_t>(0, y), 64, 0U);
		DrawGroundAt(surface, tile, tile, { 0, 31 }, -1, true);
		auto picture = std::make_unique<FloorPicture>();
		for (int k = 0; k < 31; k++) {
			const int inset = k < 16 ? 30 - 2 * k : 2 * (k - 15);
			const uint32_t *row = surface.at<uint32_t>(0, 31 - k);
			for (int x = inset; x <= 63 - inset; x++)
				picture->px[static_cast<size_t>(k) * 64 + x] = (row[x] & 0xFFFFFFU) | 0x01000000U;
		}
		PaintedOfTile[tile.x][tile.y] = std::move(picture);
		PaintedCount++;
	}
	BlitFloorPicture(out, *PaintedOfTile[tile.x][tile.y], target, light, fullBright);
}

void DrawWoodsMixedGround(const Surface &out, Point tile, Point target, int light, bool fullBright)
{
	if (leveltype != DTYPE_WOODS || !BuiltMixed || out.isIndexed() || GroundArt[Path].px.empty())
		return;
	const int brightness = (fullBright && light < LightsMax) || light < 0 ? 256 : BrightnessFor(light);
	const Picture &road = GroundArt[Path];
	const int originX = (tile.x - tile.y) * 32;
	const int originY = (tile.x + tile.y) * 16;
	const int centreX = target.x + 32;
	const int centreY = target.y - 15;
	// Only a tile within two of the road carries any of it; the rest is graded and left.
	const auto onRoad = [](int x, int y) {
		x = std::clamp(x, 0, MAXDUNX - 1);
		y = std::clamp(y, 0, MAXDUNY - 1);
		return Ground[x][y] == Path && Tree[x][y] == NoTree ? 1 : 0;
	};
	bool nearRoad = false;
	for (int dy = -2; dy <= 2 && !nearRoad; dy++)
		for (int dx = -2; dx <= 2 && !nearRoad; dx++) {
			const Point p = tile + Displacement { dx, dy };
			nearRoad = InDungeonBounds(p) && Ground[p.x][p.y] == Path && Tree[p.x][p.y] == NoTree;
		}
	if (InRiver(tile))
		nearRoad = false; // the bridge's planks and the water, not the painted road
	for (int k = 0; k < 31; k++) {
		const int y = target.y - k;
		if (y < 0 || y >= out.h())
			continue;
		const int inset = k < 16 ? 30 - 2 * k : 2 * (k - 15);
		const int x0 = std::max(target.x + inset, 0);
		const int x1 = std::min(target.x + 63 - inset, out.w() - 1);
		uint32_t *row = out.at<uint32_t>(0, y);
		const int wy = originY + (y - target.y) + (1 << 20);
		const int dy = y - centreY;
		for (int x = x0; x <= x1; x++) {
			const uint32_t c = row[x];
			// Tristram's grey-green grass toward the painted trees' olive (the user's frame, graded three ways: this one read
			// as the same woods): red x0.85, green x1.08, blue x0.62.
			uint32_t r = std::max<uint32_t>(((c >> 16) & 0xFF) * 218 >> 8, 1);
			uint32_t g = std::max<uint32_t>(std::min<uint32_t>(255, ((c >> 8) & 0xFF) * 276 >> 8), 1);
			uint32_t b = std::max<uint32_t>((c & 0xFF) * 159 >> 8, 1);
			if (nearRoad) {
				// How much road this pixel is (user, 2026-10-13: "a fading effect where road tiles spill into grass, not sharp
				// edges"): the road tiles' share blended across the tile centres, nudged by the world-pinned noise so the edge
				// frays, then eased over the middle half - about a tile of fade.
				const int wx = originX + (x - target.x) + (1 << 20);
				const auto &n = EdgeNoise[static_cast<size_t>((wy >> 3) & (NoiseSize - 1)) * NoiseSize + ((wx >> 3) & (NoiseSize - 1))];
				const int dx = x - centreX;
				const int u = tile.x * 64 + dx + 2 * dy + 64 * 4; // in 1/64 tile, tile centres on multiples of 64
				const int v = tile.y * 64 + 2 * dy - dx + 64 * 4;
				const int ui = u / 64 - 4;
				const int vi = v / 64 - 4;
				const int fu = u & 63;
				const int fv = v & 63;
				int share = (onRoad(ui, vi) * (64 - fu) + onRoad(ui + 1, vi) * fu) * (64 - fv)
				    + (onRoad(ui, vi + 1) * (64 - fu) + onRoad(ui + 1, vi + 1) * fu) * fv; // 0..4096
				share += (n[0] + n[1]) * 8;
				int a = std::clamp((share - 1024) * 256 / 2048, 0, 256);
				a = a * a * (768 - 2 * a) >> 16; // smoothstep, 0..256
				if (a > 0) {
					uint32_t t = road.px[static_cast<size_t>(wy % road.h) * road.w + (wx % road.w)];
					t = brightness >= 256 ? t : Shade(t, brightness);
					r = (((t >> 16) & 0xFF) * a + r * (256 - a)) >> 8;
					g = (((t >> 8) & 0xFF) * a + g * (256 - a)) >> 8;
					b = ((t & 0xFF) * a + b * (256 - a)) >> 8;
				}
			}
			row[x] = (std::max<uint32_t>(r, 1) << 16) | (std::max<uint32_t>(g, 1) << 8) | std::max<uint32_t>(b, 1);
		}
	}
}

namespace {

/**
 * @brief Where a tree in front of the hero must let him show (user's play test, 2026-10-13: the canopies hid the hero):
 * the hero's chest on screen and the circle round it the tree is drawn see-through in. Set per tree by
 * DrawWoodsTileSprite; radius 0 for none.
 */
struct SeeThrough {
	int x = 0;
	int y = 0;
	int radius = 0;
};

void DrawPicture(const Surface &out, const Picture &pic, int left, int top, int srcX, int srcY, int w, int h, int brightness, SeeThrough hole = {})
{
	if (brightness <= 0)
		return;
	const int r2 = hole.radius * hole.radius;
	const int y0 = std::max(0, -top);
	const int y1 = std::min(h, out.h() - top);
	const bool wholeRows = srcX == 0 && w == pic.w && !pic.spanFirst.empty();
	for (int y = y0; y < y1; y++) {
		const int sy = top + y;
		uint32_t *row = out.at<uint32_t>(0, sy);
		const uint32_t *src = &pic.px[static_cast<size_t>(srcY + y) * pic.w + srcX];
		// Clipped to the screen, and to the row's opaque span when the whole picture is drawn.
		int x0 = std::max(0, -left);
		int x1 = std::min(w, out.w() - left);
		if (wholeRows) {
			x0 = std::max<int>(x0, pic.spanFirst[srcY + y]);
			x1 = std::min<int>(x1, pic.spanLast[srcY + y]);
		}
		for (int x = x0; x < x1; x++) {
			const int sx = left + x;
			const uint32_t c = src[x];
			if ((c & 0xFF000000U) == 0)
				continue;
			const uint32_t lit = brightness >= 256 ? (c & 0xFFFFFF) | 0x010101 : Shade(c, brightness);
			if (r2 > 0) {
				const int ddx = sx - hole.x;
				const int ddy = (sy - hole.y) * 3 / 2; // a little taller than wide, as a figure is
				const int d2 = ddx * ddx + ddy * ddy;
				if (d2 < r2) {
					// A quarter of the tree at the centre, all of it at the circle's edge.
					const int keep = 64 + 192 * d2 / r2;
					const uint32_t bg = row[sx];
					const auto mix = [&](int shift) { return ((((lit >> shift) & 0xFF) * keep + ((bg >> shift) & 0xFF) * (256 - keep)) >> 8) & 0xFF; };
					row[sx] = (static_cast<uint32_t>(mix(16)) << 16) | (static_cast<uint32_t>(mix(8)) << 8) | static_cast<uint32_t>(mix(0));
					continue;
				}
			}
			row[sx] = lit;
		}
	}
}

/**
 * @brief A tree that sways (user, 2026-10-14: "animate a bit the trees like we did in tristram"): @p pic drawn as DrawPicture
 * draws it, each row shifted by @p wave times its share of the sway - nothing at the foot (@p footRow, the picture's own row),
 * full from 48 px up, eased in as the town's foliage is (scene_fx.cpp ApplyFoliageSway). A shift between whole pixels blends the
 * two source pixels it falls between, so the crown moves smoothly rather than a pixel at a time.
 */
void DrawPictureSwaying(const Surface &out, const Picture &pic, int left, int top, int brightness, SeeThrough hole, float wave, int footRow)
{
	if (brightness <= 0)
		return;
	const int r2 = hole.radius * hole.radius;
	const int y0 = std::max(0, -top);
	const int y1 = std::min(pic.h, out.h() - top);
	const int w = pic.w;
	for (int y = y0; y < y1; y++) {
		const int sy = top + y;
		uint32_t *row = out.at<uint32_t>(0, sy);
		const uint32_t *src = &pic.px[static_cast<size_t>(y) * w];
		const float up = std::min(1.0F, static_cast<float>(std::max(0, footRow - y)) / 48.0F);
		const float shift = wave * std::pow(up, 1.3F);
		const int whole = static_cast<int>(std::floor(shift));
		const int part = static_cast<int>((shift - static_cast<float>(whole)) * 256.0F); // the share of the pixel to the left
		const int first = pic.spanFirst.empty() ? 0 : pic.spanFirst[y];
		const int last = pic.spanLast.empty() ? w : pic.spanLast[y];
		const int x0 = std::max(first + whole, -left);
		const int x1 = std::min(last + whole + 1, out.w() - left);
		for (int x = x0; x < x1; x++) {
			const int i1 = x - whole;     // the pixel the shift's whole part lands here
			const int i0 = i1 - 1;        // and the one to its left, which the fraction brings in
			const uint32_t c1 = i1 >= 0 && i1 < w ? src[i1] : 0;
			const uint32_t c0 = i0 >= 0 && i0 < w ? src[i0] : 0;
			const bool o1 = (c1 & 0xFF000000U) != 0;
			const bool o0 = (c0 & 0xFF000000U) != 0;
			uint32_t c;
			if (o1 && o0) {
				const auto blend = [&](int sh) { return ((((c1 >> sh) & 0xFF) * (256 - part) + ((c0 >> sh) & 0xFF) * part) >> 8) & 0xFF; };
				c = 0xFF000000U | (static_cast<uint32_t>(blend(16)) << 16) | (static_cast<uint32_t>(blend(8)) << 8) | static_cast<uint32_t>(blend(0));
			} else if (part < 128 ? o1 : o0) {
				c = part < 128 ? c1 : c0; // an edge: the nearer of the two, or nothing
			} else {
				continue;
			}
			const int sx = left + x;
			const uint32_t lit = brightness >= 256 ? (c & 0xFFFFFF) | 0x010101 : Shade(c, brightness);
			if (r2 > 0) {
				const int ddx = sx - hole.x;
				const int ddy = (sy - hole.y) * 3 / 2;
				const int d2 = ddx * ddx + ddy * ddy;
				if (d2 < r2) {
					const int keep = 64 + 192 * d2 / r2;
					const uint32_t bg = row[sx];
					const auto mix = [&](int sh) { return ((((lit >> sh) & 0xFF) * keep + ((bg >> sh) & 0xFF) * (256 - keep)) >> 8) & 0xFF; };
					row[sx] = (static_cast<uint32_t>(mix(16)) << 16) | (static_cast<uint32_t>(mix(8)) << 8) | static_cast<uint32_t>(mix(0));
					continue;
				}
			}
			row[sx] = lit;
		}
	}
}

} // namespace

namespace {
/** @brief A mirrored tree past the map's edge is being drawn: it is never in front of the hero, so it opens no see-through. */
bool DrawingBeyondMap = false;

/**
 * @brief The mirror of a tile past the edge, moved a band (six tiles) along the edge where it lies by a road's end: the band's
 * tiles beside the box's road strip blended its dirt, and the mirror repeated it every six tiles out (audit round 2).
 */
Point BeyondMapMirror(Point tile)
{
	const Point m = WoodsMirrorBeyondMap(tile);
	if (!TileNearRoad(m))
		return m;
	for (const Displacement d : { Displacement { 0, Lo }, Displacement { 0, -Lo }, Displacement { Lo, 0 }, Displacement { -Lo, 0 },
	         Displacement { 0, 2 * Lo }, Displacement { 0, -2 * Lo }, Displacement { 2 * Lo, 0 }, Displacement { -2 * Lo, 0 } }) {
		const Point n = m + d;
		if (InDungeonBounds(n) && !Inside(n) && !TileNearRoad(n))
			return n;
	}
	return m;
}
} // namespace

bool DrawWoodsBeyondMapFloor(const Surface &out, Point tile, Point target, bool fullBright)
{
	if (leveltype != DTYPE_WOODS || InDungeonBounds(tile))
		return false;
	const Point m = BeyondMapMirror(tile);
	const int light = ExplorationMemoryLight(m, dLight[m.x][m.y]);
	if (WoodsArtActive(out) && !WoodsTileUsesCels(m)) {
		DrawGroundAt(out, m, tile, target, light, fullBright); // the texture pinned to where the tile is, not its mirror
		return true;
	}
	return DrawWoodsMixedFloorCached(out, m, target, light, fullBright);
}

bool WoodsBeyondMapUsesCels(const Surface &out)
{
	return leveltype == DTYPE_WOODS && !WoodsArtActive(out) && !(BuiltMixed && !out.isIndexed());
}

void DrawWoodsBeyondMapTree(const Surface &out, Point tile, Point target)
{
	// The painted trees only where the look draws them (the Painted Art, or the Mixed look's trees over Tristram's cels) and
	// on a true-colour surface: DrawPicture writes 32-bit pixels (audit round 1). The pure Tristram look draws the mirror's
	// own cels instead (WoodsBeyondMapUsesCels).
	if (leveltype != DTYPE_WOODS || InDungeonBounds(tile) || out.isIndexed() || !(WoodsArtActive(out) || BuiltMixed))
		return;
	const Point m = BeyondMapMirror(tile);
	DrawingBeyondMap = true;
	DrawWoodsTileSprite(out, m, target, ExplorationMemoryLight(m, dLight[m.x][m.y]));
	DrawingBeyondMap = false;
}

void DrawWoodsTileSprite(const Surface &out, Point tile, Point target, int light)
{
	if (!InDungeonBounds(tile))
		return;
	const int brightness = BrightnessFor(light);
	const int footX = target.x + 32;
	const int footY = target.y - 15;
	if (const uint8_t kind = Tree[tile.x][tile.y]; kind != NoTree) {
		if (TreeHidden[tile.x][tile.y])
			return; // deep in the forest, behind the trees in front
		const Picture &pic = TreeArt[kind];
		if (pic.px.empty())
			return;
		const int left = footX + TreeJitterX[tile.x][tile.y] - pic.footX;
		const int top = footY + TreeJitterY[tile.x][tile.y] - pic.footY;
		SeeThrough hole;
		// In front of the hero (drawn after his row) and over him on screen: the circle round his chest is see-through.
		if (MyPlayer != nullptr && MyPlayer->isOnActiveLevel() && !DrawingBeyondMap) {
			const Point hero = MyPlayer->position.tile;
			if (hero.x + hero.y < tile.x + tile.y) {
				const int heroX = footX + ((hero.x - tile.x) - (hero.y - tile.y)) * 32;
				const int heroY = footY + ((hero.x - tile.x) + (hero.y - tile.y)) * 16 - 36;
				if (heroX > left - 40 && heroX < left + pic.w + 40 && heroY > top - 40 && heroY < top + pic.h + 40)
					hole = { heroX, heroY, 70 };
			}
		}
		// The sway (Living Town, as Tristram's trees): the trees of the playable woods, each on its own phase from where it
		// stands; the forest beyond the fence stands still - it is the costly part of the picture and no one walks there.
		if (*sgOptions.Oracool.livingTown && Inside(tile) && !out.isIndexed()) {
			const float t = static_cast<float>(SDL_GetTicks() % 3600000U) / 1000.0F;
			const float phase = static_cast<float>((tile.x - tile.y) * 32) * 0.012F + static_cast<float>(TreeJitterX[tile.x][tile.y]) * 0.21F;
			// A taller tree sways a little further: 1.2 px at the crown of a small one, up to 2 px for the tallest.
			const float reach = std::min(2.0F, 1.2F + static_cast<float>(pic.h) / 300.0F);
			const float wave = reach * (std::sin(t * 1.1F + phase) + 0.3F * std::sin(t * 2.7F + phase * 2.0F)) / 1.3F;
			DrawPictureSwaying(out, pic, left, top, brightness, hole, wave, pic.footY);
		} else {
			DrawPicture(out, pic, left, top, 0, 0, pic.w, pic.h, brightness, hole);
		}
		return;
	}
	if (const uint8_t cell = Scatter[tile.x][tile.y]; cell != 0 && cell <= ScatterCells.size() && !ScatterCells[cell - 1].px.empty()) {
		const Picture &pic = ScatterCells[cell - 1];
		DrawPicture(out, pic, footX - pic.footX, footY - pic.footY, 0, 0, pic.w, pic.h, brightness);
	}
}

void DrawWoodsGateTint(const Surface &out, Point tile, Point target)
{
	if (out.isIndexed())
		return;
	// Which road, if any, this tile belongs to.
	bool open = false;
	bool closed = false;
	if (leveltype == DTYPE_TOWN) {
		open = OnTownGate(tile);
	} else if (leveltype == DTYPE_WOODS && setlevel) {
		const bool wayOn = tile.x <= Lo + 1 && std::abs(tile.y - WayOn.y) <= 1;
		const bool lastFloor = FloorIndex(setlvlnum) == WoodsFloors - 1;
		// The road back, the way on to the next floor (amber on the last, where it ends), the cave's entrance.
		open = (tile.x >= Hi - 1 && std::abs(tile.y - Arrival.y) <= 1) || (wayOn && !lastFloor) || (MouthOnFloor && tile == MouthEntrance);
		closed = wayOn && lastFloor;
	}
	if (!open && !closed)
		return;
	// Green for a road you can take, amber for one not open yet; a slow pulse, a little stronger toward the tile's centre.
	const int tr = open ? 120 : 236;
	const int tg = open ? 255 : 170;
	const int tb = open ? 110 : 60;
	const float pulse = 0.5F + 0.5F * std::sin(static_cast<float>(SDL_GetTicks() % 100000U) * 0.004F);
	const int base = static_cast<int>((open ? 70.0F : 55.0F) + 50.0F * pulse); // of 256
	for (int k = 0; k < 31; k++) {
		const int y = target.y - k;
		if (y < 0 || y >= out.h())
			continue;
		const int inset = k < 16 ? 30 - 2 * k : 2 * (k - 15);
		const int x0 = std::max(target.x + inset, 0);
		const int x1 = std::min(target.x + 63 - inset, out.w() - 1);
		uint32_t *row = out.at<uint32_t>(0, y);
		const int dy = std::abs(k - 15);
		for (int x = x0; x <= x1; x++) {
			const int dx = std::abs(x - (target.x + 32));
			const int centre = 256 - std::min(256, (dx * 4 + dy * 8)); // 256 at the centre, 0 at the corners
			const int a = base * (160 + centre) / 416;
			const uint32_t c = row[x];
			const int r = static_cast<int>((c >> 16) & 0xFF);
			const int g = static_cast<int>((c >> 8) & 0xFF);
			const int b = static_cast<int>(c & 0xFF);
			// Mixed toward the tint and brightened, so it reads on dark grass and on bright paving alike.
			const int nr = std::min(255, r + ((tr - r) * a >> 8) + (a >> 3));
			const int ng = std::min(255, g + ((tg - g) * a >> 8) + (a >> 3));
			const int nb = std::min(255, b + ((tb - b) * a >> 8) + (a >> 3));
			row[x] = static_cast<uint32_t>((std::max(nr, 1) << 16) | (std::max(ng, 1) << 8) | std::max(nb, 1));
		}
	}
}

uint8_t WoodsGroundAt(Point tile)
{
	return InDungeonBounds(tile) ? Ground[tile.x][tile.y] : Forest;
}

bool WoodsTreeAt(Point tile)
{
	return !InDungeonBounds(tile) || Tree[tile.x][tile.y] != NoTree;
}

Point WoodsArrival()
{
	return Arrival;
}

Point WoodsWayOn()
{
	return WayOn;
}

uint8_t WoodsAutomapCell(Point cell)
{
	if (cell.x < 0 || cell.y < 0 || cell.x >= WoodsCells || cell.y >= WoodsCells)
		return 0;
	return MapCell[cell.x][cell.y];
}

Point WoodsMirrorBeyondMap(Point tile)
{
	// Into the forest band between the map's edge and the box (tiles 0..Lo-1, and the last Lo), shifted by whole bands - Lo is
	// six, a multiple of both TreeHidden lattices (2 x 2 and 3 x 3), so the hidden and drawn trees keep their rhythm and no
	// tile repeats its neighbour (a reflection twinned the edge row, audit round 1).
	const auto fold = [](int v, int size) {
		if (v < 0)
			return ((v % Lo) + Lo) % Lo;
		if (v >= size)
			return size - Lo + (v - size) % Lo;
		return v;
	};
	return { fold(tile.x, MAXDUNX), fold(tile.y, MAXDUNY) };
}

bool WoodsStampForTest(int which, std::vector<uint16_t> &pieces, int &w, int &h)
{
	pieces.clear();
	if (which == 0) {
		w = 8;
		h = 6;
		for (const auto &row : CottageShell)
			pieces.insert(pieces.end(), std::begin(row), std::end(row));
		return true;
	}
	if (which == 1) {
		w = MouthWidth;
		h = MouthHeight;
		for (const auto &row : MouthStamp)
			pieces.insert(pieces.end(), std::begin(row), std::end(row));
		return true;
	}
	if (which >= 2 && which <= 7) {
		w = 2;
		h = 2;
		pieces.assign(std::begin(RiverPieces[which - 1]), std::end(RiverPieces[which - 1]));
		return true;
	}
	return false;
}

void SetWoodsRiverForTest(int mode)
{
	RiverForced = mode;
}

bool WoodsRiverAtForTest(Point tile)
{
	return InRiver(tile);
}

void SetWoodsCaveMouthFloorForTest(int floorIndex)
{
	MouthFloor = floorIndex;
}

Point StampWoodsCaveMouthForTest()
{
	MouthFloor = -1;
	StampRiver();
	StampCaveMouth();
	LayFences();
	LayHouses();
	return MouthOnFloor ? MouthEntrance : Point { 0, 0 };
}

void SetWoodsRungForTest(int floorIndex, int rung)
{
	Game.rung[std::clamp(floorIndex, 0, WoodsFloors - 1)] = static_cast<uint8_t>(rung);
}

WoodsWorldPlanForTest PlanWoodsWorldForTest(uint32_t seed, int floor)
{
	PlanWoodsWorld(seed, floor);
	WoodsWorldPlanForTest plan;
	for (const WorldPiece &piece : WorldPieces)
		plan.objects.emplace_back(static_cast<int>(piece.type), piece.at);
	plan.wisps = WispCount;
	plan.fairyRing = HasFairyRing;
	plan.fairyRingAt = GladeCentre;
	return plan;
}

void SetWoodsSeedForTest(int floorIndex, uint32_t seed)
{
	Game.seed[std::clamp(floorIndex, 0, WoodsFloors - 1)] = seed;
}

void LoadWoodsArtForTest()
{
	ArtTried = false;
	LoadArt();
	BuildEdgeNoise();
}

} // namespace devilution::oracool
