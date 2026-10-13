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
#include <vector>

#include <SDL.h>

#include "diablo.h"
#include "engine/palette.h"
#include "engine/random.hpp"
#include "engine/surface.hpp"
#include "interfac.h"
#include "levels/gendung.h"
#include "levels/trigs.h"
#include "lighting.h"
#include "oracool/colour_light.h"
#include "player.h"
#include "plrmsg.h"
#include "quests.h"
#include "utils/log.hpp"
#include "utils/png.h"

namespace devilution::oracool {

namespace {

// ---------------------------------------------------------------- the per-game state

constexpr int WoodsFloors = SL_LAST_WOODS - SL_FIRST_WOODS + 1;

/** @brief One game's woods: each floor's seed, rolled on its first visit and kept for the rest of the game (D4). */
struct WoodsGame {
	std::array<uint32_t, WoodsFloors> seed {};
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

Point Arrival { 88, 48 };
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
	const float u = Unit(h);
	if (ground == Forest)
		return u < 0.30F ? OakLarge : u < 0.52F ? PineTall : u < 0.70F ? OakSmall : u < 0.84F ? PineSmall : u < 0.94F ? DeadLarge : DeadSmall;
	return u < 0.30F ? OakSmall : u < 0.55F ? PineSmall : u < 0.75F ? OakLarge : u < 0.88F ? DeadSmall : u < 0.95F ? PineTall : DeadLarge;
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
			Ground[x][y] = moist > 0.64F ? GrassDark : moist < 0.32F ? GrassDry : Grass;
			const float grove = ValueNoise(x, y, 8.0F, seed ^ 0x6A7EU);
			const int edge = std::min({ x - Lo, y - Lo, Hi - x, Hi - y });
			const float borderDepth = 2.0F + 4.0F * ValueNoise(x, y, 5.0F, seed ^ 0xB0DEU);
			const uint32_t h = Hash(static_cast<uint32_t>(x), static_cast<uint32_t>(y), seed ^ 0x7EE5U);
			if (static_cast<float>(edge) < borderDepth) {
				Ground[x][y] = Forest;
				Tree[x][y] = PickTree(Forest, h);
			} else if (grove > 0.60F) {
				Ground[x][y] = Forest;
				if (Unit(h) < 0.22F) // a grove you can see into (the play test: at 0.42 the canopies were one carpet)
					Tree[x][y] = PickTree(Forest, h >> 3);
			} else if (Unit(h) < 0.025F) {
				Tree[x][y] = PickTree(static_cast<Material>(Ground[x][y]), h >> 3); // a lone tree in the grass
			}
		}
	}

	LayPath(Arrival, WayOn, 3, seed ^ 0x9A7EU);
	// A side path to a clearing, so the floor is more than one road.
	const Point glade { Lo + span / 2 + static_cast<int>((Unit(Hash(seed, 3, 0)) - 0.5F) * 20), Lo + span / 2 + static_cast<int>((Unit(Hash(seed, 4, 0)) - 0.5F) * 30) };
	Clearing(glade, 6, Grass);
	LayPath(glade, { (Arrival.x + WayOn.x) / 2, (Arrival.y + WayOn.y) / 2 }, 2, seed ^ 0x51DEU);
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
}

// ---------------------------------------------------------------- the art

struct Picture {
	int w = 0;
	int h = 0;
	std::vector<uint32_t> px; // 0x00RRGGBB, with the top byte 0xFF where opaque in a sprite
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

/** @brief A PNG from the archives as RGBA bytes, halved (the art is drawn at twice the game's size, D6). */
bool LoadHalved(const char *path, Picture &out, bool sprite)
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
	out.w = rgba->w / 2;
	out.h = rgba->h / 2;
	out.px.assign(static_cast<size_t>(out.w) * out.h, 0);
	const auto *bytes = static_cast<const uint8_t *>(rgba->pixels);
	for (int y = 0; y < out.h; y++) {
		for (int x = 0; x < out.w; x++) {
			int r = 0, g = 0, b = 0, n = 0;
			for (int sy = 0; sy < 2; sy++) {
				for (int sx = 0; sx < 2; sx++) {
					const uint8_t *q = bytes + static_cast<size_t>(2 * y + sy) * rgba->pitch + static_cast<size_t>(2 * x + sx) * 4;
					if (sprite && q[3] < 128)
						continue;
					r += q[0];
					g += q[1];
					b += q[2];
					n++;
				}
			}
			// A sprite pixel is opaque where most of its four were; the ground is always opaque.
			if (n == 0 || (sprite && n < 2))
				continue;
			out.px[static_cast<size_t>(y) * out.w + x] = 0xFF000000U | (static_cast<uint32_t>(r / n) << 16) | (static_cast<uint32_t>(g / n) << 8) | static_cast<uint32_t>(b / n);
		}
	}
	SDL_FreeSurface(rgba);
	return true;
}

