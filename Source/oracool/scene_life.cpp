/**
 * @file oracool/scene_life.cpp
 *
 * The fourth graphics batch (oracool/scene_life.h). Every effect here is visual only: it reads the game, never writes it.
 * Like scene_fx.cpp, the pixel loops do their channel arithmetic in macros (the Debug build is compiled with /Ob0).
 */
#include "oracool/scene_life.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "engine/clx_sprite.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/render/scrollrt.h"
#include "engine/render/text_render.hpp"
#include "engine/surface.hpp"
#include "items.h"
#include "levels/gendung.h"
#include "lighting.h"
#include "missiles.h"
#include "monster.h"
#include "objects.h"
#include "options.h"
#include "oracool/colour_light.h"
#include "oracool/ground_fx.h"
#include "oracool/particles.h"
#include "oracool/rift.h"
#include "oracool/rift_bosses.h"
#include "oracool/scene_fx.h"
#include "oracool/splats.h"
#include "oracool/woods.h" // OnDesertFloor - no fireflies over the desert
#include "player.h"

#ifdef _MSC_VER
#pragma runtime_checks("", off)
#pragma optimize("gt", on)
#endif

#define SL_R(c) (static_cast<int>(((c) >> 16) & 0xFF))
#define SL_G(c) (static_cast<int>(((c) >> 8) & 0xFF))
#define SL_B(c) (static_cast<int>((c) & 0xFF))
#define SL_PACK(r, g, b) ((static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b))
#define SL_MIX(c, r, g, b, a) \
	SL_PACK(SL_R(c) + ((((r)-SL_R(c)) * (a)) >> 8), SL_G(c) + ((((g)-SL_G(c)) * (a)) >> 8), SL_B(c) + ((((b)-SL_B(c)) * (a)) >> 8))
#define SL_SCALE(c, k) \
	((((((c)&0xFF00FFU) * static_cast<uint32_t>(k)) >> 8) & 0xFF00FFU) | (((((c)&0x00FF00U) * static_cast<uint32_t>(k)) >> 8) & 0x00FF00U))
#define SL_SAT(v) ((v) > 255 ? 255 : (v))
#define SL_ADD(c, r, g, b) SL_PACK(SL_SAT(SL_R(c) + (r)), SL_SAT(SL_G(c) + (g)), SL_SAT(SL_B(c) + (b)))

namespace devilution::oracool {

namespace {

constexpr float Pi = 3.14159265F;

// ---- Small tools ------------------------------------------------------------------------------------------------------

uint32_t Hash(uint32_t a, uint32_t b, uint32_t c = 0)
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

float Clamp01(float v)
{
	return v < 0 ? 0 : (v > 1 ? 1 : v);
}

float Smooth(float v)
{
	v = Clamp01(v);
	return v * v * (3.0F - 2.0F * v);
}

float Frac(float v)
{
	return v - std::floor(v);
}

float Now()
{
	return SceneSeconds();
}

uint32_t Rng = 0x2545F491U;

float Rand()
{
	Rng ^= Rng << 13;
	Rng ^= Rng >> 17;
	Rng ^= Rng << 5;
	return Unit(Rng);
}

uint32_t LevelStamp()
{
	return static_cast<uint32_t>(leveltype) | (static_cast<uint32_t>(currlevel) << 8) | (static_cast<uint32_t>(setlevel ? 1 : 0) << 16)
	    | (static_cast<uint32_t>(setlvlnum) << 24);
}

/** A tile's middle in world pixels: the isometric step, 64x32. */
float WorldX(Point tile)
{
	return static_cast<float>(tile.x - tile.y) * 32.0F;
}

float WorldY(Point tile)
{
	return static_cast<float>(tile.x + tile.y) * 16.0F;
}

/** The tile under a world pixel. */
Point TileAtWorld(float wx, float wy)
{
	const float u = wx / 32.0F;
	const float v = wy / 16.0F;
	return { static_cast<int>(std::floor((u + v) * 0.5F + 0.5F)), static_cast<int>(std::floor((v - u) * 0.5F + 0.5F)) };
}

bool InMap(Point tile)
{
	return tile.x >= 0 && tile.y >= 0 && tile.x < MAXDUNX && tile.y < MAXDUNY;
}

bool Walkable(Point tile)
{
	return InMap(tile) && !TileHasAny(dPiece[tile.x][tile.y], TileProperties::Solid);
}

/** Where the world lands on a surface: screen = world + (ox, oy). */
struct View {
	float ox = 0;
	float oy = 0;
	int w = 0;
	int h = 0;
};

View MakeView(const Surface &out, Point firstTile, Point firstTileBuffer)
{
	View v;
	v.ox = static_cast<float>(firstTileBuffer.x) + 32.0F - static_cast<float>(firstTile.x - firstTile.y) * 32.0F;
	v.oy = static_cast<float>(firstTileBuffer.y) - 14.5F - static_cast<float>(firstTile.x + firstTile.y) * 16.0F;
	v.w = out.w();
	v.h = out.h();
	return v;
}

/** The tiles a view can show, with a margin for what stands up out of them. */
void ViewTiles(const View &v, int &x0, int &x1, int &y0, int &y1)
{
	float minX = 1e9F, maxX = -1e9F, minY = 1e9F, maxY = -1e9F;
	const float cornersX[4] = { -96.0F, static_cast<float>(v.w) + 96.0F, -96.0F, static_cast<float>(v.w) + 96.0F };
	const float cornersY[4] = { -64.0F, -64.0F, static_cast<float>(v.h) + 160.0F, static_cast<float>(v.h) + 160.0F };
	for (int i = 0; i < 4; i++) {
		const float u = (cornersX[i] - v.ox) / 32.0F;
		const float w = (cornersY[i] - v.oy) / 16.0F;
		const float tx = (u + w) * 0.5F;
		const float ty = (w - u) * 0.5F;
		minX = std::min(minX, tx);
		maxX = std::max(maxX, tx);
		minY = std::min(minY, ty);
		maxY = std::max(maxY, ty);
	}
	x0 = std::max(0, static_cast<int>(std::floor(minX)));
	x1 = std::min(MAXDUNX - 1, static_cast<int>(std::ceil(maxX)));
	y0 = std::max(0, static_cast<int>(std::floor(minY)));
	y1 = std::min(MAXDUNY - 1, static_cast<int>(std::ceil(maxY)));
}

/** Seen on the screen now: lit in the dungeon, anywhere in town. */
bool TileShows(Point tile)
{
	return InMap(tile) && (leveltype == DTYPE_TOWN || dLight[tile.x][tile.y] < LightsMax);
}

/** A 64x64 tile of value noise, wrapping, sampled bilinearly (as the ground fog's). */
float TableNoise(float x, float y)
{
	static std::array<float, 64 * 64> Table;
	static bool Built = false;
	if (!Built) {
		for (int i = 0; i < 64 * 64; i++)
			Table[static_cast<size_t>(i)] = Unit(Hash(static_cast<uint32_t>(i & 63), static_cast<uint32_t>(i >> 6), 4077));
		Built = true;
	}
	const float fx = std::floor(x);
	const float fy = std::floor(y);
	const int ix = static_cast<int>(fx) & 63;
	const int iy = static_cast<int>(fy) & 63;
	float tx = x - fx;
	float ty = y - fy;
	tx = tx * tx * (3.0F - 2.0F * tx);
	ty = ty * ty * (3.0F - 2.0F * ty);
	const float *t = Table.data();
	const float a = t[iy * 64 + ix], b = t[iy * 64 + ((ix + 1) & 63)];
	const float c = t[((iy + 1) & 63) * 64 + ix], d = t[((iy + 1) & 63) * 64 + ((ix + 1) & 63)];
	const float top = a + (b - a) * tx;
	return top + ((c + (d - c) * tx) - top) * ty;
}

/** How much of the day's light Tristram has (1 at noon .. 0 at night), as the third batch's sun reads it. */
float Daylight()
{
	if (TownIsDark())
		return 0;
	if (!*sgOptions.Oracool.dayNightCycle)
		return 1;
	const LightTint now = TownAmbientAt(TownHour());
	const LightTint noon = TownAmbientAt(12.0F);
	const float a = static_cast<float>(now.r + now.g + now.b);
	const float b = static_cast<float>(noon.r + noon.g + noon.b);
	return b > 0 ? Smooth((a / b - 0.9F) / 0.1F) : 1.0F;
}

/** The weather that shows now and how far it has set in (0 with the option off). */
Weather ShownWeather(float &intensity)
{
	intensity = 0;
	if (!*sgOptions.Oracool.weather)
		return Weather::Clear;
	return CurrentWeather(&intensity);
}

/** Light added at a pixel - or a splat when the card is collecting them (as the loot beams do). */
struct LightPen {
	const Surface *out = nullptr;
	std::vector<Splat> *splats = nullptr;
	int w = 0;
	int h = 0;

