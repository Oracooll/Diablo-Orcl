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
#include <vector>

#include <SDL.h>

#include "diablo.h"
#include "engine/palette.h"
#include "engine/random.hpp"
#include "engine/surface.hpp"
#include "init.h" // gbIsHellfire - the cave mouth is Hellfire's town art
#include "interfac.h"
#include "items.h" // CreateRndItem - what the houses hold
#include "levels/gendung.h"
#include "levels/trigs.h"
#include "monstdat.h"
#include "oracool/area_level.h"
#include "oracool/borrowed_tiles.h" // the Caves' fences on the woods floors
#include "oracool/oracool.h"       // AddGeneratedLevelWaypointSigil, EnsureWaypointGraphicsLoaded
#include "oracool/stairless.h"     // OpenFloorNear - the landing by the cave mouth
#include "oracool/waypoint_menu.h" // FirstWoodsWaypoint
#include "oracool/woods_caves.h"   // the caves under the floors
#include "lighting.h"
#include "options.h"
#include "oracool/colour_light.h"
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

/** @brief The floor's playable box in world tiles: the dungeon's 40 x 40 megatiles, with trees all round it. */
constexpr int Lo = 16;
constexpr int Hi = 95;

uint8_t Ground[MAXDUNX][MAXDUNY];
uint8_t Tree[MAXDUNX][MAXDUNY];
/** @brief A small thing on the ground: a scatter cell + 1 (RfA-62's sheet), 0 for none. */
uint8_t Scatter[MAXDUNX][MAXDUNY];
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
uint8_t FenceCell[DMAXX][DMAXY];
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
constexpr int RingHi = DMAXX - 2;

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
uint8_t RiverCell[DMAXX][DMAXY];
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
	if (tile.x < 16 || tile.y < 16 || tile.x >= 96 || tile.y >= 96)
		return false;
	return RiverCell[(tile.x - 16) / 2][(tile.y - 16) / 2] != NoRiver;
}

bool InMouth(Point tile)
{
	return MouthOnFloor && tile.x >= MouthOrigin.x && tile.x < MouthOrigin.x + MouthWidth && tile.y >= MouthOrigin.y && tile.y < MouthOrigin.y + MouthHeight;
}