void LoadArt()
{
	if (ArtTried)
		return;
	ArtTried = true;
	bool ok = true;
	for (int m = 0; m < MaterialCount; m++)
		ok = LoadHalved(GroundFiles[m], GroundArt[m], false) && ok;
	for (int t = 1; t < TreeKindCount; t++) {
		ok = LoadHalved(TreeFiles[t].path, TreeArt[t], true) && ok;
		TreeArt[t].footX = TreeFiles[t].footX / 2;
		TreeArt[t].footY = TreeFiles[t].footY / 2;
	}
	ok = LoadHalved("levels\\woods\\scatter_sheet.png", ScatterArt, true) && ok;
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
}

void CreateWoods(uint32_t seed, lvl_entry /*entry*/)
{
	// The tile data before the generator: it picks its solid piece from it (LoadGameLevel loads it after LoadSetMap).
	LoadLevelSOLData();
	FindSolidPiece();
	Generate(seed);
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
	(void)fresh;
	const int floor = FloorIndex(setlvlnum);
	if (Game.seed[floor] == 0)
		Game.seed[floor] = (static_cast<uint32_t>(AdvanceRndSeed()) ^ (SDL_GetTicks() * 2654435761U)) | 1U;
	CreateDungeon(Game.seed[floor], ENTRY_SETLVL);
	InitNoTriggers();
	LoadRndLvlPal(leveltype);
	LoadLevelSOLData();
	// No pack on the hero's landing (no monsters yet in milestone 1; the flag is what the scatter will read).
	for (int dy = -3; dy <= 3; dy++) {
		for (int dx = -3; dx <= 3; dx++) {
			const Point tile = Arrival + Displacement { dx, dy };
			if (InDungeonBounds(tile))
				dFlags[tile.x][tile.y] |= DungeonFlag::Populated;
		}
	}
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
	// The road back: the last tiles of the arrival road, at the map's edge.
	if (tile.x >= Hi - 1 && std::abs(tile.y - Arrival.y) <= 1) {
		StartNewLvl(player, WM_DIABRTNLVL, GetMapReturnLevel());
		return true;
	}
	const bool atWayOn = tile.x <= Lo + 1 && std::abs(tile.y - WayOn.y) <= 1;
	if (atWayOn && !AtWayOnBefore)
		EventPlrMsg("The path on into the woods is not open yet.", UiFlags::ColorWhite);
	AtWayOnBefore = atWayOn;
	return false;
}

const char *WoodsFloorName(_setlevels level)
{
	constexpr const char *Names[WoodsFloors] = { "The Greywood Verge", "Thornmoor", "The Hanging Wood", "Witchbark Hollow" };
	return Names[FloorIndex(level)];
}

bool WoodsArtActive(const Surface &out)
{
	return leveltype == DTYPE_WOODS && ArtLoaded && !out.isIndexed();
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
	for (int y = 0; y < h; y++) {
		const int sy = top + y;
		if (sy < 0 || sy >= out.h())
			continue;
		uint32_t *row = out.at<uint32_t>(0, sy);
		const uint32_t *src = &pic.px[static_cast<size_t>(srcY + y) * pic.w + srcX];
		for (int x = 0; x < w; x++) {
			const int sx = left + x;
			if (sx < 0 || sx >= out.w())
				continue;
			const uint32_t c = src[x];
			if ((c & 0xFF000000U) == 0)
				continue;
			const uint32_t lit = Shade(c, brightness);
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
		// A 32 x 32 cell after halving; its item stands at the cell's bottom centre.
		DrawPicture(out, ScatterArt, footX - 16, footY - 28, (index % 4) * 32, (index / 4) * 32, 32, 32, brightness);
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
		open = tile.x >= Hi - 1 && std::abs(tile.y - Arrival.y) <= 1;
		closed = tile.x <= Lo + 1 && std::abs(tile.y - WayOn.y) <= 1;
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

void LoadWoodsArtForTest()
{
	ArtTried = false;
	LoadArt();
	BuildEdgeNoise();
}

} // namespace devilution::oracool