	void add(int px, int py, int r, int g, int b) const
	{
		if (px < 0 || py < 0 || px >= w || py >= h || (r | g | b) <= 0)
			return;
		r = SL_SAT(r);
		g = SL_SAT(g);
		b = SL_SAT(b);
		if (splats != nullptr) {
			splats->push_back({ static_cast<float>(px), static_cast<float>(py), static_cast<float>(r) / 255.0F, static_cast<float>(g) / 255.0F,
			    static_cast<float>(b) / 255.0F, 0.0F });
			return;
		}
		uint32_t &c = *out->at<uint32_t>(px, py);
		c = SL_ADD(c, r, g, b);
	}
	void addF(int px, int py, uint32_t rgb, float k) const
	{
		if (k <= 0.004F)
			return;
		add(px, py, static_cast<int>(static_cast<float>(SL_R(rgb)) * k), static_cast<int>(static_cast<float>(SL_G(rgb)) * k), static_cast<int>(static_cast<float>(SL_B(rgb)) * k));
	}
};

LightPen MakePen(const Surface &out)
{
	LightPen pen;
	pen.out = &out;
	pen.splats = SplatCollection();
	pen.w = out.w();
	pen.h = out.h();
	return pen;
}

/** Mixes a pixel toward a colour by @p a/256, in the bounds. */
void Blend(const Surface &out, int x, int y, uint32_t rgb, int a)
{
	if (x < 0 || y < 0 || x >= out.w() || y >= out.h() || a <= 0)
		return;
	a = a > 256 ? 256 : a;
	uint32_t &c = *out.at<uint32_t>(x, y);
	c = SL_MIX(c, SL_R(rgb), SL_G(rgb), SL_B(rgb), a);
}

// ---- The world's place on the screen, once a frame ---------------------------------------------------------------------

struct ScreenMap {
	bool valid = false;
	View view;
	int regionX = 0; // the pre-zoom surface's left edge in the full screen
	float zoom = 1;
};
ScreenMap Map;

float LastTick = -1;

// ---- Wet ground ---------------------------------------------------------------------------------------------------------

float Wetness = 0;

void TickWetness(float dt)
{
	float intensity = 0;
	const Weather w = ShownWeather(intensity);
	if (w == Weather::Rain)
		Wetness = std::min(1.0F, Wetness + dt * intensity / 40.0F);
	else
		Wetness = std::max(0.0F, Wetness - dt / (w == Weather::Snow ? 60.0F : 150.0F));
}

struct PuddleShape {
	float rx, ry;     // the main pool's radii at full size
	float ox, oy;     // its middle off the tile's middle
	float lobeX, lobeY; // the second, smaller pool's middle off the first's (in radii)
};

bool PuddleAt(Point tile, PuddleShape &s)
{
	if (!InMap(tile))
		return false;
	const uint32_t h = Hash(static_cast<uint32_t>(tile.x), static_cast<uint32_t>(tile.y), 501);
	if (h % 6 != 0)
		return false;
	// Not at a wall's foot: the tile and its four neighbours are open ground.
	if (!Walkable(tile) || !Walkable(tile + Displacement { 1, 0 }) || !Walkable(tile + Displacement { -1, 0 }) || !Walkable(tile + Displacement { 0, 1 })
	    || !Walkable(tile + Displacement { 0, -1 }))
		return false;
	s.rx = 9.0F + static_cast<float>((h >> 8) % 13);
	s.ry = s.rx * 0.45F;
	s.ox = static_cast<float>(static_cast<int>((h >> 13) % 17) - 8);
	s.oy = static_cast<float>(static_cast<int>((h >> 18) % 9) - 4);
	s.lobeX = ((h >> 22) & 1) != 0 ? 0.6F : -0.6F;
	s.lobeY = 0.25F + 0.2F * Unit(h >> 3);
	return true;
}

// ---- Footprints ---------------------------------------------------------------------------------------------------------

struct Print {
	float x, y;   // world pixels
	float nx, ny; // the step's direction on the floor
	float born;
	bool snow;
};
constexpr size_t MaxPrints = 256;
constexpr float PrintLife = 30.0F;
std::array<Print, MaxPrints> Prints;
size_t PrintHead = 0;
size_t PrintCount = 0;
float LastPrintX = 0;
float LastPrintY = 0;
bool HavePrintPlace = false;
bool PrintLeft = false;

Displacement WalkOffset(const Player &player)
{
	return player.isWalking() ? GetOffsetForWalking(player.AnimInfo, player._pdir) : Displacement {};
}

void TickFootprints()
{
	if (!*sgOptions.Oracool.footprints || !IsUnderSky(leveltype) || MyPlayer == nullptr) {
		HavePrintPlace = false;
		return;
	}
	float snow = 0;
	const Weather w = ShownWeather(snow);
	const bool snowy = w == Weather::Snow && snow > 0.25F;
	const bool wet = Wetness > 0.25F && !OnDesertFloor(); // the woods' rain does not wet the desert's sand
	const Player &player = *MyPlayer;
	const Displacement walk = WalkOffset(player);
	const float x = WorldX(player.position.tile) + static_cast<float>(walk.deltaX);
	const float y = WorldY(player.position.tile) + static_cast<float>(walk.deltaY);
	if (!HavePrintPlace) {
		LastPrintX = x;
		LastPrintY = y;
		HavePrintPlace = true;
		return;
	}
	const float dx = x - LastPrintX;
	const float dy = (y - LastPrintY) * 2.0F; // on the floor, not the screen
	const float d = std::sqrt(dx * dx + dy * dy);
	if (d > 160.0F || (!snowy && !wet)) {
		// A teleport, or no ground to print on: start again from here.
		LastPrintX = x;
		LastPrintY = y;
		return;
	}
	if (d < 14.0F)
		return;
	const float nx = dx / d;
	const float ny = dy / d;
	const float side = PrintLeft ? 3.5F : -3.5F;
	PrintLeft = !PrintLeft;
	Print &p = Prints[PrintHead];
	p.x = x - ny * side;
	p.y = y + nx * side * 0.5F;
	p.nx = nx;
	p.ny = ny;
	p.born = Now();
	p.snow = snowy;
	PrintHead = (PrintHead + 1) % MaxPrints;
	PrintCount = std::min(PrintCount + 1, MaxPrints);
	LastPrintX = x;
	LastPrintY = y;
}

// ---- Crows --------------------------------------------------------------------------------------------------------------

struct Crow {
	bool alive = false;
	bool flying = false;
	bool faceLeft = false;
	float x = 0, y = 0, z = 0; // world pixels; z above the ground
	float vx = 0, vy = 0, vz = 0;
	float since = 0;
	float nextHop = 0;
	uint32_t seed = 0;
};
constexpr int CrowCount = 5;
std::array<Crow, CrowCount> Crows;
std::array<float, CrowCount> CrowBack {};

bool CrowsOut()
{
	if (leveltype != DTYPE_TOWN || !*sgOptions.Oracool.townLife || Daylight() < 0.3F)
		return false;
	float intensity = 0;
	return ShownWeather(intensity) == Weather::Clear || intensity < 0.3F;
}

void TakeOff(Crow &crow, float fromX, float fromY)
{
	float dx = crow.x - fromX;
	float dy = (crow.y - fromY) * 2.0F;
	const float d = std::sqrt(dx * dx + dy * dy);
	if (d < 1.0F) {
		dx = Rand() - 0.5F;
		dy = 1.0F;
	} else {
		dx /= d;
		dy /= d;
	}
	crow.flying = true;
	crow.since = Now();
	crow.vx = dx * 95.0F + (Rand() - 0.5F) * 30.0F;
	crow.vy = dy * 45.0F + (Rand() - 0.5F) * 15.0F;
	crow.vz = 55.0F + Rand() * 20.0F;
	crow.faceLeft = crow.vx < 0;
}

void TickCrows(float dt)
{
	const float now = Now();
	const bool out = CrowsOut();
	if (leveltype != DTYPE_TOWN || MyPlayer == nullptr) {
		for (Crow &crow : Crows)
			crow.alive = false;
		return;
	}
	const Point hero = MyPlayer->position.tile;
	const Displacement walk = WalkOffset(*MyPlayer);
	const float heroX = WorldX(hero) + static_cast<float>(walk.deltaX);
	const float heroY = WorldY(hero) + static_cast<float>(walk.deltaY);
	for (int i = 0; i < CrowCount; i++) {
		Crow &crow = Crows[static_cast<size_t>(i)];
		if (!crow.alive) {
			if (!out || now < CrowBack[static_cast<size_t>(i)])
				continue;
			// Land somewhere out of the hero's sight for now, so none pops into view.
			for (int tries = 0; tries < 24; tries++) {
				const Point tile = hero + Displacement { static_cast<int>(Rand() * 33.0F) - 16, static_cast<int>(Rand() * 33.0F) - 16 };
				const int far = std::max(std::abs(tile.x - hero.x), std::abs(tile.y - hero.y));
				if (far < 11 || !Walkable(tile) || dMonster[tile.x][tile.y] != 0 || dObject[tile.x][tile.y] != 0)
					continue;
				// Out of sight on the screen itself, not only by tiles (audit: eleven tiles east is still in a 960 view).
				const float sx = WorldX(tile) + Map.view.ox;
				const float sy = WorldY(tile) + Map.view.oy;
				if (Map.valid && sx > -48.0F && sx < static_cast<float>(Map.view.w) + 48.0F && sy > -24.0F && sy < static_cast<float>(Map.view.h) + 48.0F)
					continue;
				crow = {};
				crow.alive = true;
				crow.x = WorldX(tile) + (Rand() - 0.5F) * 20.0F;
				crow.y = WorldY(tile) + (Rand() - 0.5F) * 8.0F;
				crow.faceLeft = Rand() < 0.5F;
				crow.nextHop = now + 1.0F + Rand() * 3.0F;
				crow.seed = static_cast<uint32_t>(Rand() * 16777215.0F);
				break;
			}
			continue;
		}
		if (!crow.flying) {
			// Off when the hero (or anyone) comes within three tiles, or when the light goes.
			bool startled = !out;
			const Point at = TileAtWorld(crow.x, crow.y);
			for (const Player &player : Players) {
				if (!player.plractive || !player.isOnActiveLevel())
					continue;
				const Point p = player.position.tile;
				if (std::max(std::abs(p.x - at.x), std::abs(p.y - at.y)) <= 3)
					startled = true;
			}
			for (int dx = -1; dx <= 1 && !startled; dx++)
				for (int dy = -1; dy <= 1 && !startled; dy++)
					startled = InMap(at + Displacement { dx, dy }) && dMonster[at.x + dx][at.y + dy] != 0; // a townsman walking by
			if (startled) {
				TakeOff(crow, heroX, heroY);
				continue;
			}
			if (now >= crow.nextHop) {
				const float nx = crow.x + (Rand() - 0.5F) * 12.0F;
				const float ny = crow.y + (Rand() - 0.5F) * 6.0F;
				if (Walkable(TileAtWorld(nx, ny))) {
					crow.faceLeft = nx < crow.x;
					crow.x = nx;
					crow.y = ny;
				}
				crow.nextHop = now + 1.5F + Rand() * 4.0F;
			}
			continue;
		}
		crow.x += crow.vx * dt;
		crow.y += crow.vy * dt;
		crow.z += crow.vz * dt;
		crow.vz = std::min(crow.vz + 12.0F * dt, 95.0F);
		if (now - crow.since > 5.0F || now < crow.since) {
			crow.alive = false;
			CrowBack[static_cast<size_t>(i)] = now + 25.0F + Rand() * 40.0F;
		}
	}
}

// Tiny drawings of a crow, rows top to bottom, the last row on the ground: X body, B beak, L legs.
constexpr const char *CrowStand[] = { "....XX.", "XXXXXXB", ".XXXXX.", "..L.L.." };
constexpr const char *CrowPeck[] = { ".......", "XXXX...", ".XXXXX.", "..L.LXB" };
constexpr const char *CrowWingsUp[] = { "X.......X", ".X.....X.", "..XXXXX..", "....X...." };
constexpr const char *CrowWingsDown[] = { "....X....", "..XXXXX..", ".X.....X.", "X.......X" };

void DrawCrowShape(const Surface &out, const char *const *rows, int width, int x, int bottom, bool mirror)
{
	for (int r = 0; r < 4; r++) {
		const int y = bottom - 3 + r;
		for (int c = 0; c < width; c++) {
			const char k = rows[r][mirror ? width - 1 - c : c];
			if (k == '.')
				continue;
			const uint32_t rgb = k == 'X' ? SL_PACK(24, 22, 30) : SL_PACK(78, 70, 58);
			Blend(out, x - width / 2 + c, y, rgb, 236);
		}
	}
}

// ---- Torch smoke --------------------------------------------------------------------------------------------------------

bool Smokes(const Object &object)
{
	switch (object._otype) {
	case OBJ_TORCHL:
	case OBJ_TORCHR:
	case OBJ_TORCHL2:
	case OBJ_TORCHR2:
	case OBJ_CANDLE1:
	case OBJ_CANDLE2:
	case OBJ_CANDLEO:
	case OBJ_BCROSS:
		return true;
	default:
		return false;
	}
}

/** The top of a sprite's flame: its topmost drawn pixel, as (x, rows from the top); found once per frame of art. */
std::unordered_map<const uint8_t *, Point> Tips; // cleared with the level: a freed sheet's address can come back

bool FlameTip(ClxSprite sprite, Point &tip)
{
	const auto found = Tips.find(sprite.pixelData());
	if (found != Tips.end()) {
		tip = found->second;
		return tip.x >= 0;
	}
	const int w = sprite.width();
	const int h = sprite.height();
	Point result { -1, -1 };
	if (w > 0 && h > 0 && w <= 320 && h <= 320) {
		static std::optional<OwnedSurface> Scratch;
		if (!Scratch)
			Scratch.emplace(320, 320);
		for (int y = 0; y < h; y++)
			std::memset(Scratch->at(0, y), 0, static_cast<size_t>(w));
		ClxDraw(Scratch->subregion(0, 0, w, h), Point { 0, h - 1 }, sprite);
		for (int y = 0; y < h && result.x < 0; y++) {
			const uint8_t *row = Scratch->at(0, y);
			int first = -1, last = -1;
			for (int x = 0; x < w; x++) {
				if (row[x] != 0) {
					first = first < 0 ? x : first;
					last = x;
				}
			}
			if (first >= 0)
				result = { (first + last) / 2, y };
		}
	}
	if (Tips.size() > 512)
		Tips.clear();
	Tips[sprite.pixelData()] = result;
	tip = result;
	return result.x >= 0;
}

// ---- Shockwaves -----------------------------------------------------------------------------------------------------------

struct Wave {
	float x, y; // world pixels
	float born;
	float strength;
};
std::vector<Wave> Waves;
constexpr float WaveLife = 0.5F;

struct SeenMissile {
	const Missile *missile;
	MissileGraphicID graphic;
	MissileID type;
	Point start; // with the address: a new missile in a freed one's memory is told apart (audit)
};
std::vector<SeenMissile> Seen;
std::vector<SeenMissile> SeenNext;
bool SeenPrimed = false;

float WaveForType(MissileID type)
{
	switch (type) {
	case MissileID::Nova:
	case MissileID::MeteorImpact:
	case MissileID::BigExplosion:
		return 1.0F;
	case MissileID::FrostNova:
	case MissileID::CorpseBurst:
		return 0.9F;
	case MissileID::WarcryRing:
		return 0.8F;
	case MissileID::DiabloApocalypseBoom:
		return 0.5F;
	default:
		return 0;
	}
}

float WaveForGraphic(MissileGraphicID graphic)
{
	switch (graphic) {
	case MissileGraphicID::BigExplosion: return 0.9F; // a fireball's burst
	case MissileGraphicID::MeteorImpact:
	case MissileGraphicID::DeathNova: return 1.0F;
	case MissileGraphicID::CorpseExplosion: return 0.9F;
	case MissileGraphicID::IceBlast:
	case MissileGraphicID::FreezingBurst:
	case MissileGraphicID::LightningStrikeImpact: return 0.6F;
	case MissileGraphicID::MagmaBallExplosion:
	case MissileGraphicID::GlacialShatter:
	case MissileGraphicID::DiabloApocalypseBoom: return 0.5F;
	case MissileGraphicID::HolyBoltExplosion:
	case MissileGraphicID::ApocalypseBoom: return 0.4F;
	case MissileGraphicID::BloodStarExplosion:
	case MissileGraphicID::BloodStarBlueExplosion:
	case MissileGraphicID::BloodStarYellowExplosion:
	case MissileGraphicID::BloodStarRedExplosion:
	case MissileGraphicID::YellowFlareExplosion:
	case MissileGraphicID::BlueFlareExplosion:
	case MissileGraphicID::RedFlareExplosion:
	case MissileGraphicID::OrangeFlareExplosion:
	case MissileGraphicID::BlueFlareExplosion2: return 0.35F;
	case MissileGraphicID::FireArrowExplosion: return 0.3F;
	default: return 0;
	}
}

void PushWave(float x, float y, float strength)
{
	if (Waves.size() >= 8)
		Waves.erase(Waves.begin());
	Waves.push_back({ x, y, Now(), strength });
}

void TickShockwaves()
{
	const float now = Now();
	Waves.erase(std::remove_if(Waves.begin(), Waves.end(), [now](const Wave &w) { return now - w.born > WaveLife || now < w.born; }), Waves.end());
	SeenNext.clear();
	for (const Missile &m : Missiles) {
		const SeenMissile key { &m, m._miAnimType, m._mitype, m.position.start };
		const auto at = std::lower_bound(Seen.begin(), Seen.end(), key, [](const SeenMissile &a, const SeenMissile &b) { return a.missile < b.missile; });
		const bool known = at != Seen.end() && at->missile == &m && at->type == m._mitype && at->start == m.position.start;
		float strength = 0;
		if (!known)
			strength = WaveForType(m._mitype);
		if (strength == 0 && (!known || at->graphic != m._miAnimType))
			strength = WaveForGraphic(m._miAnimType);
		if (SeenPrimed && strength > 0 && *sgOptions.Oracool.shockwaves && TileShows(m.position.tile))
			PushWave(WorldX(m.position.tile) + static_cast<float>(m.position.offset.deltaX), WorldY(m.position.tile) + static_cast<float>(m.position.offset.deltaY) - 10.0F, strength);
		SeenNext.push_back(key);
	}
	std::sort(SeenNext.begin(), SeenNext.end(), [](const SeenMissile &a, const SeenMissile &b) { return a.missile < b.missile; });
	std::swap(Seen, SeenNext);
	SeenPrimed = true;
}

// ---- Drop flares --------------------------------------------------------------------------------------------------------

struct DropWatch {
	uint32_t seed = 0;
	bool tumbling = false;
};
std::array<DropWatch, MAXITEMS + 1> Watch {};

struct Flare {
	Point tile;
	uint32_t rgb;
	float born;
};
std::vector<Flare> Flares;
constexpr float FlareLife = 1.2F;

void TickDropFlares()
{
	const float now = Now();
	Flares.erase(std::remove_if(Flares.begin(), Flares.end(), [now](const Flare &f) { return now - f.born > FlareLife || now < f.born; }), Flares.end());
	for (uint8_t i = 0; i < ActiveItemCount; i++) {
		const int index = ActiveItems[i];
		if (index < 0 || index > MAXITEMS)
			continue;
		const Item &item = Items[index];
		DropWatch &watch = Watch[static_cast<size_t>(index)];
		if (watch.seed != item._iSeed) {
			// Another item in the slot: one seen tumbling flares when it lands; one already lying there (a level's own) never.
			watch.seed = item._iSeed;
			watch.tumbling = item._iAnimFlag;
			continue;
		}
		if (item._iAnimFlag) {
			watch.tumbling = true;
			continue;
		}
		if (!watch.tumbling)
			continue;
		watch.tumbling = false;
		const LootLook look = LootLookFor(item);
		if (*sgOptions.Oracool.dropFlares && look.rgb != 0 && look.strength >= 1.0F && Flares.size() < 16)
			Flares.push_back({ item.position, look.rgb, now });
	}
}

// ---- The guardian's death moment -----------------------------------------------------------------------------------------

struct DeathMoment {
	bool active = false;
	float start = 0;
	uint32_t level = 0;
	std::string name;
};
DeathMoment Fall;
int FallenId = -1;
uint32_t FallenLevel = 0;
constexpr float FallLength = 3.6F;

void TickGuardianDeath()
{
	if (Fall.active && (Fall.level != LevelStamp() || Now() - Fall.start > FallLength || Now() < Fall.start))
		Fall.active = false;
	if (leveltype == DTYPE_TOWN)
		return;
	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		const Monster &monster = Monsters[ActiveMonsters[i]];
		if (monster.mode != MonsterMode::Death || (!IsRiftGuardian(monster) && !IsFallenRiftGuardian(monster)))
			continue;
		const int id = static_cast<int>(monster.getId());
		if (FallenId == id && FallenLevel == LevelStamp())
			continue;
		FallenId = id;
		FallenLevel = LevelStamp();
		if (!*sgOptions.Oracool.guardianDeathMoment)
			continue;
		Fall.active = true;
		Fall.start = Now();
		Fall.level = LevelStamp();
		Fall.name = RiftBossName();
		KickScreenShake(6.0F, 0.8F);
		PushWave(WorldX(monster.position.tile), WorldY(monster.position.tile) - 24.0F, 1.5F);
		EmitHitSparks(monster.position.tile, 0xFFC860, 30);
	}
}

// ---- Portals --------------------------------------------------------------------------------------------------------------

uint32_t PortalColour(MissileID type)
{
	switch (type) {
	case MissileID::TownPortal: return 0x6EA0FF;
	case MissileID::RedPortal: return 0xFF5A3C;
	case MissileID::RiftPortalGold: return 0xFFC85A;
	case MissileID::RiftPortalPurple: return 0xBE6EFF;
	default: return 0;
	}
}

/** A drawn missile's current sprite and its bottom-left on the surface, from its tile box's bottom-left. */
std::optional<ClxSprite> MissileSprite(const Missile &missile, Point targetBufferPosition, Point &bottomLeft)
{
	if (!missile._miAnimData || missile._miAnimFrame < 1 || static_cast<size_t>(missile._miAnimFrame) > missile._miAnimData->numSprites())
		return std::nullopt;
	bottomLeft = targetBufferPosition + missile.position.offsetForRendering - Displacement { missile._miAnimWidth2, missile.oracoolScaleLift };
	return (*missile._miAnimData)[missile._miAnimFrame - 1];
}

/** The tile box's bottom-left of @p tile on a view. */
Point TileBox(const View &v, Point tile)
{
	return { static_cast<int>(std::floor(WorldX(tile) + v.ox - 32.0F + 0.5F)), static_cast<int>(std::floor(WorldY(tile) + v.oy + 14.5F + 0.5F)) };
}

} // namespace