/** @brief The river's town pieces over the floor's tiles. */
void StampRiver()
{
	for (int my = 0; my < DMAXY; my++) {
		for (int mx = 0; mx < DMAXX; mx++) {
			const uint8_t part = RiverCell[mx][my];
			if (part == NoRiver)
				continue;
			for (int t = 0; t < 4; t++) {
				const int x = 16 + 2 * mx + (t & 1);
				const int y = 16 + 2 * my + (t >> 1);
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
		const int x = 16 + 2 * mx + (t & 1);
		const int y = 16 + 2 * my + (t >> 1);
		Tree[x][y] = NoTree;
		Scatter[x][y] = 0;
	}
}

bool CellHasPath(int mx, int my)
{
	for (int t = 0; t < 4; t++) {
		if (Ground[16 + 2 * mx + (t & 1)][16 + 2 * my + (t >> 1)] == Path)
			return true;
	}
	return false;
}

bool CellOpen(int mx, int my)
{
	for (int dy = -1; dy <= 1; dy++)
		for (int dx = -1; dx <= 1; dx++)
			if (mx + dx >= 0 && my + dy >= 0 && mx + dx < DMAXX && my + dy < DMAXY && RiverCell[mx + dx][my + dy] != NoRiver)
				return false;
	for (int t = 0; t < 4; t++) {
		const int x = 16 + 2 * mx + (t & 1);
		const int y = 16 + 2 * my + (t >> 1);
		if (Tree[x][y] != NoTree || Ground[x][y] == Path || Ground[x][y] == Forest)
			return false;
	}
	return true;
}

/** @brief A road on any of the cell's four tiles. */
bool PathInCell(int mx, int my)
{
	if (mx < 0 || my < 0 || mx >= DMAXX || my >= DMAXY)
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
	const int arrivalCell = (Arrival.x - 16) / 2;
	const int wayOnCell = (WayOn.x - 16) / 2;
	bool accepted = false;
	for (int attempt = 0; attempt < 24; attempt++) {
		std::memset(RiverCell, 0, sizeof(RiverCell));
		int c = wayOnCell + 5 + static_cast<int>(Hash(seed, static_cast<uint32_t>(attempt), 0x51BE) % 8U);
		bool ok = true;
		bool diagonalNext = true;
		int j = 0;
		while (j < DMAXY && ok) {
			if (diagonalNext) {
				if (PathInCell(c, j) || PathInCell(c + 1, j) || PathInCell(c, j + 1) || PathInCell(c + 1, j + 1)) {
					ok = false; // a road on a diagonal: another column
					break;
				}
				RiverCell[c][j] = RiverM3;
				RiverCell[c + 1][j] = RiverM4;
				if (j + 1 < DMAXY) {
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
			for (int my = 0; my < DMAXY && ok; my++) {
				for (int mx = 0; mx < DMAXX && ok; mx++) {
					if (RiverCell[mx][my] == NoRiver)
						continue;
					const Point centre { 16 + 2 * mx + 1, 16 + 2 * my + 1 };
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
	for (int my = 0; my < DMAXY; my++) {
		for (int mx = 0; mx < DMAXX; mx++) {
			if (RiverCell[mx][my] == NoRiver)
				continue;
			for (int y = 16 + 2 * my; y < 18 + 2 * my; y++) {
				for (int x = 15 + 2 * mx; x < 19 + 2 * mx; x++) {
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
	if (mx < 0 || my < 0 || mx >= DMAXX || my >= DMAXY)
		return false;
	for (int dy = -1; dy <= 1; dy++)
		for (int dx = -1; dx <= 1; dx++)
			if (mx + dx >= 0 && my + dy >= 0 && mx + dx < DMAXX && my + dy < DMAXY && RiverCell[mx + dx][my + dy] != NoRiver)
				return false;
	for (int t = 0; t < 4; t++) {
		const int x = 16 + 2 * mx + (t & 1);
		const int y = 16 + 2 * my + (t >> 1);
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
	const int positions = (DMAXX - 12) * (DMAXY - 12);
	const int start = static_cast<int>(Hash(seed, 0x40E, 1) % static_cast<uint32_t>(positions));
	for (int k = 0; k < positions && HouseCount < want; k++) {
		const int at = (start + k * 7919) % positions; // a stride prime to the count: every position once, scattered
		const int bx = 4 + at % (DMAXX - 12);
		const int by = 4 + at / (DMAXX - 12);
		if (bx + W + 2 >= DMAXX - 3 || by + D + 1 >= DMAXY - 3)
			continue;
		bool fits = true;
		// The block, a cell round it, and one more in front of the east door: no road, grove, river or fence there.
		for (int my = by - 1; my <= by + D && fits; my++)
			for (int mx = bx - 1; mx <= bx + W + 1 && fits; mx++)
				fits = CellBuildable(mx, my) && FenceCell[mx][my] == 0;
		const Point centre { 16 + 2 * bx + W, 16 + 2 * by + D };
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
				if (mx >= 0 && my >= 0 && mx < DMAXX && my < DMAXY)
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
			if (cx < 0 || cy < 0 || cx >= DMAXX || cy >= DMAXY)
				continue;
			if (d != 0 && CellHasPath(cx, cy))
				FenceCell[cx][cy] = 0;
		}
		FenceCell[mx][my] = alongY ? GateAlongY : GateAlongX;
	};
	gateAt(RingHi, (Arrival.y - 16) / 2, true);
	gateAt(RingLo, (WayOn.y - 16) / 2, true);
	for (int my = 0; my < DMAXY; my++)
		for (int mx = 0; mx < DMAXX; mx++) {
			if (RiverCell[mx][my] != NoRiver)
				FenceCell[mx][my] = 0; // the river runs under the ring
			if (FenceCell[mx][my] != 0)
				ClearCell(mx, my);
		}

	// The pens.
	const Point keepAway[] = { Arrival, WayOn, MouthOnFloor ? MouthEntrance : Arrival, Arrival + Displacement { -2, -2 } };
	int pens = 0;
	const int want = 2 + static_cast<int>(Hash(seed, 7, 7) % 2);
	for (int attempt = 0; attempt < 60 && pens < want; attempt++) {
		const uint32_t h = Hash(seed, static_cast<uint32_t>(attempt), 0xBE2U);
		const int w = 3 + static_cast<int>(h % 2);
		const int d = 3 + static_cast<int>((h >> 3) % 2);
		const int mx0 = 4 + static_cast<int>((h >> 6) % static_cast<uint32_t>(DMAXX - 8 - w));
		const int my0 = 4 + static_cast<int>((h >> 14) % static_cast<uint32_t>(DMAXY - 8 - d));
		bool fits = true;
		for (int my = my0 - 1; my <= my0 + d && fits; my++)
			for (int mx = mx0 - 1; mx <= mx0 + w && fits; mx++)
				fits = CellOpen(mx, my) && FenceCell[mx][my] == 0;
		const Point centre { 16 + 2 * mx0 + w, 16 + 2 * my0 + d };
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
					const int x = 16 + 2 * mx + (t & 1);
					const int y = 16 + 2 * my + (t >> 1);
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
		const Point origin { 16 + 2 * (house.x0 - 2) + CottageShellOffset.deltaX, 16 + 2 * (house.y0 - 1) + CottageShellOffset.deltaY };
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
	for (int my = 0; my < DMAXY; my++) {
		for (int mx = 0; mx < DMAXX; mx++) {
			const uint8_t mega = FenceCell[mx][my];
			uint16_t pieces[4];
			if (mega == 0 || !BorrowedMegatilePieces(DTYPE_CAVES, mega, pieces))
				continue;
			for (int t = 0; t < 4; t++) {
				const int x = 16 + 2 * mx + (t & 1);
				const int y = 16 + 2 * my + (t >> 1);
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
			// One in two (the checker of x + y): at a quarter of the painted size (v1.201.017) the trees no longer meet at one in four.
			TreeHidden[x][y] = depth[x][y] >= 3 && ((x + y) & 1) != 0;
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
				// The bone (cell 11) is the rarest of the twelve.
				int cell = static_cast<int>((h >> 16) % 12);
				if (cell == 10 && (h & 0x300) != 0)
					cell = static_cast<int>((h >> 20) % 10);
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
Picture ScatterArt; // twelve 32 x 32 cells after halving, 4 x 3
bool ArtLoaded = false;
bool ArtTried = false;
/** @brief The floor last built was built from Tristram's own tiles (the Woods Painted Art option off). */
bool BuiltVanilla = true;
/** @brief ... and its trees and ground details are the painted ones, over Tristram's ground (the Mixed look). */
bool BuiltMixed = false;

/**
 * @brief A PNG from the archives at @p percent of its painted size, each pixel the average of the block of the painting it
 * covers (an area resample, any ratio). A sprite pixel is opaque where at least half its block was.
 */
bool LoadScaled(const char *path, Picture &out, bool sprite, int percent)
{
	SDL_Surface *png = LoadPNG(path);
	if (png == nullptr) {
		LogWarn("Woods art missing: {}", path);
		return false;
	}
	SDL_Surface *rgba = SDL_ConvertSurfaceFormat(png, SDL_PIXELFORMAT_ABGR8888, 0);
	SDL_FreeSurface(png);
	if (rgba == nullptr)
		return false;
	out.w = std::max(1, (rgba->w * percent + 50) / 100);
	out.h = std::max(1, (rgba->h * percent + 50) / 100);
	out.px.assign(static_cast<size_t>(out.w) * out.h, 0);
	const auto *bytes = static_cast<const uint8_t *>(rgba->pixels);
	for (int y = 0; y < out.h; y++) {
		const int sy0 = y * rgba->h / out.h;
		const int sy1 = std::max(sy0 + 1, (y + 1) * rgba->h / out.h);
		for (int x = 0; x < out.w; x++) {
			const int sx0 = x * rgba->w / out.w;
			const int sx1 = std::max(sx0 + 1, (x + 1) * rgba->w / out.w);
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
	SDL_FreeSurface(rgba);
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
	return true;
}

/** @brief The ground details' sheet, in percent of the painting: 25, a 16 px cell (the Testing Ground's 24-30). */
constexpr int ScatterPercent = 25;

void LoadArt()
{
	if (ArtTried)
		return;
	ArtTried = true;
	bool ok = true;
	// The scales the user chose on the Orcl Testing Ground (2026-10-14, its settings/current), in percent of the painted art:
	// the grasses at 13-14, the forest floor at 16, the dirt road kept at 25; every tree at 25; the ground details at 25 (the
	// page's 24-30 per detail, one sheet here).
	constexpr int GroundPercent[MaterialCount] = { 14, 13, 14, 25, 16 }; // Grass, GrassDark, GrassDry, Path, Forest
	constexpr int TreePercent = 25;
	for (int m = 0; m < MaterialCount; m++)
		ok = LoadScaled(GroundFiles[m], GroundArt[m], false, GroundPercent[m]) && ok;
	for (int t = 1; t < TreeKindCount; t++) {
		ok = LoadScaled(TreeFiles[t].path, TreeArt[t], true, TreePercent) && ok;
		TreeArt[t].footX = TreeFiles[t].footX * TreePercent / 100;
		TreeArt[t].footY = TreeFiles[t].footY * TreePercent / 100;
	}
	ok = LoadScaled("levels\\woods\\scatter_sheet.png", ScatterArt, true, ScatterPercent) && ok;
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

uint32_t Shade(uint32_t c, int brightness)
{
	const uint32_t r = std::max<uint32_t>(1, (((c >> 16) & 0xFF) * brightness) >> 8);
	const uint32_t g = std::max<uint32_t>(1, (((c >> 8) & 0xFF) * brightness) >> 8);
	const uint32_t b = std::max<uint32_t>(1, ((c & 0xFF) * brightness) >> 8);
	return (r << 16) | (g << 8) | b;
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
			if (x < 16 || y < 16 || x >= 96 || y >= 96) {
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
		if (mx < 0 || my < 0 || mx >= DMAXX || my >= DMAXY)
			return false;
		for (int t = 0; t < 4; t++) {
			const int x = 16 + 2 * mx + (t & 1);
			const int y = 16 + 2 * my + (t >> 1);
			if (Tree[x][y] == NoTree && Ground[x][y] == Path)
				return true;
		}
		return false;
	};
	for (int my = 0; my < DMAXY; my++) {
		for (int mx = 0; mx < DMAXX; mx++) {
			int trees = 0;
			int dry = 0;
			for (int t = 0; t < 4; t++) {
				const int x = 16 + 2 * mx + (t & 1);
				const int y = 16 + 2 * my + (t >> 1);
				trees += Tree[x][y] != NoTree ? 1 : 0;
				dry += Ground[x][y] == GrassDry ? 1 : 0;
			}
			const uint32_t h = Hash(static_cast<uint32_t>(mx), static_cast<uint32_t>(my), seed ^ 0x7A11U);
			const int edge = std::min({ mx, my, DMAXX - 1 - mx, DMAXY - 1 - my });
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
			dungeon[mx][my] = static_cast<uint8_t>(std::min(paintedTrees && trees >= 3 ? VanillaRockMass : mega, 255));
			FillMegatile(16 + 2 * mx, 16 + 2 * my, mega);
			if (paintedTrees) {
				// A painted tree's tile blocks as a trunk does, and is drawn as the grass it stands on.
				for (int t = 0; t < 4; t++) {
					const int x = 16 + 2 * mx + (t & 1);
					const int y = 16 + 2 * my + (t >> 1);
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
			if (Tree[x][y] != NoTree)
				dPiece[x][y] = SolidPiece;
		}
	}
	// The minimap's map (automap.cpp reads dungeon[][]): each megatile 1 open ground, 2 the forest's edge, 3 road, 4 deep
	// forest. Before this the woods kept Tristram's dungeon[][] and the minimap drew the town (the play test, 2026-10-13).
	for (int my = 0; my < DMAXY; my++) {
		for (int mx = 0; mx < DMAXX; mx++) {
			int trees = 0;
			bool road = false;
			for (int t = 0; t < 4; t++) {
				const int x = 16 + 2 * mx + (t & 1);
				const int y = 16 + 2 * my + (t >> 1);
				trees += Tree[x][y] != NoTree ? 1 : 0;
				road = road || (Tree[x][y] == NoTree && Ground[x][y] == Path);
			}
			uint8_t kind = trees >= 3 ? 4 : road ? 3 : 1;
			if (kind == 4) {
				// The edge: deep forest beside open ground.
				for (int d = 0; d < 4 && kind == 4; d++) {
					const int nx = mx + (d == 0 ? 1 : d == 1 ? -1 : 0);
					const int ny = my + (d == 2 ? 1 : d == 3 ? -1 : 0);
					if (nx < 0 || ny < 0 || nx >= DMAXX || ny >= DMAXY)
						continue;
					int open = 0;
					for (int t = 0; t < 4; t++)
						open += Tree[16 + 2 * nx + (t & 1)][16 + 2 * ny + (t >> 1)] == NoTree ? 1 : 0;
					if (open >= 2)
						kind = 2;
				}
			}
			dungeon[mx][my] = kind;
		}
	}
	// Outdoors under the sky: the ground is lit all round, not dark beyond the hero's torch (Tristram's light is 0).
	std::memset(dLight, 3, sizeof(dLight));
	ViewPosition = Arrival;
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
	wx0 = 16 + 2 * house.x0;
	wy0 = 16 + 2 * house.y0;
	wx1 = 16 + 2 * house.x1;
	wy1 = 16 + 2 * house.y1;
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
			const Point at { 16 + 2 * house.x0 + 2 + static_cast<int>(Hash(static_cast<uint32_t>(i), static_cast<uint32_t>(k), 0x100DU) % static_cast<uint32_t>(2 * (house.x1 - house.x0) - 3)),
				16 + 2 * house.y0 + 2 + static_cast<int>(Hash(static_cast<uint32_t>(k), static_cast<uint32_t>(i), 0x100EU) % static_cast<uint32_t>(2 * (house.y1 - house.y0) - 3)) };
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
	return { 16 + 2 * house.x1 + 1, 16 + 2 * house.doorY + 1 }; // outside the east arch
}

Point WoodsHouseInsideForTest(int index)
{
	if (index < 0 || index >= HouseCount)
		return { 0, 0 };
	const House &house = Houses[index];
	return { 16 + 2 * house.x1 - 1, 16 + 2 * house.doorY + 1 }; // inside it
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

bool WoodsAcceptsMonster(const MonsterData &data)
{
	if (data.availability == MonsterAvailability::Never)
		return false;
	if (gbIsSpawn && data.availability == MonsterAvailability::Retail)
		return false;
	if (&data == &MonstersData[MT_HORKSPWN] || &data == &MonstersData[MT_GOLEM])
		return false;
	// Not the quest and story bosses, and nothing that lives only as something else's summon.
	for (const _monster_id boss : { MT_DIABLO, MT_NAKRUL, MT_SKING, MT_CLEAVER, MT_DEFILER }) {
		if (&data == &MonstersData[boss])
			return false;
	}
	const int floor = WoodsBandFloor();
	if (floor >= data.minDunLvl && floor <= data.maxDunLvl)
		return true;
	// The Hive's and the Crypt's monsters stand on the Caves' and Hell's rungs (area_level.cpp), as in a rift.
	const int twin = floor + 2 * FloorsPerArea;
	return floor > 2 * FloorsPerArea && twin >= data.minDunLvl && twin <= data.maxDunLvl;
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

void DrawWoodsGroundTile(const Surface &out, Point tile, Point target, int light, bool fullBright)
{
	const int brightness = (fullBright && light < LightsMax) || light < 0 ? 256 : BrightnessFor(light);
	// The tile's diamond: 31 rows ending on the tile's bottom line, as dun_render draws a floor's two triangles.
	const int originX = (tile.x - tile.y) * 32; // where the tile lies in the world, in pixels
	const int originY = (tile.x + tile.y) * 16;
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

} // namespace

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
		if (MyPlayer != nullptr && MyPlayer->isOnActiveLevel()) {
			const Point hero = MyPlayer->position.tile;
			if (hero.x + hero.y < tile.x + tile.y) {
				const int heroX = footX + ((hero.x - tile.x) - (hero.y - tile.y)) * 32;
				const int heroY = footY + ((hero.x - tile.x) + (hero.y - tile.y)) * 16 - 36;
				if (heroX > left - 40 && heroX < left + pic.w + 40 && heroY > top - 40 && heroY < top + pic.h + 40)
					hole = { heroX, heroY, 70 };
			}
		}
		DrawPicture(out, pic, left, top, 0, 0, pic.w, pic.h, brightness, hole);
		return;
	}
	if (const uint8_t cell = Scatter[tile.x][tile.y]; cell != 0 && !ScatterArt.px.empty()) {
		const int index = cell - 1;
		// A cell of the 4 x 3 sheet at its scale; its item stands at the cell's bottom centre.
		const int size = ScatterArt.w / 4;
		DrawPicture(out, ScatterArt, footX - size / 2, footY - size * 7 / 8, (index % 4) * size, (index / 4) * size, size, size, brightness);
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

void LoadWoodsArtForTest()
{
	ArtTried = false;
	LoadArt();
	BuildEdgeNoise();
}

} // namespace devilution::oracool