// ==== Once a frame ========================================================================================================

void NoteWorldView(const Surface &visible, const Surface &fullOut, Point firstTile, Point firstTileBuffer, float zoom)
{
	Map.valid = !visible.isIndexed();
	Map.view = MakeView(visible, firstTile, firstTileBuffer);
	Map.regionX = visible.region.x - fullOut.region.x;
	Map.zoom = zoom > 1.0F ? zoom : 1.0F;
	const float now = Now();
	float dt = LastTick < 0 || now < LastTick ? 0.0F : now - LastTick;
	dt = dt > 0.25F ? 0.25F : dt;
	LastTick = now;
	if (IsUnderSky(leveltype))
		TickWetness(dt);
	TickFootprints();
	TickCrows(dt);
	TickShockwaves();
	TickDropFlares();
	TickGuardianDeath();
}

// ==== On the floor ========================================================================================================

void ApplySceneLifeGround(const Surface &out, Point firstTile, Point firstTileBuffer)
{
	if (out.isIndexed() || !IsUnderSky(leveltype) || out.w() <= 0 || out.h() <= 0)
		return;
	const View v = MakeView(out, firstTile, firstTileBuffer);
	const float now = Now();
	const int w = out.w();
	const int h = out.h();

	// Puddles: grown with the wetness, shrinking as they dry; rippled while the rain falls.
	float rain = 0;
	const bool raining = ShownWeather(rain) == Weather::Rain && rain > 0.05F;
	const float size = std::sqrt(Clamp01(Wetness * 1.5F));
	if (*sgOptions.Oracool.puddles && Wetness > 0.04F && !OnDesertFloor()) {
		int x0, x1, y0, y1;
		ViewTiles(v, x0, x1, y0, y1);
		for (int ty = y0; ty <= y1; ty++) {
			for (int tx = x0; tx <= x1; tx++) {
				PuddleShape s;
				if (!PuddleAt({ tx, ty }, s))
					continue;
				const float rx = s.rx * size;
				const float ry = s.ry * size;
				if (rx < 3.0F)
					continue;
				const float cx = WorldX({ tx, ty }) + v.ox + s.ox;
				const float cy = WorldY({ tx, ty }) + v.oy + s.oy;
				const float bx = cx + s.lobeX * rx;
				const float by = cy + s.lobeY * ry;
				const float lrx = rx * 0.7F, lry = ry * 0.75F;
				const int px0 = std::max(0, static_cast<int>(std::min(cx - rx, bx - lrx)) - 1);
				const int px1 = std::min(w - 1, static_cast<int>(std::max(cx + rx, bx + lrx)) + 1);
				const int py0 = std::max(0, static_cast<int>(std::min(cy - ry, by - lry)) - 1);
				const int py1 = std::min(h - 1, static_cast<int>(std::max(cy + ry, by + lry)) + 1);
				if (px0 > px1 || py0 > py1)
					continue;
				// Two rain rings at a time, each at a point in the pool, for a third of a second.
				const uint32_t ph = Hash(static_cast<uint32_t>(tx), static_cast<uint32_t>(ty), 777);
				float ringX[2] = { 0, 0 }, ringY[2] = { 0, 0 }, ringR[2] = { -1, -1 }, ringA[2] = { 0, 0 };
				if (raining) {
					for (int k = 0; k < 2; k++) {
						const float cycleF = now * 1.6F + Unit(Hash(ph, static_cast<uint32_t>(k)));
						const uint32_t cycle = static_cast<uint32_t>(cycleF);
						const float life = Frac(cycleF);
						if (life > 0.5F)
							continue;
						ringX[k] = (Unit(Hash(ph, cycle, 1 + k)) - 0.5F) * 1.1F;
						ringY[k] = (Unit(Hash(ph, cycle, 3 + k)) - 0.5F) * 1.1F;
						ringR[k] = life * 0.9F;
						ringA[k] = (1.0F - life / 0.5F) * 70.0F * rain;
					}
				}
				const float irx = 1.0F / rx, iry = 1.0F / ry, ilrx = 1.0F / lrx, ilry = 1.0F / lry;
				for (int py = py0; py <= py1; py++) {
					uint32_t *row = out.at<uint32_t>(0, py);
					const float v1 = (static_cast<float>(py) - cy) * iry;
					const float v2 = (static_cast<float>(py) - by) * ilry;
					for (int px = px0; px <= px1; px++) {
						const float u1 = (static_cast<float>(px) - cx) * irx;
						const float u2 = (static_cast<float>(px) - bx) * ilrx;
						const float d1 = u1 * u1 + v1 * v1;
						const float d2 = u2 * u2 + v2 * v2;
						const float d2min = d1 < d2 ? d1 : d2;
						if (d2min >= 1.0F)
							continue;
						const float d = std::sqrt(d2min);
						const uint32_t c = row[px];
						// Darker, toward a grey-blue sky; a faint bright rim on its far side; a soft edge.
						uint32_t t = SL_SCALE(c, 160);
						t = SL_MIX(t, 96, 110, 134, 76);
						if (d > 0.72F && v1 < 0)
							t = SL_MIX(t, 196, 206, 222, static_cast<int>((d - 0.72F) / 0.28F * 70.0F));
						for (int k = 0; k < 2; k++) {
							if (ringR[k] < 0)
								continue;
							const float du = u1 - ringX[k];
							const float dv = v1 - ringY[k];
							const float off = std::fabs(std::sqrt(du * du + dv * dv) - ringR[k]);
							if (off < 0.09F)
								t = SL_ADD(t, static_cast<int>(ringA[k] * (1.0F - off / 0.09F)), static_cast<int>(ringA[k] * (1.0F - off / 0.09F)),
								    static_cast<int>(ringA[k] * 1.1F * (1.0F - off / 0.09F)));
						}
						const int edge = d > 0.82F ? static_cast<int>((1.0F - d) / 0.18F * 256.0F) : 256;
						row[px] = SL_MIX(c, SL_R(t), SL_G(t), SL_B(t), edge);
					}
				}
			}
		}
	}

	// Footprints: a toe and a heel each, dark in snow, darker still on wet ground, fading over their last ten seconds.
	if (*sgOptions.Oracool.footprints && PrintCount > 0) {
		for (size_t i = 0; i < PrintCount; i++) {
			const Print &p = Prints[(PrintHead + MaxPrints - 1 - i) % MaxPrints];
			const float age = now - p.born;
			if (age < 0 || age >= PrintLife)
				continue;
			const float fade = age < PrintLife - 10.0F ? 1.0F : (PrintLife - age) / 10.0F;
			const float sx = p.x + v.ox;
			const float sy = p.y + v.oy;
			if (sx < -8 || sy < -8 || sx > static_cast<float>(w) + 8 || sy > static_cast<float>(h) + 8)
				continue;
			const int toeX = static_cast<int>(std::floor(sx + p.nx * 2.5F + 0.5F));
			const int toeY = static_cast<int>(std::floor(sy + p.ny * 1.25F + 0.5F));
			const int heelX = static_cast<int>(std::floor(sx - p.nx * 2.5F + 0.5F));
			const int heelY = static_cast<int>(std::floor(sy - p.ny * 1.25F + 0.5F));
			const Point dots[] = { { toeX - 1, toeY }, { toeX, toeY }, { toeX + 1, toeY }, { toeX, toeY + 1 }, { heelX, heelY }, { heelX + 1, heelY } };
			for (const Point &d : dots) {
				if (d.x < 0 || d.y < 0 || d.x >= w || d.y >= h)
					continue;
				uint32_t &c = *out.at<uint32_t>(d.x, d.y);
				if (p.snow)
					c = SL_MIX(c, 118, 128, 150, static_cast<int>(120.0F * fade));
				else
					c = SL_SCALE(c, static_cast<uint32_t>(256 - static_cast<int>(80.0F * fade)));
			}
		}
	}
}

// ==== Over the world, before its light ===================================================================================

void DrawSceneLifeBodies(const Surface &out, Point firstTile, Point firstTileBuffer)
{
	if (out.isIndexed() || out.w() <= 0 || out.h() <= 0)
		return;
	const View v = MakeView(out, firstTile, firstTileBuffer);
	const float now = Now();
	const int w = out.w();
	const int h = out.h();

	if (IsUnderSky(leveltype)) {
		float weather = 0;
		const Weather shown = ShownWeather(weather);
		const float day = Daylight();

		// The weather's tone over the whole picture (user, 2026-10-11, the Necromancer video: snow laid on the floor pass
		// alone whitened the floor tiles and left every tree's and rock's ground piece dark - a checkerboard). Even over
		// everything: rain darkens and greys it a little, snow cools it, greys it and lifts it a little - roofs, trees and
		// ground alike.
		if ((shown == Weather::Rain || shown == Weather::Snow) && weather > 0.01F) {
			const bool snow = shown == Weather::Snow;
			const int grey = static_cast<int>((snow ? 70.0F : 40.0F) * weather);
			const uint32_t keep = static_cast<uint32_t>(256 - static_cast<int>((snow ? 0.0F : 30.0F) * weather));
			const int lift = static_cast<int>(22.0F * weather);
			for (int y = 0; y < h; y++) {
				uint32_t *row = out.at<uint32_t>(0, y);
				for (int x = 0; x < w; x++) {
					uint32_t c = row[x];
					if ((c & 0xFFFFFF) == 0)
						continue; // the void beyond the map stays black
					const int l = (SL_R(c) * 77 + SL_G(c) * 150 + SL_B(c) * 29) >> 8;
					c = SL_MIX(c, l, l, l, grey);
					c = snow ? SL_MIX(c, 205, 213, 228, lift) : SL_SCALE(c, keep);
					row[x] = c;
				}
			}
		}

		// Cloud shadows by day, fainter under an overcast sky.
		if (*sgOptions.Oracool.cloudShadows && day > 0.01F) {
			const float k = day * (shown == Weather::Clear ? 1.0F : 1.0F - 0.8F * weather);
			if (k > 0.01F) {
				constexpr int Cell = 8;
				const int gw = w / Cell + 2;
				const int gh = h / Cell + 2;
				static std::vector<int> Shade;
				Shade.assign(static_cast<size_t>(gw) * gh, 0);
				bool any = false;
				for (int gy = 0; gy < gh; gy++) {
					for (int gx = 0; gx < gw; gx++) {
						const float wx = static_cast<float>(gx * Cell) - v.ox;
						const float wy = (static_cast<float>(gy * Cell) - v.oy) * 2.0F;
						const float n = TableNoise((wx - now * 16.0F) / 480.0F, (wy - now * 6.0F) / 480.0F) * 0.7F
						    + TableNoise((wx - now * 16.0F) / 170.0F, wy / 170.0F) * 0.3F;
						const float d = Smooth((n - 0.5F) / 0.16F) * 0.27F * k;
						const int value = static_cast<int>(d * 256.0F);
						Shade[static_cast<size_t>(gy) * gw + gx] = value;
						any = any || value > 0;
					}
				}
				if (any) {
					const int *shade = Shade.data();
					for (int gy = 0; gy * Cell < h; gy++) {
						const int *r0 = shade + static_cast<size_t>(gy) * gw;
						const int *r1 = r0 + gw;
						for (int gx = 0; gx * Cell < w; gx++) {
							const int c00 = r0[gx], c10 = r0[gx + 1], c01 = r1[gx], c11 = r1[gx + 1];
							if ((c00 | c10 | c01 | c11) == 0)
								continue;
							for (int fy = 0; fy < Cell; fy++) {
								const int y = gy * Cell + fy;
								if (y >= h)
									break;
								uint32_t *row = out.at<uint32_t>(0, y);
								const int left = c00 + (((c01 - c00) * fy) >> 3);
								const int right = c10 + (((c11 - c10) * fy) >> 3);
								for (int fx = 0; fx < Cell; fx++) {
									const int x = gx * Cell + fx;
									if (x >= w)
										break;
									const int a = left + (((right - left) * fx) >> 3);
									if (a <= 0)
										continue;
									const uint32_t c = row[x];
									row[x] = SL_SCALE(c, static_cast<uint32_t>(256 - a));
								}
							}
						}
					}
				}
			}
		}

		if (*sgOptions.Oracool.townLife) {
			// Crows: on the ground pecking, or off and away.
			for (const Crow &crow : Crows) {
				if (!crow.alive)
					continue;
				const int sx = static_cast<int>(std::floor(crow.x + v.ox + 0.5F));
				const int sy = static_cast<int>(std::floor(crow.y + v.oy + 0.5F));
				if (sx < -20 || sx > w + 20 || sy < -40 || sy > h + 220)
					continue;
				if (!crow.flying) {
					const bool peck = Frac(now * 0.7F + Unit(crow.seed)) < 0.18F;
					DrawCrowShape(out, peck ? CrowPeck : CrowStand, 7, sx, sy, crow.faceLeft);
				} else {
					// Its shadow on the ground under it, fading as it climbs.
					const int shadow = static_cast<int>(90.0F * (1.0F - Clamp01(crow.z / 140.0F)));
					for (int dx = -2; dx <= 2; dx++)
						Blend(out, sx + dx, sy, SL_PACK(0, 0, 0), shadow);
					const bool up = Frac(now * 8.5F + Unit(crow.seed)) < 0.5F;
					DrawCrowShape(out, up ? CrowWingsUp : CrowWingsDown, 9, sx, sy - static_cast<int>(crow.z), crow.faceLeft);
				}
			}
			// Leaves blowing through by day, when nothing falls from the sky.
			if (day > 0.2F && (shown == Weather::Clear || weather < 0.2F)) {
				const float spanX = static_cast<float>(w + 80);
				const float spanY = static_cast<float>(h + 80);
				static constexpr uint32_t LeafColours[] = { SL_PACK(150, 90, 30), SL_PACK(182, 122, 40), SL_PACK(118, 70, 26), SL_PACK(170, 148, 58) };
				for (uint32_t i = 0; i < 14; i++) {
					const uint32_t s = Hash(i, 71);
					const float speedX = 34.0F + Unit(s) * 30.0F;
					const float speedY = 12.0F + Unit(s >> 4) * 12.0F;
					const float flutter = std::sin(now * (1.6F + Unit(s >> 8)) + Unit(Hash(s, 5)) * 6.28F);
					float x = std::fmod(Unit(Hash(s, 2)) * 4000.0F + now * speedX + flutter * 10.0F + v.ox, spanX);
					float y = std::fmod(Unit(Hash(s, 3)) * 4000.0F + now * speedY + std::fabs(flutter) * 6.0F + v.oy, spanY);
					x = (x < 0 ? x + spanX : x) - 40.0F;
					y = (y < 0 ? y + spanY : y) - 40.0F;
					const int px = static_cast<int>(x);
					const int py = static_cast<int>(y);
					const uint32_t rgb = LeafColours[i % 4];
					const float spin = Frac(now * (1.2F + Unit(s >> 12)) + Unit(s >> 16));
					Blend(out, px, py, rgb, 225);
					if (spin < 0.25F)
						Blend(out, px + 1, py, rgb, 225);
					else if (spin < 0.5F)
						Blend(out, px + 1, py + 1, rgb, 200);
					else if (spin < 0.75F)
						Blend(out, px, py + 1, rgb, 225);
					else
						Blend(out, px - 1, py + 1, rgb, 200);
				}
			}
		}
	}

	// Torch smoke: soft puffs rising off the flame's top and thinning as they climb.
	if (*sgOptions.Oracool.torchSmoke && leveltype != DTYPE_TOWN) {
		for (int i = 0; i < ActiveObjectCount; i++) {
			const int index = ActiveObjects[i];
			if (index < 0 || index >= MAXOBJECTS)
				continue;
			const Object &object = Objects[index];
			if (!Smokes(object) || !TileShows(object.position) || dLight[object.position.x][object.position.y] >= LightsMax - 1)
				continue;
			if (!object._oAnimData || object._oAnimFrame < 1 || static_cast<size_t>(object._oAnimFrame) > object._oAnimData->numSprites())
				continue;
			const ClxSprite sprite = (*object._oAnimData)[object._oAnimFrame - 1];
			Point tip;
			if (!FlameTip(sprite, tip))
				continue;
			const Point box = TileBox(v, object.position);
			const int sx = box.x - (sprite.width() - 64) / 2 + tip.x;
			const int sy = box.y - sprite.height() + 1 + tip.y - 2;
			if (sx < -40 || sx > w + 40 || sy < -10 || sy > h + 90)
				continue;
			for (uint32_t p = 0; p < 6; p++) {
				const uint32_t s = Hash(static_cast<uint32_t>(index), p, 811);
				const float life = Frac(now * 0.30F + Unit(s));
				const float rise = life * 66.0F;
				const float drift = std::sin(now * 0.9F + static_cast<float>(p) * 1.7F) * 3.0F * life + life * 9.0F;
				const float radius = 1.5F + life * 5.5F;
				const float alpha = (life < 0.12F ? life / 0.12F : 1.0F) * (1.0F - life) * 0.32F;
				const float warm = Clamp01(1.0F - life / 0.4F);
				const int cr = static_cast<int>(84.0F + 70.0F * warm), cg = static_cast<int>(80.0F + 44.0F * warm), cb = static_cast<int>(78.0F + 14.0F * warm);
				const float cx = static_cast<float>(sx) + drift;
				const float cy = static_cast<float>(sy) - rise;
				const int r = static_cast<int>(std::ceil(radius));
				for (int dy = -r; dy <= r; dy++) {
					const int py = static_cast<int>(cy) + dy;
					if (py < 0 || py >= h)
						continue;
					uint32_t *row = out.at<uint32_t>(0, py);
					for (int dx = -r; dx <= r; dx++) {
						const int px = static_cast<int>(cx) + dx;
						if (px < 0 || px >= w)
							continue;
						const float d = std::sqrt(static_cast<float>(dx * dx + dy * dy)) / radius;
						if (d >= 1.0F)
							continue;
						const int a = static_cast<int>(alpha * (1.0F - d) * (1.0F - d) * 256.0F);
						if (a <= 0)
							continue;
						const uint32_t c = row[px];
						row[px] = SL_MIX(c, cr, cg, cb, a);
					}
				}
			}
		}
	}
}

// ==== Light, beside the loot beams ========================================================================================

/**
 * @brief Blessed Shield's beam (user, 2026-10-12): gold light a tile long from the shield's middle (@p cx, @p cy) back along
 * (@p dx, @p dy), the unit direction away from its flight - a bright core three pixels wide in a wider glow, both tapering
 * to the tail. A tile is 64 pixels east-west and 32 north-south on screen, so its length follows the direction. Splat-aware.
 */
void DrawShieldBeam(const Surface &out, float cx, float cy, float dx, float dy)
{
	const LightPen pen = MakePen(out);
	constexpr uint32_t Gold = SL_PACK(255, 196, 80);
	constexpr uint32_t Core = SL_PACK(255, 230, 150); // warm, not white: gold, as the shield is
	// One tile along (dx, dy): across the tile's diamond, |x| / 32 + |y| / 16 = 1 at its edge.
	const float length = 2.0F / (std::fabs(dx) / 32.0F + std::fabs(dy) / 16.0F);
	const int steps = static_cast<int>(length);
	const float nx = -dy, ny = dx; // across the beam
	for (int s = 0; s <= steps; s++) {
		const float f = 1.0F - static_cast<float>(s) / length; // 1 at the shield, 0 at the tail
		const float px = cx + dx * static_cast<float>(s);
		const float py = cy + dy * static_cast<float>(s);
		const float half = 1.0F + 3.0F * f; // the glow narrows to the tail
		for (int o = -4; o <= 4; o++) {
			const float across = static_cast<float>(o);
			if (std::fabs(across) > half)
				continue;
			const float edge = 1.0F - std::fabs(across) / (half + 0.5F);
			const int ix = static_cast<int>(px + nx * across);
			const int iy = static_cast<int>(py + ny * across);
			if (std::abs(o) <= 1)
				pen.addF(ix, iy, Core, f * 0.8F);
			else
				pen.addF(ix, iy, Gold, f * edge * 0.8F);
		}
	}
}

void DrawSceneLifeLights(const Surface &out, Point firstTile, Point firstTileBuffer)
{
	if (out.isIndexed() || out.w() <= 0 || out.h() <= 0)
		return;
	const View v = MakeView(out, firstTile, firstTileBuffer);
	const LightPen pen = MakePen(out);
	const float now = Now();
	const int w = out.w();
	const int h = out.h();

	// Fireflies over Tristram at night, over open ground.
	float weather = 0;
	const Weather shown = ShownWeather(weather);
	if (IsUnderSky(leveltype) && !OnDesertFloor() && *sgOptions.Oracool.townLife && TownIsDark() && (shown == Weather::Clear || weather < 0.3F)) {
		int x0, x1, y0, y1;
		ViewTiles(v, x0, x1, y0, y1);
		constexpr uint32_t Glow = SL_PACK(205, 255, 125);
		for (int ty = y0; ty <= y1; ty++) {
			for (int tx = x0; tx <= x1; tx++) {
				const uint32_t s = Hash(static_cast<uint32_t>(tx), static_cast<uint32_t>(ty), 601);
				if (s % 11 != 0 || !Walkable({ tx, ty }))
					continue;
				const float phase = Unit(s >> 4) * 6.283F;
				const float rate = 1.0F + Unit(s >> 9) * 0.9F;
				float b = std::sin(now * rate + phase);
				b = b > 0.35F ? (b - 0.35F) / 0.65F : 0.0F;
				b *= b;
				if (b < 0.03F)
					continue;
				const float wx = WorldX({ tx, ty }) + std::sin(now * 0.45F + phase) * 18.0F + std::sin(now * 1.3F + phase * 2.0F) * 5.0F;
				const float wy = WorldY({ tx, ty }) + std::cos(now * 0.38F + phase) * 9.0F;
				const float z = 14.0F + std::sin(now * 0.9F + phase) * 6.0F;
				const int px = static_cast<int>(wx + v.ox);
				const int py = static_cast<int>(wy + v.oy - z);
				if (px < -4 || py < -4 || px > w + 4 || py > h + 4)
					continue;
				pen.addF(px, py, Glow, b);
				pen.addF(px - 1, py, Glow, b * 0.45F);
				pen.addF(px + 1, py, Glow, b * 0.45F);
				pen.addF(px, py - 1, Glow, b * 0.45F);
				pen.addF(px, py + 1, Glow, b * 0.45F);
				for (int dy = -2; dy <= 2; dy++)
					for (int dx = -2; dx <= 2; dx++)
						if (std::abs(dx) + std::abs(dy) >= 2 && std::abs(dx) + std::abs(dy) <= 3)
							pen.addF(px + dx, py + dy, Glow, b * 0.12F);
			}
		}
	}

	// Swing trails: a melee swing's arc in front of the hero, brightest at the blade's end.
	if (*sgOptions.Oracool.swingTrails) {
		for (const Player &player : Players) {
			if (!player.plractive || !player.isOnActiveLevel() || player._pmode != PM_ATTACK || !TileShows(player.position.tile))
				continue;
			const int frame = player.AnimInfo.currentFrame;
			const int hit = std::max<int>(player._pAFNum - 1, 0); // the blow lands at currentFrame == _pAFNum - 1 (audit)
			const int first = std::max(0, hit - 4);
			if (frame < first || frame > hit + 2)
				continue;
			const float head = Clamp01((static_cast<float>(frame - first) + 0.5F) / static_cast<float>(hit - first + 1));
			const float fade = frame <= hit ? 1.0F : 1.0F - static_cast<float>(frame - hit) / 3.0F;
			if (fade <= 0)
				continue;
			// The facing, as a screen angle: Diablo's eight directions from South round to SouthEast.
			static constexpr float Facing[8] = { 90.0F, 135.0F, 180.0F, 225.0F, 270.0F, 315.0F, 0.0F, 45.0F };
			const int dir = static_cast<int>(player._pdir);
			const float phi = (dir >= 0 && dir < 8 ? Facing[dir] : 90.0F) * Pi / 180.0F;
			const uint32_t rgb = player._pIFMinDam > 0 ? SL_PACK(255, 150, 60) : (player._pILMinDam > 0 ? SL_PACK(140, 185, 255) : SL_PACK(222, 228, 242));
			const Displacement walk = WalkOffset(player);
			const float cx = WorldX(player.position.tile) + static_cast<float>(walk.deltaX) + v.ox + std::cos(phi) * 8.0F;
			const float cy = WorldY(player.position.tile) + static_cast<float>(walk.deltaY) + v.oy - 36.0F + std::sin(phi) * 4.0F;
			constexpr float Span = 1.75F;
			const float a0 = phi + Span;
			const float a1 = phi - Span;
			const float headAngle = a0 + (a1 - a0) * head;
			const float tailT = std::max(0.0F, head - 0.55F);
			const float tailAngle = a0 + (a1 - a0) * tailT;
			constexpr float R = 30.0F;
			const int steps = static_cast<int>(std::fabs(headAngle - tailAngle) * R) + 2;
			for (int i = 0; i <= steps; i++) {
				const float u = static_cast<float>(i) / static_cast<float>(steps); // 0 the tail, 1 the blade
				const float angle = tailAngle + (headAngle - tailAngle) * u;
				const float k = std::pow(u, 1.6F) * fade * 0.85F;
				const float px = cx + std::cos(angle) * R;
				const float py = cy + std::sin(angle) * R * 0.55F;
				const int ix = static_cast<int>(px);
				const int iy = static_cast<int>(py);
				pen.addF(ix, iy, rgb, k);
				if (u > 0.4F)
					pen.addF(ix, iy - 1, rgb, k * 0.55F);
				if (u > 0.75F) {
					pen.addF(ix, iy + 1, rgb, k * 0.55F);
					pen.addF(ix, iy - 2, rgb, k * 0.25F);
				}
			}
		}
	}

	// Blessed Shield's beam (user, 2026-10-12: "adding trailing light beam (1-tile) long"): gold light a tile long behind the
	// spinning shield, from its middle back along its flight - a bright core three pixels wide in a wider glow, both tapering
	// to the tail. A tile is 64 pixels east-west and 32 north-south on screen, so its length follows the direction.
	if (*sgOptions.Oracool.shieldBeam) {
		for (const Missile &m : Missiles) {
			if (m._mitype != MissileID::BlessedShieldThrow || !m._miDrawFlag || m.position.velocity == Displacement {} || !TileShows(m.position.tileForRendering))
				continue;
			Point bottomLeft;
			const std::optional<ClxSprite> drawn = MissileSprite(m, TileBox(v, m.position.tileForRendering), bottomLeft);
			if (!drawn)
				continue;
			const float vx = static_cast<float>(m.position.velocity.deltaX) / 65536.0F;
			const float vy = static_cast<float>(m.position.velocity.deltaY) / 65536.0F;
			const float speed = std::sqrt(vx * vx + vy * vy);
			if (speed < 0.5F)
				continue;
			const float cx = static_cast<float>(bottomLeft.x) + static_cast<float>(drawn->width()) * 0.5F;
			const float cy = static_cast<float>(bottomLeft.y) - static_cast<float>(drawn->height()) * 0.5F;
			if (cx < -80 || cy < -80 || cx > static_cast<float>(w) + 80 || cy > static_cast<float>(h) + 80)
				continue;
			DrawShieldBeam(out, cx, cy, -vx / speed, -vy / speed);
		}
	}

	// Missile trails: a streak back along the way it flies, in its element's colour.
	if (*sgOptions.Oracool.missileTrails) {
		for (const Missile &m : Missiles) {
			if (!m._miDrawFlag || m.position.velocity == Displacement {} || !TileShows(m.position.tileForRendering))
				continue;
			if (m._mitype == MissileID::BlessedShieldThrow && *sgOptions.Oracool.shieldBeam)
				continue; // its beam above, in place of the element's streak
			const uint32_t rgb = MissileTrailColour(static_cast<int>(m._mitype));
			if (rgb == 0)
				continue;
			Point bottomLeft;
			const std::optional<ClxSprite> drawn = MissileSprite(m, TileBox(v, m.position.tileForRendering), bottomLeft);
			if (!drawn)
				continue;
			const ClxSprite sprite = *drawn;
			const float vx = static_cast<float>(m.position.velocity.deltaX) / 65536.0F;
			const float vy = static_cast<float>(m.position.velocity.deltaY) / 65536.0F;
			const float speed = std::sqrt(vx * vx + vy * vy);
			if (speed < 0.5F)
				continue;
			const float cx = static_cast<float>(bottomLeft.x) + static_cast<float>(sprite.width()) * 0.5F;
			const float cy = static_cast<float>(bottomLeft.y) - static_cast<float>(sprite.height()) * 0.5F;
			if (cx < -60 || cy < -60 || cx > static_cast<float>(w) + 60 || cy > static_cast<float>(h) + 60)
				continue;
			const float dx = -vx / speed;
			const float dy = -vy / speed;
			const float length = std::min(48.0F, std::max(12.0F, speed * 3.0F));
			const float strength = GetMissileData(m._mitype).isArrow() ? 0.35F : 0.6F;
			const int steps = static_cast<int>(length);
			for (int s = 2; s <= steps; s++) {
				const float f = 1.0F - static_cast<float>(s) / length;
				const float k = std::pow(f, 1.6F) * strength;
				const int px = static_cast<int>(cx + dx * static_cast<float>(s));
				const int py = static_cast<int>(cy + dy * static_cast<float>(s));
				pen.addF(px, py, rgb, k);
				if (f > 0.6F) {
					pen.addF(px - static_cast<int>(std::round(dy)), py + static_cast<int>(std::round(dx)), rgb, k * 0.45F);
					pen.addF(px + static_cast<int>(std::round(dy)), py - static_cast<int>(std::round(dx)), rgb, k * 0.45F);
				}
			}
		}
	}

	// Drop flares: a star where a unique, set, primal or runeword item lands.
	if (*sgOptions.Oracool.dropFlares) {
		for (const Flare &f : Flares) {
			const float age = now - f.born;
			if (age < 0 || age > FlareLife)
				continue;
			const float env = age < 0.08F ? age / 0.08F : std::pow(1.0F - (age - 0.08F) / (FlareLife - 0.08F), 1.5F);
			const int cx = static_cast<int>(WorldX(f.tile) + v.ox);
			const int cy = static_cast<int>(WorldY(f.tile) + v.oy) - 10;
			if (cx < -90 || cy < -90 || cx > w + 90 || cy > h + 90)
				continue;
			const uint32_t hot = SL_PACK((SL_R(f.rgb) + 255 * 2) / 3, (SL_G(f.rgb) + 255 * 2) / 3, (SL_B(f.rgb) + 255 * 2) / 3);
			// The core.
			for (int dy = -7; dy <= 7; dy++) {
				for (int dx = -7; dx <= 7; dx++) {
					const float d = std::sqrt(static_cast<float>(dx * dx + dy * dy)) / 7.0F;
					if (d < 1.0F)
						pen.addF(cx + dx, cy + dy, hot, (1.0F - d) * (1.0F - d) * env * 1.2F);
				}
			}
			// The star's four rays, the level ones longest, and two short diagonals.
			const int lx = static_cast<int>(10.0F + 62.0F * env);
			const int ly = static_cast<int>(static_cast<float>(lx) * 0.6F);
			for (int d = 1; d <= lx; d++) {
				const float k = std::pow(1.0F - static_cast<float>(d) / static_cast<float>(lx), 3.0F) * env;
				pen.addF(cx + d, cy, hot, k);
				pen.addF(cx - d, cy, hot, k);
				pen.addF(cx + d, cy - 1, f.rgb, k * 0.35F);
				pen.addF(cx - d, cy + 1, f.rgb, k * 0.35F);
			}
			for (int d = 1; d <= ly; d++) {
				const float k = std::pow(1.0F - static_cast<float>(d) / static_cast<float>(ly), 3.0F) * env;
				pen.addF(cx, cy + d, hot, k);
				pen.addF(cx, cy - d, hot, k);
			}
			const int ld = static_cast<int>(static_cast<float>(lx) * 0.28F);
			for (int d = 1; d <= ld; d++) {
				const float k = std::pow(1.0F - static_cast<float>(d) / static_cast<float>(ld + 1), 2.0F) * env * 0.5F;
				pen.addF(cx + d, cy + d, f.rgb, k);
				pen.addF(cx - d, cy - d, f.rgb, k);
				pen.addF(cx + d, cy - d, f.rgb, k);
				pen.addF(cx - d, cy + d, f.rgb, k);
			}
			// A ring running out across the floor.
			const float ring = 8.0F + age * 72.0F;
			const float ringK = (1.0F - age / FlareLife) * 0.55F;
			const int ringSteps = static_cast<int>(ring * 6.3F);
			for (int i = 0; i < ringSteps; i++) {
				const float a = static_cast<float>(i) / static_cast<float>(ringSteps) * 6.2832F;
				pen.addF(cx + static_cast<int>(std::cos(a) * ring), cy + 10 + static_cast<int>(std::sin(a) * ring * 0.5F), f.rgb, ringK);
			}
		}
	}

	// Living portals: a glow on the floor under them and motes drawn into them.
	if (*sgOptions.Oracool.livingPortals) {
		for (const Missile &m : Missiles) {
			if (!m._miDrawFlag || !IsLivingPortal(m) || !TileShows(m.position.tileForRendering))
				continue;
			Point bottomLeft;
			const std::optional<ClxSprite> drawn = MissileSprite(m, TileBox(v, m.position.tileForRendering), bottomLeft);
			if (!drawn)
				continue;
			const ClxSprite sprite = *drawn;
			const uint32_t rgb = PortalColour(m._mitype);
			const float cx = static_cast<float>(bottomLeft.x) + static_cast<float>(sprite.width()) * 0.5F;
			const float cy = static_cast<float>(bottomLeft.y) - static_cast<float>(sprite.height()) * 0.5F;
			const float floorX = WorldX(m.position.tileForRendering) + v.ox + static_cast<float>(m.position.offsetForRendering.deltaX);
			const float floorY = WorldY(m.position.tileForRendering) + v.oy + static_cast<float>(m.position.offsetForRendering.deltaY);
			if (cx < -80 || cy < -120 || cx > static_cast<float>(w) + 80 || cy > static_cast<float>(h) + 120)
				continue;
			const float pulse = 0.30F + 0.08F * std::sin(now * 3.0F);
			for (int dy = -14; dy <= 14; dy++) {
				for (int dx = -36; dx <= 36; dx++) {
					const float u = static_cast<float>(dx) / 36.0F;
					const float vv = static_cast<float>(dy) / 14.0F;
					const float d = u * u + vv * vv;
					if (d < 1.0F)
						pen.addF(static_cast<int>(floorX) + dx, static_cast<int>(floorY) + dy, rgb, (1.0F - d) * (1.0F - d) * pulse);
				}
			}
			const float rx = static_cast<float>(sprite.width()) * 0.34F;
			const float ry = static_cast<float>(sprite.height()) * 0.44F;
			const uint32_t seed = static_cast<uint32_t>(m.position.tile.x * 131 + m.position.tile.y);
			for (uint32_t i = 0; i < 18; i++) {
				const uint32_t s = Hash(seed, i, 909);
				const float life = Frac(now * 0.42F + Unit(s));
				const float angle = Unit(s >> 5) * 6.283F + now * (1.2F + Unit(s >> 11) * 0.6F) + life * 2.5F;
				const float r = 1.0F - life;
				const int px = static_cast<int>(cx + std::cos(angle) * rx * r);
				const int py = static_cast<int>(cy + std::sin(angle) * ry * r);
				const float k = std::sin(life * Pi) * 0.9F;
				pen.addF(px, py, rgb, k);
				pen.addF(px + 1, py, rgb, k * 0.4F);
				pen.addF(px, py + 1, rgb, k * 0.4F);
			}
		}
	}
}

bool IsLivingPortal(const Missile &missile)
{
	return PortalColour(missile._mitype) != 0;
}

// ==== Over the finished world ==============================================================================================

void ApplySceneLifeAfterWorld(const Surface &world)
{
	if (world.isIndexed() || !Map.valid || world.w() <= 0 || world.h() <= 0)
		return;
	const float now = Now();
	const int w = world.w();
	const int h = world.h();
	const float zoom = Map.zoom;

	// Shockwaves: a ring running out over the ground that bends what lies under it outward, its crest a little brighter.
	for (const Wave &wave : Waves) {
		const float age = now - wave.born;
		if (age < 0 || age > WaveLife)
			continue;
		const float t = age / WaveLife;
		const float cx = static_cast<float>(Map.regionX) + (wave.x + Map.view.ox) * zoom;
		const float cy = (wave.y + Map.view.oy) * zoom;
		const float radius = (10.0F + t * 150.0F * std::min(wave.strength, 1.5F)) * zoom;
		const float band = 13.0F * zoom;
		const float amp = 6.0F * std::min(wave.strength, 1.5F) * std::pow(1.0F - t, 1.2F) * zoom;
		const float glow = 22.0F * (1.0F - t) * std::min(wave.strength, 1.0F);
		const int x0 = std::max(0, static_cast<int>(cx - radius - band) - 1);
		const int x1 = std::min(w - 1, static_cast<int>(cx + radius + band) + 1);
		const int y0 = std::max(0, static_cast<int>(cy - (radius + band) * 0.5F) - 1);
		const int y1 = std::min(h - 1, static_cast<int>(cy + (radius + band) * 0.5F) + 1);
		if (x0 >= x1 || y0 >= y1)
			continue;
		const int bw = x1 - x0 + 1;
		const int bh = y1 - y0 + 1;
		static std::vector<uint32_t> Copy;
		Copy.resize(static_cast<size_t>(bw) * bh);
		for (int y = 0; y < bh; y++)
			std::memcpy(&Copy[static_cast<size_t>(y) * bw], world.at<uint32_t>(x0, y0 + y), static_cast<size_t>(bw) * 4);
		const float inner = radius - band > 0 ? radius - band : 0.0F;
		const float outer = radius + band;
		for (int y = y0; y <= y1; y++) {
			uint32_t *row = world.at<uint32_t>(0, y);
			const float dy = (static_cast<float>(y) - cy) * 2.0F;
			for (int x = x0; x <= x1; x++) {
				const float dx = static_cast<float>(x) - cx;
				const float d2 = dx * dx + dy * dy;
				if (d2 < inner * inner || d2 > outer * outer || d2 < 1.0F)
					continue;
				const float d = std::sqrt(d2);
				const float off = (d - radius) / band; // -1 .. 1 across the band
				const float q = 1.0F - off * off;
				const float prof = q * q;
				const float k = amp * prof / d;
				int sx = static_cast<int>(std::floor(static_cast<float>(x) - dx * k + 0.5F)) - x0;
				int sy = static_cast<int>(std::floor(static_cast<float>(y) - dy * 0.5F * k + 0.5F)) - y0;
				sx = sx < 0 ? 0 : (sx >= bw ? bw - 1 : sx);
				sy = sy < 0 ? 0 : (sy >= bh ? bh - 1 : sy);
				const uint32_t c = Copy[static_cast<size_t>(sy) * bw + sx];
				const int add = static_cast<int>(glow * prof);
				row[x] = add > 0 ? SL_ADD(c, add, add, add) : c;
			}
		}
	}

	// The guardian's fall: a white-gold flash, then the colour drained from the world a moment and coming back.
	if (Fall.active && *sgOptions.Oracool.guardianDeathMoment) {
		const float age = now - Fall.start;
		const int flash = age >= 0 && age < 0.28F ? static_cast<int>(150.0F * (1.0F - age / 0.28F)) : 0;
		float grey = 0;
		if (age >= 0 && age < 1.8F)
			grey = (age < 0.15F ? age / 0.15F : 1.0F - Smooth((age - 0.6F) / 1.2F)) * 0.75F;
		const int g = static_cast<int>(grey * 256.0F);
		if (flash > 0 || g > 0) {
			for (int y = 0; y < h; y++) {
				uint32_t *row = world.at<uint32_t>(0, y);
				for (int x = 0; x < w; x++) {
					uint32_t c = row[x];
					if (g > 0) {
						const int l = (SL_R(c) * 77 + SL_G(c) * 150 + SL_B(c) * 29) >> 8;
						c = SL_MIX(c, l, l, l, g);
					}
					if (flash > 0)
						c = SL_MIX(c, 255, 240, 210, flash);
					row[x] = c;
				}
			}
		}
	}
}

void DrawGuardianDeathCard(const Surface &out)
{
	if (!Fall.active || !*sgOptions.Oracool.guardianDeathMoment || out.isIndexed() || Fall.name.empty())
		return;
	const float age = Now() - Fall.start;
	if (age < 0.5F || age > FallLength - 0.2F)
		return;
	const int y = static_cast<int>(static_cast<float>(out.h()) * 0.2F);
	DrawString(out, "VANQUISHED", Rectangle { Point { 0, y - 26 }, Size { out.w(), 20 } }, { UiFlags::AlignCenter | UiFlags::ColorWhitegold });
	DrawString(out, Fall.name, Rectangle { Point { 0, y }, Size { out.w(), 40 } }, { UiFlags::FontSize30 | UiFlags::AlignCenter | UiFlags::ColorGold });
}

void AddShockwave(Point tile, Displacement offset, float strength)
{
	if (!*sgOptions.Oracool.shockwaves || strength <= 0)
		return;
	PushWave(WorldX(tile) + static_cast<float>(offset.deltaX), WorldY(tile) + static_cast<float>(offset.deltaY) - 10.0F, strength);
}

void ForgetSceneLife()
{
	PrintCount = 0;
	PrintHead = 0;
	HavePrintPlace = false;
	for (Crow &crow : Crows)
		crow.alive = false;
	CrowBack.fill(0);
	Waves.clear();
	Seen.clear();
	SeenPrimed = false;
	Watch.fill({});
	Flares.clear();
	Fall.active = false;
	FallenId = -1; // every Nephalem rift has the same stamp, and his slot can come round again (audit)
	FallenLevel = 0;
	Tips.clear();
	LastTick = -1;
}

// ==== Test hooks =============================================================================================================

float GroundWetness()
{
	return Wetness;
}

void SetGroundWetness(float wetness)
{
	Wetness = Clamp01(wetness);
}

void AdvanceGroundWetness(float seconds)
{
	TickWetness(seconds);
}

size_t FootprintCount()
{
	return PrintCount;
}

size_t ShockwaveCount()
{
	return Waves.size();
}

bool TileHoldsPuddle(Point tile)
{
	PuddleShape s;
	return PuddleAt(tile, s);
}

uint32_t MissileTrailColour(int missileId)
{
	const MissileID type = static_cast<MissileID>(missileId);
	switch (type) {
	// Not projectiles, or drawn some other way: a charging monster, walls, portals, the hero's clones.
	case MissileID::Rhino:
	case MissileID::FlameWave:
	case MissileID::FireWall:
	case MissileID::LightningWall:
	case MissileID::Golem:
	case MissileID::Teleport:
	case MissileID::Phasing:
	case MissileID::Warp:
	case MissileID::TownPortal:
	case MissileID::RedPortal:
	case MissileID::RiftPortalGold:
	case MissileID::RiftPortalPurple:
	case MissileID::AcidCloud:
	case MissileID::Guardian:
	case MissileID::BoneWallEffect:
	case MissileID::Inferno:
	case MissileID::Blizzard:
		return 0;
	default:
		break;
	}
	const MissileData &data = GetMissileData(type);
	if (!data.isDrawn())
		return 0;
	if (data.isArrow() && data.damageType() == DamageType::Physical)
		return SL_PACK(200, 202, 214);
	switch (data.damageType()) {
	case DamageType::Fire: return SL_PACK(255, 138, 48);
	case DamageType::Lightning: return SL_PACK(156, 196, 255);
	case DamageType::Magic: return SL_PACK(192, 140, 255);
	case DamageType::Acid: return SL_PACK(140, 240, 96);
	case DamageType::Cold: return SL_PACK(156, 230, 255);
	default: return SL_PACK(222, 212, 192);
	}
}

} // namespace devilution::oracool

#undef SL_R
#undef SL_G
#undef SL_B
#undef SL_PACK
#undef SL_MIX
#undef SL_SCALE
#undef SL_SAT
#undef SL_ADD

#ifdef _MSC_VER
#pragma optimize("", on)
#pragma runtime_checks("", restore)
#endif
