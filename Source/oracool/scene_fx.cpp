/**
 * @file oracool/scene_fx.cpp
 *
 * The third graphics batch (oracool/scene_fx.h). Every effect here is visual only: it reads the game, never writes it.
 *
 * Audit round 1 (two reviewers) reshaped it: the figures' shadows and reflections and the ground fog are laid on the
 * floor before the world is kept (the relief and Lit Sprites read anything changed after the keep as a wall or a
 * sprite); the shadows go into one screen layer, blurred once; the falling rain and snow come after the light passes,
 * dimmed by the hour; the shake moves the finished picture; every clock stops while the game is paused; and the pixel
 * loops do their arithmetic inline (the Debug build is compiled with /Ob0: no call is inlined, see colour_light.cpp).
 */
#include "oracool/scene_fx.h"

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "diablo.h"
#include "engine/palette.h"
#include "engine/render/clx_render.hpp"
#include "engine/render/text_render.hpp"
#include "engine/surface.hpp"
#include "gmenu.h"
#include "levels/gendung.h"
#include "lighting.h"
#include "monster.h"
#include "multi.h"
#include "options.h"
#include "oracool/chill.h"
#include "oracool/colour_light.h"
#include "oracool/furious_charge.h"
#include "oracool/gpu_post.h"
#include "oracool/particles.h"
#include "oracool/post_fx.h"
#include "oracool/rfa12_actives.h"
#include "oracool/rift.h"
#include "oracool/woods.h" // IsWoodsBoss - he dissolves as a rift guardian does
#include "oracool/rift_bosses.h"
#include "oracool/scene_feel.h"
#include "oracool/scene_glow.h"
#include "oracool/scene_life.h"
#include "oracool/splats.h"
#include "oracool/whirlwind.h"
#include "player.h"

// The pixel loops below are optimised even in the Debug build (as colour_light.cpp and post_fx.cpp are): they run over
// every figure, or over the whole picture, every frame.
#ifdef _MSC_VER
#pragma runtime_checks("", off)
#pragma optimize("gt", on)
#endif

// The channel arithmetic, as macros: a function would stay a call in the Debug build. MIX moves c toward (r, g, b) by
// a/256 (0 <= a <= 256 keeps every channel in range); SCALE multiplies all three by k/256 (k <= 256) on the packed word.
#define SFX_R(c) (static_cast<int>(((c) >> 16) & 0xFF))
#define SFX_G(c) (static_cast<int>(((c) >> 8) & 0xFF))
#define SFX_B(c) (static_cast<int>((c) & 0xFF))
#define SFX_PACK(r, g, b) ((static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b))
#define SFX_MIX(c, r, g, b, a) \
	SFX_PACK(SFX_R(c) + ((((r)-SFX_R(c)) * (a)) >> 8), SFX_G(c) + ((((g)-SFX_G(c)) * (a)) >> 8), SFX_B(c) + ((((b)-SFX_B(c)) * (a)) >> 8))
#define SFX_SCALE(c, k) \
	((((((c)&0xFF00FFU) * static_cast<uint32_t>(k)) >> 8) & 0xFF00FFU) | (((((c)&0x00FF00U) * static_cast<uint32_t>(k)) >> 8) & 0x00FF00U))
#define SFX_SAT(v) ((v) > 255 ? 255 : (v))
#define SFX_ADD(c, r, g, b) SFX_PACK(SFX_SAT(SFX_R(c) + (r)), SFX_SAT(SFX_G(c) + (g)), SFX_SAT(SFX_B(c) + (b)))

namespace devilution::oracool {

namespace {

// ---- The clock: real seconds, standing still while the game is paused ---------------------------------------------------

double SceneClock = 0;
uint32_t SceneTicks = 0;

/** The effects' seconds: a double underneath, wrapped every ten hours so a float of it keeps its steps. */
float Now()
{
	return static_cast<float>(SceneClock);
}

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

/** Where a tile's box lands relative to another tile's: the world's isometric step, 64x32. */
Displacement Iso(int dx, int dy)
{
	return { (dx - dy) * 32, (dx + dy) * 16 };
}

/** The level a record belongs to: a change of it forgets the per-level records. */
uint32_t LevelStamp()
{
	return static_cast<uint32_t>(leveltype) | (static_cast<uint32_t>(currlevel) << 8) | (static_cast<uint32_t>(setlevel ? 1 : 0) << 16)
	    | (static_cast<uint32_t>(setlvlnum) << 24);
}

// ---- A sprite's silhouette, as palette indices ----------------------------------------------------------------------

struct Mask {
	int w = 0;
	int h = 0;
	std::vector<uint8_t> px;
	std::vector<int16_t> first; // each row's first opaque column (w when the row is clear)
	std::vector<int16_t> last;  // ...and its last (-1)
};

constexpr int MaxMaskSide = 320;

/**
 * @p sprite drawn into an 8-bit scratch surface and copied out: its pixels' palette indices, 0 where it is clear. Drawn
 * at (0, h - 1), the repository's convention (audit round 1): row r of the mask is the screen row bottomLeft.y - h + 1 + r.
 */
bool RenderMask(ClxSprite sprite, Mask &mask)
{
	const int w = sprite.width();
	const int h = sprite.height();
	if (w <= 0 || h <= 0 || w > MaxMaskSide || h > MaxMaskSide)
		return false;
	static std::optional<OwnedSurface> Scratch;
	if (!Scratch)
		Scratch.emplace(MaxMaskSide, MaxMaskSide);
	for (int y = 0; y < h; y++)
		std::memset(Scratch->at(0, y), 0, static_cast<size_t>(w));
	ClxDraw(Scratch->subregion(0, 0, w, h), Point { 0, h - 1 }, sprite);
	mask.w = w;
	mask.h = h;
	mask.px.resize(static_cast<size_t>(w) * h);
	mask.first.resize(static_cast<size_t>(h));
	mask.last.resize(static_cast<size_t>(h));
	for (int y = 0; y < h; y++) {
		uint8_t *row = &mask.px[static_cast<size_t>(y) * w];
		std::memcpy(row, Scratch->at(0, y), static_cast<size_t>(w));
		int a = 0;
		while (a < w && row[a] == 0)
			a++;
		int b = w - 1;
		while (b >= a && row[b] == 0)
			b--;
		mask.first[static_cast<size_t>(y)] = static_cast<int16_t>(a);
		mask.last[static_cast<size_t>(y)] = static_cast<int16_t>(b);
	}
	return true;
}

// ---- Shadows and reflections: the figure ground pass ------------------------------------------------------------------

struct ShadowCaster {
	float nx, ny; // the shadow's direction on the floor (unit; screen y is half the floor's)
	float length; // how far it stretches per pixel of height
	float strength;
	float room = 1e9F; // how far along the floor it may run before a wall
};

struct GroundPass {
	bool active = false;
	bool shadows = false;
	bool reflect = false;
	bool damp = false; // the fifth batch's wet floors (oracool/scene_feel.h): the Caves' and the Nest's floor mirrors too
	Point firstTile;
	Point firstTileBuffer;
	int w = 0, h = 0;
	std::vector<uint8_t> layer; // the shadows' darkness, one byte a pixel
	int dx0 = 0, dy0 = 0, dx1 = -1, dy1 = -1; // the part of the layer written this frame
	std::array<GpuLight, 32> lights;
	size_t lightCount = 0;
	const uint8_t *blocks = nullptr;
	const uint8_t *liquid = nullptr;
	bool sun = false;
	ShadowCaster sunCaster {};
	float lampFade = 1.0F; // in town: the lamps' shadows fading in as the day goes (round 3 audit)
};
GroundPass Ground;

bool BlocksAt(int tx, int ty)
{
	if (tx < 0 || ty < 0 || tx >= MAXDUNX || ty >= MAXDUNY)
		return true;
	if (Ground.blocks != nullptr)
		return Ground.blocks[static_cast<size_t>(tx) * MAXDUNY + ty] != 0;
	return TileHasAny(dPiece[tx][ty], TileProperties::Solid);
}

/** Whether a wall stands between the tiles, walked a half tile at a time. */
bool LightBlocked(float x0, float y0, float x1, float y1)
{
	const float dx = x1 - x0;
	const float dy = y1 - y0;
	const float len = std::sqrt(dx * dx + dy * dy);
	const int steps = static_cast<int>(len * 2.0F);
	for (int i = 1; i < steps; i++) {
		const float t = static_cast<float>(i) / static_cast<float>(steps);
		if (BlocksAt(static_cast<int>(std::floor(x0 + dx * t + 0.5F)), static_cast<int>(std::floor(y0 + dy * t + 0.5F))))
			return true;
	}
	return false;
}

/**
 * How far, in floor pixels, a shadow from (@p fx, @p fy) can run along its floor direction (@p nx, @p ny) before a wall
 * stops it (round 2 audit: a long shadow ran through a wall onto the floor beyond it).
 */
float ShadowRoom(float fx, float fy, float nx, float ny)
{
	// A floor pixel's step in tiles: screen x is nx, screen y half of ny; the isometric step undone.
	const float u = nx / 32.0F;
	const float v = ny * 0.5F / 16.0F;
	const float stepX = (u + v) * 0.5F;
	const float stepY = (v - u) * 0.5F;
	for (float d = 8.0F; d < 320.0F; d += 8.0F) {
		if (BlocksAt(static_cast<int>(std::floor(fx + stepX * d + 0.5F)), static_cast<int>(std::floor(fy + stepY * d + 0.5F))))
			return d;
	}
	return 320.0F;
}

/** The lights the figure at tile (@p fx, @p fy) - its true place, walk included - casts shadows from: up to three. */
int ShadowCastersFor(float fx, float fy, bool isHero, std::array<ShadowCaster, 3> &out)
{
	if (Ground.sun) {
		out[0] = Ground.sunCaster;
		out[0].room = ShadowRoom(fx, fy, out[0].nx, out[0].ny);
		return 1;
	}
	int count = 0;
	float fourth = 0; // the strongest light that is not among the three
	for (size_t i = 0; i < Ground.lightCount; i++) {
		const GpuLight &l = Ground.lights[i];
		if (isHero && l.kind == 0)
			continue; // his own light, which moves with him: it casts no shadow of him (audit round 1: it flickered)
		const float dxTile = l.x - fx;
		const float dyTile = l.y - fy;
		const float d = std::sqrt(dxTile * dxTile + dyTile * dyTile);
		if (d < 0.6F || d >= l.reach)
			continue;
		if (LightBlocked(l.x, l.y, fx, fy))
			continue; // a light behind a wall casts nothing here
		// Away from the light, on the floor: the screen vector from the light, its vertical doubled back to the floor's.
		const float sx = -(dxTile - dyTile) * 32.0F;
		const float sy = -(dxTile + dyTile) * 32.0F;
		const float len = std::sqrt(sx * sx + sy * sy);
		if (len < 1.0F)
			continue;
		float k = 0.5F + d * 0.32F;
		k = k > 1.9F ? 1.9F : k;
		const float fall = std::pow(1.0F - d / l.reach, 0.6F);
		float s = 0.5F * (l.intensity > 1.0F ? 1.0F : l.intensity) * fall * (1.15F - 0.3F * k) * Ground.lampFade;
		s = s > 0.55F ? 0.55F : s;
		if (s < 0.04F)
			continue;
		if (count == 3) {
			// Whichever is left out of the three - this light, or the weakest of them it displaces - is a fourth.
			float weakest = out[0].strength;
			for (int j = 1; j < 3; j++)
				weakest = out[j].strength < weakest ? out[j].strength : weakest;
			const float leftOut = s > weakest ? weakest : s;
			fourth = leftOut > fourth ? leftOut : fourth;
		}
		// The three strongest (round 2 audit: the first three by distance from the hero swapped as he walked).
		ShadowCaster caster { sx / len, sy / len, k, s };
		int slot = count < 3 ? count++ : -1;
		if (slot < 0) {
			int weakest = 0;
			for (int j = 1; j < 3; j++)
				weakest = out[j].strength < out[weakest].strength ? j : weakest;
			if (out[weakest].strength >= s)
				continue;
			slot = weakest;
		}
		caster.room = ShadowRoom(fx, fy, caster.nx, caster.ny);
		out[slot] = caster;
	}
	// Each drawn at its lead over the fourth and over the threshold, so the one swapping in and the one swapping out are
	// both at nothing when they swap.
	const float floorLevel = fourth > 0.04F ? fourth : 0.04F;
	int kept = 0;
	for (int i = 0; i < count; i++) {
		out[i].strength -= floorLevel;
		if (out[i].strength > 0.005F)
			out[kept++] = out[i];
	}
	return kept;
}

void CastShadows(const Mask &mask, Point bottomLeft, Point foot, const std::array<ShadowCaster, 3> &casters, int count)
{
	const int top = bottomLeft.y - mask.h + 1;
	const int maxHeight = foot.y - top;
	if (maxHeight <= 0)
		return;
	const int w = Ground.w;
	const int h = Ground.h;
	uint8_t *layer = Ground.layer.data();
	const uint8_t *src = mask.px.data();
	for (int c = 0; c < count; c++) {
		const ShadowCaster &k = casters[c];
		const int steps = static_cast<int>(k.length) + 1;
		const float invSteps = 1.0F / static_cast<float>(steps);
		const float ax = k.nx * k.length;
		const float ay = k.ny * k.length * 0.5F;
		for (int y = 0; y < mask.h; y++) {
			const int height = foot.y - (top + y);
			if (height <= 0)
				break; // below the feet: the painted shadow's ground
			// Faded in from the feet, where the painted shadow already lies, and thinning toward the head.
			float lift = static_cast<float>(height) / 14.0F;
			lift = lift > 1.0F ? 1.0F : lift;
			lift *= 1.0F - 0.35F * static_cast<float>(height) / static_cast<float>(maxHeight);
			const int full = static_cast<int>(k.strength * lift * 255.0F);
			const int xa = mask.first[static_cast<size_t>(y)];
			const int xb = mask.last[static_cast<size_t>(y)];
			if (full <= 0 || xb < xa)
				continue;
			const uint8_t *row = src + static_cast<size_t>(y) * mask.w;
			for (int t = 0; t < steps; t++) {
				const float hh = static_cast<float>(height) + static_cast<float>(t) * invSteps;
				// A wall stops it, faded over its last 12 px rather than cut (round 3 audit).
				const float before = k.room - hh * k.length;
				if (before <= 0)
					break;
				const int value = before >= 12.0F ? full : static_cast<int>(static_cast<float>(full) * before / 12.0F);
				if (value <= 0)
					break;
				const int offX = static_cast<int>(ax * hh);
				const int py = foot.y + static_cast<int>(ay * hh);
				if (py < 0 || py >= h)
					continue;
				uint8_t *cells = layer + static_cast<size_t>(py) * w;
				const int baseX = bottomLeft.x + offX;
				int rowMin = w, rowMax = -1;
				for (int x = xa; x <= xb; x++) {
					if (row[x] == 0)
						continue;
					const int px = baseX + x;
					if (px < 0 || px >= w)
						continue;
					if (value > cells[px]) {
						cells[px] = static_cast<uint8_t>(value);
						rowMin = px < rowMin ? px : rowMin;
						rowMax = px > rowMax ? px : rowMax;
					}
				}
				if (rowMax >= rowMin) {
					Ground.dx0 = rowMin < Ground.dx0 ? rowMin : Ground.dx0;
					Ground.dx1 = rowMax > Ground.dx1 ? rowMax : Ground.dx1;
					Ground.dy0 = py < Ground.dy0 ? py : Ground.dy0;
					Ground.dy1 = py > Ground.dy1 ? py : Ground.dy1;
				}
			}
		}
	}
}

bool WaterAt(const uint8_t *liquid, int x, int y)
{
	return liquid != nullptr && x >= 0 && y >= 0 && x < MAXDUNX && y < MAXDUNY && liquid[static_cast<size_t>(x) * MAXDUNY + y] == 2;
}

void Reflect(const Surface &out, const Mask &mask, Point bottomLeft, Point foot, Point tile, int light)
{
	const uint8_t *liquid = Ground.liquid;
	bool near = Ground.damp; // a damp floor is under every figure
	for (int dx = -1; dx <= 3 && !near; dx++)
		for (int dy = -1; dy <= 3 && !near; dy++)
			near = WaterAt(liquid, tile.x + dx, tile.y + dy);
	if (!near)
		return;
	const float t = Now();
	const int top = bottomLeft.y - mask.h + 1;
	const float dim = (1.0F - static_cast<float>(light < 15 ? light : 15) / 15.0F * 0.8F) * 0.9F;
	const int dimK = static_cast<int>(dim * 256.0F);
	const int w = out.w();
	const int h = out.h();
	const int lastRow = foot.y + 3 + static_cast<int>(150.0F * 0.55F);
	for (int py = foot.y + 3; py <= lastRow; py++) {
		const int height = static_cast<int>(static_cast<float>(py - foot.y - 3) / 0.55F + 0.5F);
		const int y = foot.y - height - top;
		if (height <= 0 || y < 0 || y >= mask.h || py < 0 || py >= h)
			continue;
		const int ripple = static_cast<int>(std::floor(std::sin(static_cast<float>(py) * 0.55F + t * 3.6F) * 1.2F + 0.5F));
		const int alpha = static_cast<int>((0.32F - 0.18F * static_cast<float>(height) / 150.0F) * 256.0F);
		uint32_t *row = out.at<uint32_t>(0, py);
		const uint8_t *src = mask.px.data() + static_cast<size_t>(y) * mask.w;
		// The tile under this row, from the figure's: the isometric step undone.
		const float v = static_cast<float>(py - foot.y) / 16.0F;
		for (int x = 0; x < mask.w; x++) {
			const uint8_t idx = src[x];
			if (idx == 0)
				continue;
			const int px = bottomLeft.x + x + ripple;
			if (px < 0 || px >= w)
				continue;
			const float u = static_cast<float>(px - foot.x) / 32.0F;
			const int tdx = static_cast<int>(std::floor((u + v) * 0.5F + 0.5F));
			const int tdy = static_cast<int>(std::floor((v - u) * 0.5F + 0.5F));
			int a = alpha;
			if (!WaterAt(liquid, tile.x + tdx, tile.y + tdy)) {
				// A damp floor mirrors faintly, on open floor only (scene_feel.h's Wet Floors).
				const int fx = tile.x + tdx, fy = tile.y + tdy;
				if (!Ground.damp || fx < 0 || fy < 0 || fx >= MAXDUNX || fy >= MAXDUNY || TileHasAny(dPiece[fx][fy], TileProperties::Solid))
					continue;
				a = alpha * 3 / 10;
			}
			const uint32_t c = SFX_SCALE(PaletteRGB[idx], dimK);
			const uint32_t d = row[px];
			row[px] = SFX_MIX(d, SFX_R(c), SFX_G(c), SFX_B(c), a);
		}
	}
}

// ---- Afterimages ------------------------------------------------------------------------------------------------------

struct Ghost {
	Displacement world; // the snapshot's tile box bottom-left in world pixels (Iso of its tile plus its walk offset)
	Mask mask;
	float born = 0;
	float life = 0;
};

std::vector<Ghost> Ghosts;
Mask LastHeroMask;
Mask CurrentHeroMask;
Displacement LastHeroWorld;
Point LastHeroTile; // where he truly stands (position.tile), not the tile he is drawn from
bool LastHeroValid = false;
float LastGhostAt = 0;
uint32_t GhostLevel = 0;

void DrawGhost(const Surface &out, const Ghost &g, Point tileBuffer, Point tile, float now)
{
	const float age = (now - g.born) / g.life;
	if (age < 0 || age >= 1)
		return;
	const int a = static_cast<int>(0.42F * std::pow(1.0F - age, 1.2F) * 256.0F);
	const Displacement here = Iso(tile.x, tile.y);
	const Point boxBottomLeft = tileBuffer + (g.world - here);
	const Point bottomLeft = boxBottomLeft - Displacement { (g.mask.w - 64) / 2, 0 };
	const int top = bottomLeft.y - g.mask.h + 1;
	const int w = out.w();
	const int h = out.h();
	for (int y = 0; y < g.mask.h; y++) {
		const int py = top + y;
		if (py < 0 || py >= h)
			continue;
		uint32_t *row = out.at<uint32_t>(0, py);
		const uint8_t *src = g.mask.px.data() + static_cast<size_t>(y) * g.mask.w;
		for (int x = 0; x < g.mask.w; x++) {
			const uint8_t idx = src[x];
			if (idx == 0)
				continue;
			const int px = bottomLeft.x + x;
			if (px < 0 || px >= w)
				continue;
			// A phantom: the hero's own colours run toward a cold blue-white.
			const uint32_t p = PaletteRGB[idx];
			const uint32_t c = SFX_MIX(p, 150, 200, 255, 140);
			const uint32_t d = row[px];
			row[px] = SFX_MIX(d, SFX_R(c), SFX_G(c), SFX_B(c), a);
		}
	}
}

// ---- The guardian's entrance ------------------------------------------------------------------------------------------

struct Entrance {
	bool active = false;
	int monsterId = -1;
	Point tile;
	float start = 0;
	uint32_t level = 0;
	std::string name;
};
Entrance TheEntrance;

constexpr float RiseStart = 0.35F;
constexpr float RiseLength = 1.5F;
constexpr float BeamLength = 2.9F;
constexpr float CardLength = 3.8F;
constexpr float ShakeLength = 1.45F;

float EntranceAge()
{
	return TheEntrance.active ? Now() - TheEntrance.start : 1e9F;
}

void ExpireEntrance()
{
	if (TheEntrance.active && (TheEntrance.level != LevelStamp() || EntranceAge() > CardLength + 0.5F))
		TheEntrance.active = false;
}

// ---- Conjured monsters' fade-in, the veil's keep ---------------------------------------------------------------------------

std::array<float, MaxMonsters> MonsterLastDrawn {};
std::array<float, MaxMonsters> MonsterAppeared {};

struct Kept {
	int x = 0, y = 0, w = 0, h = 0;
	int originX = 0, originY = 0; // the sprite box's own top-left: the burn's noise is anchored to it
	std::vector<uint32_t> px;
};
Kept VeilKeep;

// ---- Weather -------------------------------------------------------------------------------------------------------------

Weather Current = Weather::Clear;
Weather Target = Weather::Clear;
float Intensity = 0;
float NextChange = -1;
float NextFlash = -1;
float FlashAt = -1;
uint32_t WeatherRng = 0x1234567U;

float WeatherRand()
{
	WeatherRng ^= WeatherRng << 13;
	WeatherRng ^= WeatherRng >> 17;
	WeatherRng ^= WeatherRng << 5;
	return Unit(WeatherRng);
}

void TickWeather(float dt)
{
	const float now = Now();
	if (NextChange < 0) {
		WeatherRng ^= SDL_GetTicks() * 2654435761U;
		NextChange = now + 90.0F + WeatherRand() * 150.0F;
	}
	if (now >= NextChange) {
		const float r = WeatherRand();
		if (Current != Weather::Clear)
			Target = r < 0.7F ? Weather::Clear : Current;
		else
			Target = r < 0.33F ? Weather::Rain : (r < 0.45F ? Weather::Snow : Weather::Clear);
		NextChange = now + 150.0F + WeatherRand() * 210.0F;
	}
	if (Current != Target) {
		Intensity -= dt / 10.0F;
		if (Intensity <= 0) {
			Intensity = 0;
			Current = Target;
		}
	} else if (Current != Weather::Clear) {
		Intensity = Intensity + dt / 15.0F > 1.0F ? 1.0F : Intensity + dt / 15.0F;
	}
}

bool WeatherShows()
{
	return IsUnderSky(leveltype) && *sgOptions.Oracool.weather && Current != Weather::Clear && Intensity > 0.01F;
}

// ---- Fog -----------------------------------------------------------------------------------------------------------------

/** A 64x64 tile of value noise, wrapping, sampled bilinearly: what the fog drifts through. */
float TableNoise(float x, float y)
{
	static std::array<float, 64 * 64> Table;
	static bool Built = false;
	if (!Built) {
		for (int i = 0; i < 64 * 64; i++)
			Table[static_cast<size_t>(i)] = Unit(Hash(static_cast<uint32_t>(i & 63), static_cast<uint32_t>(i >> 6), 77));
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

float ValueNoise(float x, float y)
{
	const float fx = std::floor(x);
	const float fy = std::floor(y);
	const int ix = static_cast<int>(fx);
	const int iy = static_cast<int>(fy);
	const float tx = Smooth(x - fx);
	const float ty = Smooth(y - fy);
	const float a = Unit(Hash(static_cast<uint32_t>(ix), static_cast<uint32_t>(iy), 77));
	const float b = Unit(Hash(static_cast<uint32_t>(ix + 1), static_cast<uint32_t>(iy), 77));
	const float c = Unit(Hash(static_cast<uint32_t>(ix), static_cast<uint32_t>(iy + 1), 77));
	const float d = Unit(Hash(static_cast<uint32_t>(ix + 1), static_cast<uint32_t>(iy + 1), 77));
	return (a + (b - a) * tx) + ((c + (d - c) * tx) - (a + (b - a) * tx)) * ty;
}

bool FogLevel(uint32_t &rgb)
{
	if (!*sgOptions.Oracool.groundFog)
		return false;
	switch (leveltype) {
	case DTYPE_CAVES: rgb = SFX_PACK(160, 168, 178); return true;
	case DTYPE_CRYPT: rgb = SFX_PACK(150, 140, 176); return true;
	default: return false;
	}
}

/** The tile under a pixel of the world surface: the isometric step undone from the first tile's centre. */
Point TileUnder(int px, int py, Point firstTile, Point firstTileBuffer)
{
	const float u = static_cast<float>(px - (firstTileBuffer.x + 32)) / 32.0F;
	const float v = static_cast<float>(py - (firstTileBuffer.y - 15)) / 16.0F;
	return { firstTile.x + static_cast<int>(std::floor((u + v) * 0.5F + 0.5F)), firstTile.y + static_cast<int>(std::floor((v - u) * 0.5F + 0.5F)) };
}

// ---- Foliage ------------------------------------------------------------------------------------------------------------

/**
 * The trees' pixels this frame, by screen pixel: the colour the tree drew there and its piece's floor line plus one (0:
 * no tree). A pixel sways only while it still holds that colour - nothing drawn later over it moves.
 */
struct FoliageLayer {
	int w = 0, h = 0;
	std::vector<uint32_t> colour;
	std::vector<int32_t> base;
	int y0 = 0, y1 = -1; // the rows written
	int x0 = 0, x1 = -1;
	bool any = false;
};
FoliageLayer Foliage;
struct FoliageBox {
	int x, y, w, h;
	std::vector<uint32_t> before;
};
FoliageBox FoliageKeep;

// ---- The low health warning's heartbeat -----------------------------------------------------------------------------------

float HeartPhase = 0;
float HeartSeverity = 0;

// ---- A kick of the picture (the fourth batch, oracool/scene_life.h) -------------------------------------------------------

struct ShakeKick {
	float start = 0;
	float amplitude = 0;
	float length = 0;
};
ShakeKick Kick;

} // namespace

// ==== The clock ===========================================================================================================

void AdvanceSceneClock()
{
	const uint32_t now = SDL_GetTicks();
	float dt = SceneTicks == 0 ? 0.0F : static_cast<float>(now - SceneTicks) / 1000.0F;
	SceneTicks = now == 0 ? 1 : now;
	dt = dt < 0 ? 0 : (dt > 0.25F ? 0.25F : dt);
	// The game stands still with the single-player menu up, or paused, and so do these (as Tristram's clock does).
	if (PauseMode != 0 || (!gbIsMultiplayer && gmenu_is_active()))
		dt = 0;
	SceneClock += dt;
	if (SceneClock > 36000.0) {
		// Ten hours in: everything timed from the clock starts over, as on a new level.
		SceneClock = 0;
		ForgetSceneFx();
		NextChange = -1;
		NextFlash = -1;
	}
	if (IsUnderSky(leveltype))
		TickWeather(dt);
	// The low health warning's heartbeat: a strong beat and a softer one after it, quicker as he weakens.
	HeartSeverity = 0;
	if (MyPlayer != nullptr && MyPlayer->_pHitPoints > 0 && MyPlayer->_pMaxHP > 0) {
		const float fraction = static_cast<float>(MyPlayer->_pHitPoints) / static_cast<float>(MyPlayer->_pMaxHP);
		constexpr float Threshold = 1.0F / 3.0F;
		if (fraction < Threshold)
			HeartSeverity = Clamp01((Threshold - fraction) / Threshold);
	}
	HeartPhase += dt * (1.1F + 1.1F * HeartSeverity);
	HeartPhase -= std::floor(HeartPhase);
}

float SceneSeconds()
{
	return Now();
}

float LowHealthHeartbeat(float *severity)
{
	if (severity != nullptr)
		*severity = *sgOptions.Oracool.lowHealthWarning ? HeartSeverity : 0.0F;
	const float beat = HeartPhase;
	const float first = std::exp(-beat * 14.0F);
	const float second = 0.6F * std::exp(-std::fabs(beat - 0.28F) * 18.0F);
	return first > second ? first : second;
}

// ==== Figures =============================================================================================================

void BeginFigureGround(const Surface &out, Point firstTile, Point firstTileBuffer)
{
	Ground.active = false;
	if (out.isIndexed() || out.w() <= 0 || out.h() <= 0)
		return;
	Ground.shadows = *sgOptions.Oracool.figureShadows;
	Ground.reflect = *sgOptions.Oracool.waterReflections;
	Ground.liquid = Ground.reflect ? LiquidMapForGpu() : nullptr; // once a frame (audit round 1: it hashed per figure)
	Ground.reflect = Ground.liquid != nullptr;
	Ground.damp = DampFloorsHere();
	if (!Ground.shadows && !Ground.reflect && !Ground.damp)
		return;
	Ground.active = true;
	Ground.firstTile = firstTile;
	Ground.firstTileBuffer = firstTileBuffer;
	Ground.w = out.w();
	Ground.h = out.h();
	Ground.dx0 = Ground.w;
	Ground.dy0 = Ground.h;
	Ground.dx1 = -1;
	Ground.dy1 = -1;
	Ground.sun = false;
	Ground.lightCount = 0;
	Ground.blocks = nullptr;
	if (!Ground.shadows)
		return;
	if (Ground.layer.size() != static_cast<size_t>(Ground.w) * Ground.h)
		Ground.layer.assign(static_cast<size_t>(Ground.w) * Ground.h, 0);
	if (IsUnderSky(leveltype) && !TownIsDark()) { // and in the woods: the sun, not the hero's torch (the black shadows, 2026-10-14)
		// By day in Tristram, the sun: the shadow swings from left to right as it crosses the sky, longest at either end.
		// With Day and Night off the town stands at mid-morning, and so does the sun; with it on, the shadows fade in and
		// out with the daylight instead of popping at the edge of night.
		const bool cycle = *sgOptions.Oracool.dayNightCycle;
		const float hour = cycle ? TownHour() : 10.0F;
		float daylight = 1.0F;
		if (cycle) {
			const LightTint now = TownAmbientAt(hour);
			const LightTint noon = TownAmbientAt(12.0F);
			const float a = static_cast<float>(now.r + now.g + now.b);
			const float b = static_cast<float>(noon.r + noon.g + noon.b);
			daylight = b > 0 ? Smooth((a / b - 0.9F) / 0.1F) : 1.0F;
		}
		if (hour >= 6.5F && hour <= 18.5F && daylight > 0.01F) {
			const float day = (hour - 6.5F) / 12.0F;
			const float angle = -0.95F + 1.9F * day;
			const float height = std::sin(day * 3.14159265F);
			Ground.sun = true;
			Ground.sunCaster = { std::sin(angle), std::cos(angle), 0.55F + (1.0F - height) * 1.05F, (0.20F + 0.10F * height) * daylight };
		}
		return;
	}
	// In town after dark, the lamps' shadows fade in as the daylight goes, where the sun's faded out (round 3 audit).
	Ground.lampFade = 1.0F;
	if (IsUnderSky(leveltype) && *sgOptions.Oracool.dayNightCycle) {
		const LightTint now = TownAmbientAt(TownHour());
		const LightTint noon = TownAmbientAt(12.0F);
		const float a = static_cast<float>(now.r + now.g + now.b);
		const float b = static_cast<float>(noon.r + noon.g + noon.b);
		Ground.lampFade = b > 0 ? Smooth((0.9F - a / b) / 0.1F) : 1.0F;
	}
	// The lights near the hero, once a frame (audit round 1: once per figure, a sort each time).
	const Point centre = MyPlayer != nullptr ? Point(MyPlayer->position.tile) : firstTile;
	Ground.lightCount = GpuLightsNear(centre, Ground.lights.data(), Ground.lights.size());
	Ground.blocks = leveltype == DTYPE_TOWN ? nullptr : LightBlockingMap();
}

void AddFigureGround(const Surface &out, Point tile, Displacement offset, ClxSprite sprite, Point spriteBottomLeft, int light, bool isHero)
{
	if (!Ground.active)
		return;
	const Point tileBuffer = Ground.firstTileBuffer + Iso(tile.x - Ground.firstTile.x, tile.y - Ground.firstTile.y);
	const Point foot = tileBuffer + offset + Displacement { 32, -15 };
	// What this figure needs, before its silhouette is drawn out.
	std::array<ShadowCaster, 3> casters;
	int n = 0;
	if (Ground.shadows) {
		// Where the figure truly stands: its tile, moved by its walk (audit round 1: the tile alone flickered mid-step).
		const float u = static_cast<float>(offset.deltaX) / 32.0F;
		const float v = static_cast<float>(offset.deltaY) / 16.0F;
		const float fx = static_cast<float>(tile.x) + (u + v) * 0.5F;
		const float fy = static_cast<float>(tile.y) + (v - u) * 0.5F;
		n = ShadowCastersFor(fx, fy, isHero, casters);
	}
	bool water = Ground.damp;
	if (Ground.reflect && !water) {
		for (int dx = -1; dx <= 3 && !water; dx++)
			for (int dy = -1; dy <= 3 && !water; dy++)
				water = WaterAt(Ground.liquid, tile.x + dx, tile.y + dy);
	}
	if (n == 0 && !water)
		return;
	static Mask mask;
	if (!RenderMask(sprite, mask))
		return;
	if (n > 0)
		CastShadows(mask, spriteBottomLeft, foot, casters, n);
	if (water)
		Reflect(out, mask, spriteBottomLeft, foot, tile, light);
}

void EndFigureGround(const Surface &out)
{
	const bool any = Ground.active && Ground.shadows && Ground.dx1 >= Ground.dx0 && Ground.dy1 >= Ground.dy0;
	Ground.active = false;
	if (!any)
		return;
	// Soft: a 3x3 box blur over the part written (three across, then three down), laid on - each pixel darkened by its
	// shadow - and the layer cleared behind it.
	const int w = Ground.w;
	const int x0 = Ground.dx0 > 0 ? Ground.dx0 - 1 : 0;
	const int x1 = Ground.dx1 < w - 1 ? Ground.dx1 + 1 : w - 1;
	const int y0 = Ground.dy0 > 0 ? Ground.dy0 - 1 : 0;
	const int y1 = Ground.dy1 < Ground.h - 1 ? Ground.dy1 + 1 : Ground.h - 1;
	const int bw = x1 - x0 + 1;
	const int bh = y1 - y0 + 1;
	uint8_t *layer = Ground.layer.data();
	static std::vector<uint16_t> Horizontal;
	Horizontal.assign(static_cast<size_t>(bw) * bh, 0);
	for (int y = 0; y < bh; y++) {
		const uint8_t *src = layer + static_cast<size_t>(y0 + y) * w + x0;
		uint16_t *dst = Horizontal.data() + static_cast<size_t>(y) * bw;
		for (int x = 0; x < bw; x++) {
			const int left = x > 0 ? src[x - 1] : 0;
			const int right = x < bw - 1 ? src[x + 1] : 0;
			dst[x] = static_cast<uint16_t>(left + src[x] + right);
		}
	}
	for (int y = 0; y < bh; y++) {
		const uint16_t *up = y > 0 ? Horizontal.data() + static_cast<size_t>(y - 1) * bw : nullptr;
		const uint16_t *mid = Horizontal.data() + static_cast<size_t>(y) * bw;
		const uint16_t *down = y < bh - 1 ? Horizontal.data() + static_cast<size_t>(y + 1) * bw : nullptr;
		uint32_t *row = out.at<uint32_t>(x0, y0 + y);
		for (int x = 0; x < bw; x++) {
			const int sum = mid[x] + (up != nullptr ? up[x] : 0) + (down != nullptr ? down[x] : 0);
			const int a = sum / 9;
			if (a == 0)
				continue;
			const uint32_t c = row[x];
			const uint32_t keep = static_cast<uint32_t>(256 - ((a * 230) >> 8));
			row[x] = SFX_SCALE(c, keep);
		}
	}
	for (int y = y0; y <= y1; y++)
		std::memset(layer + static_cast<size_t>(y) * w + x0, 0, static_cast<size_t>(bw));
}

bool MonsterRising(const Monster &monster)
{
	ExpireEntrance();
	return *sgOptions.Oracool.bossEntrance && TheEntrance.active && TheEntrance.monsterId == static_cast<int>(monster.getId())
	    && EntranceAge() < RiseStart + RiseLength;
}


void DrawPlayerAfterimages(const Surface &out, const Player &player, Point tile, Point tileBuffer, Displacement offset, ClxSprite sprite)
{
	if (&player != MyPlayer || out.isIndexed())
		return;
	if (!*sgOptions.Oracool.afterimages) {
		Ghosts.clear();
		LastHeroValid = false;
		return;
	}
	const float now = Now();
	if (GhostLevel != LevelStamp()) {
		GhostLevel = LevelStamp();
		Ghosts.clear();
		LastHeroValid = false;
	}
	Ghosts.erase(std::remove_if(Ghosts.begin(), Ghosts.end(), [now](const Ghost &g) { return now - g.born >= g.life; }), Ghosts.end());
	// Oldest first, so the newest lies on top.
	for (const Ghost &g : Ghosts)
		DrawGhost(out, g, tileBuffer, tile, now);

	// This frame, noted for the next ones.
	const Displacement world = Iso(tile.x, tile.y) + offset;
	// A jump is where he STANDS moving two tiles or more between frames (user, 2026-10-11, the Necromancer video: the tile
	// he is drawn from could step two at once on a plain walk past a tree, and a phantom trail blinked beside him). A walk
	// moves him one tile at a time; only a Leap, a Teleport or a phase moves him further.
	const Point stands = player.position.tile;
	const int jump = LastHeroValid && !player.isWalking() ? std::max(std::abs(stands.x - LastHeroTile.x), std::abs(stands.y - LastHeroTile.y)) : 0;
	if (jump >= 2 && jump <= 14 && Ghosts.size() < 24) {
		// A Leap or a Teleport: a trail of him along the way he went, the first fading first.
		constexpr int Trail = 6;
		for (int i = 0; i < Trail; i++) {
			const float f = static_cast<float>(i) / static_cast<float>(Trail);
			Ghost g;
			g.world = { LastHeroWorld.deltaX + static_cast<int>(static_cast<float>(world.deltaX - LastHeroWorld.deltaX) * f),
				LastHeroWorld.deltaY + static_cast<int>(static_cast<float>(world.deltaY - LastHeroWorld.deltaY) * f) };
			g.mask = LastHeroMask;
			g.born = now - 0.05F * static_cast<float>(Trail - i);
			g.life = 0.5F;
			Ghosts.push_back(std::move(g));
		}
	}
	const bool dashing = IsFuriousChargeDashing() || IsWhirlwinding(player);
	const bool have = RenderMask(sprite, CurrentHeroMask);
	if (dashing && have && (now - LastGhostAt >= 0.045F || now < LastGhostAt) && Ghosts.size() < 24) {
		Ghost g;
		g.world = world;
		g.mask = CurrentHeroMask;
		g.born = now;
		g.life = 0.32F;
		Ghosts.push_back(std::move(g));
		LastGhostAt = now;
	}
	if (have) {
		std::swap(LastHeroMask, CurrentHeroMask);
		LastHeroWorld = world;
		LastHeroTile = stands;
		LastHeroValid = true;
	}
}

MonsterVeil PlanMonsterVeil(const Monster &monster, Point spriteBottomLeft, ClxSprite sprite)
{
	MonsterVeil veil;
	const float now = Now();
	const size_t id = monster.getId();
	if (id >= MaxMonsters)
		return veil;
	// The guardian's entrance: hidden a moment, then rising out of the floor as he fades in.
	if (MonsterRising(monster)) {
		const float age = EntranceAge();
		const float p = Smooth((age - RiseStart) / RiseLength);
		veil.active = true;
		veil.sink = static_cast<int>((1.0F - p) * static_cast<float>(sprite.height()) * 0.85F);
		veil.clipBottom = spriteBottomLeft.y - 14;
		veil.alpha = Clamp01((age - RiseStart) / 0.9F);
		return veil;
	}
	// What a guardian conjures fades in where it appears (a monster not drawn a moment ago is new to the eye).
	if (*sgOptions.Oracool.dissolveEffects && IsRiftConjured(monster) && monster.mode != MonsterMode::Death) {
		if (MonsterLastDrawn[id] == 0 || now - MonsterLastDrawn[id] > 0.5F)
			MonsterAppeared[id] = now;
		MonsterLastDrawn[id] = now == 0 ? 0.0001F : now;
		const float age = now - MonsterAppeared[id];
		if (age < 0.6F) {
			veil.active = true;
			veil.alpha = Smooth(age / 0.6F);
			veil.sink = static_cast<int>((1.0F - veil.alpha) * static_cast<float>(sprite.height()) * 0.25F);
			veil.clipBottom = spriteBottomLeft.y - 14;
			return veil;
		}
	}
	// A guardian dying burns away, from a quarter of the way into his fall.
	if (DissolvesOnDeath(monster) && monster.mode == MonsterMode::Death) {
		const int frames = monster.animInfo.numberOfFrames > 2 ? monster.animInfo.numberOfFrames - 1 : 1;
		const float progress = static_cast<float>(monster.animInfo.currentFrame) / static_cast<float>(frames);
		veil.dissolve = Clamp01((progress - 0.25F) / 0.7F);
		if (veil.dissolve > 0) {
			veil.active = true;
			static float LastEmbers = 0;
			if (now - LastEmbers > 0.07F || now < LastEmbers) {
				LastEmbers = now;
				EmitHitSparks(monster.position.tile, 0xFF8030, 3);
			}
		}
	}
	return veil;
}

void BeginMonsterVeil(const Surface &out, const MonsterVeil &veil, Point spriteBottomLeft, ClxSprite sprite)
{
	VeilKeep.w = VeilKeep.h = 0;
	if (!veil.active || out.isIndexed())
		return;
	// ClxDraw's rows run from bottomLeft.y - h + 1 to bottomLeft.y, inclusive (audit round 1: the feet were left out).
	VeilKeep.originX = spriteBottomLeft.x;
	VeilKeep.originY = spriteBottomLeft.y + veil.sink - sprite.height() + 1;
	const int x0 = VeilKeep.originX > 0 ? VeilKeep.originX : 0;
	const int y0 = VeilKeep.originY > 0 ? VeilKeep.originY : 0;
	const int x1 = spriteBottomLeft.x + sprite.width() < out.w() ? spriteBottomLeft.x + sprite.width() : out.w();
	const int y1 = spriteBottomLeft.y + veil.sink + 1 < out.h() ? spriteBottomLeft.y + veil.sink + 1 : out.h();
	if (x1 <= x0 || y1 <= y0)
		return; // off the screen: nothing kept, nothing mixed (audit round 1: an empty keep was indexed)
	VeilKeep.x = x0;
	VeilKeep.y = y0;
	VeilKeep.w = x1 - x0;
	VeilKeep.h = y1 - y0;
	VeilKeep.px.resize(static_cast<size_t>(VeilKeep.w) * VeilKeep.h);
	for (int y = 0; y < VeilKeep.h; y++)
		std::memcpy(&VeilKeep.px[static_cast<size_t>(y) * VeilKeep.w], out.at<uint32_t>(x0, y0 + y), static_cast<size_t>(VeilKeep.w) * 4);
}

void EndMonsterVeil(const Surface &out, const MonsterVeil &veil)
{
	if (!veil.active || out.isIndexed() || VeilKeep.w <= 0 || VeilKeep.h <= 0)
		return;
	const int alpha = static_cast<int>(Clamp01(veil.alpha) * 256.0F);
	const float burn = veil.dissolve;
	const int burnK = static_cast<int>(burn * 65536.0F);
	const int edgeK = static_cast<int>((burn + 0.08F) * 65536.0F);
	for (int y = 0; y < VeilKeep.h; y++) {
		uint32_t *row = out.at<uint32_t>(VeilKeep.x, VeilKeep.y + y);
		const uint32_t *kept = &VeilKeep.px[static_cast<size_t>(y) * VeilKeep.w];
		const uint32_t ny = static_cast<uint32_t>(VeilKeep.y + y - VeilKeep.originY);
		for (int x = 0; x < VeilKeep.w; x++) {
			const uint32_t now = row[x];
			const uint32_t was = kept[x];
			if ((now & 0xFFFFFF) == (was & 0xFFFFFF))
				continue;
			if (burn > 0) {
				// Coarse noise (3 px cells) so it burns in patches, not salt; anchored to his own box, so it stays on him.
				const uint32_t nx = static_cast<uint32_t>(VeilKeep.x + x - VeilKeep.originX);
				const int n = static_cast<int>((Hash(nx / 3, ny / 3, 911) & 0xFFFF) * 3 / 4 + (Hash(nx, ny, 912) & 0xFFFF) / 4);
				if (n < burnK) {
					row[x] = was;
					continue;
				}
				if (n < edgeK) {
					row[x] = SFX_MIX(now, 255, 170, 70, 210); // the burning edge
					continue;
				}
			}
			if (alpha < 256)
				row[x] = SFX_MIX(was, SFX_R(now), SFX_G(now), SFX_B(now), alpha);
		}
	}
}

void DrawMonsterStatus(const Surface &out, const Monster &monster, Point spriteBottomLeft, ClxSprite sprite)
{
	if (!*sgOptions.Oracool.statusVisuals || out.isIndexed() || monster.mode == MonsterMode::Death)
		return;
	const bool frozen = IsMonsterFrozen(monster);
	const bool chilled = !frozen && IsMonsterChilled(monster);
	const bool burning = MonsterBurning(monster);
	const bool poisoned = MonsterPoisoned(monster);
	if (!frozen && !chilled && !burning && !poisoned)
		return;
	static Mask mask;
	if (!RenderMask(sprite, mask))
		return;
	const float t = Now();
	const uint32_t id = static_cast<uint32_t>(monster.getId());
	const int top = spriteBottomLeft.y - mask.h + 1;
	const int w = out.w();
	const int h = out.h();
	// A point on the body: a column picked by the hash, the body's first pixel down it from a picked height.
	const auto bodyPoint = [&](uint32_t seed, Point &p) {
		for (int tries = 0; tries < 12; tries++) {
			const int x = static_cast<int>(Unit(Hash(id, seed, static_cast<uint32_t>(tries))) * static_cast<float>(mask.w));
			const int y0 = static_cast<int>((0.15F + 0.6F * Unit(Hash(seed, id, static_cast<uint32_t>(tries) + 50))) * static_cast<float>(mask.h));
			for (int y = y0; y < mask.h; y++) {
				if (mask.px[static_cast<size_t>(y) * mask.w + x] != 0) {
					p = { spriteBottomLeft.x + x, top + y };
					return true;
				}
			}
		}
		return false;
	};
	const auto addPixel = [&](int x, int y, int r, int g, int b) {
		if (x >= 0 && y >= 0 && x < w && y < h) {
			uint32_t &c = *out.at<uint32_t>(x, y);
			c = SFX_ADD(c, r, g, b);
		}
	};
	if (frozen) {
		// Frost twinkling across the ice.
		for (uint32_t i = 0; i < 6; i++) {
			Point p;
			if (!bodyPoint(100 + i, p))
				continue;
			const float s = std::sin(t * 5.5F + static_cast<float>(i) * 1.9F);
			if (s <= 0.2F)
				continue;
			const int v = static_cast<int>((s - 0.2F) / 0.8F * 200.0F);
			addPixel(p.x, p.y, v, v, v);
			addPixel(p.x - 1, p.y, v / 2, v / 2, v / 2 + 20);
			addPixel(p.x + 1, p.y, v / 2, v / 2, v / 2 + 20);
			addPixel(p.x, p.y - 1, v / 2, v / 2, v / 2 + 20);
			addPixel(p.x, p.y + 1, v / 2, v / 2, v / 2 + 20);
		}
	}
	if (chilled) {
		// Frost motes drifting up off him.
		for (uint32_t i = 0; i < 4; i++) {
			Point p;
			if (!bodyPoint(200 + i, p))
				continue;
			const float cycle = std::fmod(t * 0.8F + Unit(Hash(id, i, 201)), 1.0F);
			const int y = p.y - static_cast<int>(cycle * 22.0F);
			const int v = static_cast<int>((1.0F - cycle) * 110.0F);
			addPixel(p.x + static_cast<int>(std::sin(t * 2.0F + static_cast<float>(i)) * 2.0F), y, v / 2, v * 3 / 4, v);
		}
	}
	if (burning) {
		// Tongues of flame licking up from the body.
		for (uint32_t i = 0; i < 6; i++) {
			Point p;
			if (!bodyPoint(300 + i, p))
				continue;
			const float flicker = 0.6F + 0.4F * std::sin(t * 13.0F + static_cast<float>(i) * 2.3F);
			const int height = 5 + static_cast<int>(Unit(Hash(id, i, 301)) * 6.0F * flicker);
			for (int k = 0; k < height; k++) {
				const float f = 1.0F - static_cast<float>(k) / static_cast<float>(height);
				const int sway = static_cast<int>(std::sin(t * 9.0F + static_cast<float>(k) * 0.6F + static_cast<float>(i)) * (1.0F - f) * 1.5F);
				addPixel(p.x + sway, p.y - k, static_cast<int>(230 * f * flicker), static_cast<int>(120 * f * f * flicker), static_cast<int>(20 * f));
				if (k < height / 2)
					addPixel(p.x + sway + 1, p.y - k, static_cast<int>(150 * f * flicker), static_cast<int>(60 * f * flicker), 0);
			}
		}
	}
	if (poisoned) {
		// Green drops falling off him.
		for (uint32_t i = 0; i < 3; i++) {
			Point p;
			if (!bodyPoint(400 + i, p))
				continue;
			const float cycle = std::fmod(t * 1.1F + Unit(Hash(id, i, 401)), 1.0F);
			const int y = p.y + static_cast<int>(cycle * cycle * 20.0F);
			const int v = static_cast<int>((1.0F - cycle * 0.6F) * 170.0F);
			addPixel(p.x, y, v / 3, v, v / 4);
			addPixel(p.x, y + 1, v / 4, v * 3 / 4, v / 5);
		}
	}
}

bool DissolvesOnDeath(const Monster &monster)
{
	return *sgOptions.Oracool.dissolveEffects && (IsRiftGuardian(monster) || IsFallenRiftGuardian(monster) || IsWoodsBoss(monster));
}

// ==== World passes ======================================================================================================

void ApplyWeatherGround(const Surface &out, Point firstTile, Point firstTileBuffer)
{
	if (!IsUnderSky(leveltype) || out.isIndexed() || !WeatherShows())
		return;
	const int w = out.w();
	const int h = out.h();
	if (Current == Weather::Rain) {
		// The wet and the white are laid over the whole picture now (scene_life.cpp's DrawSceneLifeBodies, user 2026-10-11:
		// on the floor alone they left every tree's and rock's ground piece bare - a checkerboard of light tiles).
		// Splashes on the ground, each a brief ring of light - on walkable floor only (audit round 1: they rang on roofs).
		const float t = Now();
		const int splashes = static_cast<int>(70.0F * Intensity);
		for (int i = 0; i < splashes; i++) {
			const float phase = Unit(Hash(static_cast<uint32_t>(i), 41));
			const float cycleF = t * 3.0F + phase;
			const uint32_t cycle = static_cast<uint32_t>(cycleF);
			const float life = cycleF - std::floor(cycleF);
			if (life > 0.35F)
				continue;
			const int sx = static_cast<int>(Unit(Hash(static_cast<uint32_t>(i), cycle, 42)) * static_cast<float>(w));
			const int sy = static_cast<int>(Unit(Hash(static_cast<uint32_t>(i), cycle, 43)) * static_cast<float>(h));
			const Point tile = TileUnder(sx, sy, firstTile, firstTileBuffer);
			if (tile.x < 0 || tile.y < 0 || tile.x >= MAXDUNX || tile.y >= MAXDUNY || TileHasAny(dPiece[tile.x][tile.y], TileProperties::Solid))
				continue;
			const int a = static_cast<int>((1.0F - life / 0.35F) * 110.0F);
			const int r = 1 + static_cast<int>(life * 8.0F);
			for (int dx = -r; dx <= r; dx++) {
				for (int dy = -1; dy <= 0; dy++) {
					const int px = sx + dx, py = sy + dy;
					if (px >= 0 && py >= 0 && px < w && py < h && (dx == r || dx == -r || dy == -1)) {
						uint32_t &c = *out.at<uint32_t>(px, py);
						c = SFX_MIX(c, 190, 205, 225, a);
					}
				}
			}
		}
	}
}

void ApplyGroundFog(const Surface &out, Point firstTile, Point firstTileBuffer)
{
	uint32_t fog;
	if (out.isIndexed() || out.w() <= 0 || out.h() <= 0 || !FogLevel(fog))
		return;
	const float t = Now();
	const int w = out.w();
	const int h = out.h();
	// The mist lies on the floor, drifting slowly; it shows by the light that is there (dLight: 0 bright .. 15 dark), so it
	// never glows in the dark. Worked on an 8 px grid, blended between.
	constexpr int Cell = 8;
	const int gw = w / Cell + 2;
	const int gh = h / Cell + 2;
	static std::vector<int> Density;
	Density.assign(static_cast<size_t>(gw) * gh, 0);
	const float worldX0 = static_cast<float>((firstTile.x - firstTile.y) * 32 - firstTileBuffer.x);
	const float worldY0 = static_cast<float>((firstTile.x + firstTile.y) * 16 - firstTileBuffer.y);
	bool any = false;
	for (int gy = 0; gy < gh; gy++) {
		for (int gx = 0; gx < gw; gx++) {
			const int sx = gx * Cell;
			const int sy = gy * Cell;
			// The light first: most of a dungeon screen is dark, and dark fog is not seen.
			const float u = static_cast<float>(sx - (firstTileBuffer.x + 32)) / 32.0F;
			const float v = static_cast<float>(sy - (firstTileBuffer.y - 15)) / 16.0F;
			const int tx = firstTile.x + static_cast<int>(std::floor((u + v) * 0.5F + 0.5F));
			const int ty = firstTile.y + static_cast<int>(std::floor((v - u) * 0.5F + 0.5F));
			int light = LightsMax;
			if (tx >= 0 && ty >= 0 && tx < MAXDUNX && ty < MAXDUNY)
				light = dLight[tx][ty];
			if (light >= LightsMax - 1)
				continue;
			const float wx = static_cast<float>(sx) + worldX0;
			const float wy = static_cast<float>(sy) + worldY0;
			const float n = TableNoise((wx + t * 9.0F) / 150.0F, (wy * 1.7F + t * 4.0F) / 150.0F) * 0.65F
			    + TableNoise((wx - t * 6.0F) / 55.0F, (wy * 1.7F) / 55.0F) * 0.35F;
			float d = (n - 0.42F) / 0.36F;
			if (d <= 0)
				continue;
			d = d > 1.0F ? 1.0F : d;
			d = d * d * (3.0F - 2.0F * d) * 0.30F;
			const float lit = 1.0F - static_cast<float>(light) / static_cast<float>(LightsMax);
			d *= lit * lit;
			const int value = static_cast<int>(d * 256.0F);
			Density[static_cast<size_t>(gy) * gw + gx] = value;
			any = any || value > 0;
		}
	}
	if (!any)
		return;
	const int fr = SFX_R(fog), fg = SFX_G(fog), fb = SFX_B(fog);
	const int *density = Density.data();
	for (int gy = 0; gy * Cell < h; gy++) {
		const int *r0 = density + static_cast<size_t>(gy) * gw;
		const int *r1 = r0 + gw;
		for (int gx = 0; gx * Cell < w; gx++) {
			const int c00 = r0[gx], c10 = r0[gx + 1], c01 = r1[gx], c11 = r1[gx + 1];
			if ((c00 | c10 | c01 | c11) == 0)
				continue; // a clear block (audit round 1)
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
					row[x] = SFX_MIX(c, fr, fg, fb, a);
				}
			}
		}
	}
}

void DrawGuardianBeam(const Surface &out, Point firstTile, Point firstTileBuffer)
{
	ExpireEntrance();
	if (!TheEntrance.active || !*sgOptions.Oracool.bossEntrance || out.isIndexed())
		return;
	const float age = EntranceAge();
	if (age > BeamLength)
		return;
	if (TheEntrance.monsterId >= 0 && static_cast<size_t>(TheEntrance.monsterId) < MaxMonsters && IsRiftGuardian(Monsters[TheEntrance.monsterId]))
		TheEntrance.tile = Monsters[TheEntrance.monsterId].position.tile;
	const float cx = static_cast<float>(firstTileBuffer.x + 32) + static_cast<float>((TheEntrance.tile.x - firstTile.x) - (TheEntrance.tile.y - firstTile.y)) * 32.0F;
	const float base = static_cast<float>(firstTileBuffer.y - 15) + static_cast<float>((TheEntrance.tile.x - firstTile.x) + (TheEntrance.tile.y - firstTile.y)) * 16.0F;
	const float grow = Smooth(age / 0.3F);
	const float envelope = Smooth(age / 0.25F) * (age < 1.2F ? 1.0F : 1.0F - Smooth((age - 1.2F) / (BeamLength - 1.2F)));
	if (envelope <= 0.01F)
		return;
	const int w = out.w();
	const int h = out.h();
	std::vector<Splat> *splats = SplatCollection();
	const auto light = [&](int px, int py, int r, int g, int b) {
		if (px < 0 || py < 0 || px >= w || py >= h || (r | g | b) <= 0)
			return;
		if (splats != nullptr) {
			splats->push_back({ static_cast<float>(px), static_cast<float>(py), static_cast<float>(r > 255 ? 255 : r) / 255.0F,
			    static_cast<float>(g > 255 ? 255 : g) / 255.0F, static_cast<float>(b > 255 ? 255 : b) / 255.0F, 0.0F });
			return;
		}
		uint32_t &c = *out.at<uint32_t>(px, py);
		c = SFX_ADD(c, r, g, b);
	};
	// The column: from the floor where he rises to the top of the screen, a soft gold-white shaft with a white core.
	const int halfWidth = std::max(2, static_cast<int>(30.0F * grow));
	const int top = std::max(0, static_cast<int>(base) - 900);
	const int bottom = std::min(h - 1, static_cast<int>(base));
	if (splats != nullptr)
		splats->reserve(splats->size() + static_cast<size_t>(bottom - top + 1) * static_cast<size_t>(2 * halfWidth + 1) + 20000);
	float across[2 * 64 + 1];
	const int hw = halfWidth > 64 ? 64 : halfWidth;
	for (int dx = -hw; dx <= hw; dx++) {
		const float a = 1.0F - static_cast<float>(dx < 0 ? -dx : dx) / static_cast<float>(hw + 1);
		across[dx + hw] = a * a;
	}
	const int column = static_cast<int>(cx);
	const int x0 = column - hw < 0 ? 0 : column - hw;
	const int x1 = column + hw > w - 1 ? w - 1 : column + hw;
	for (int py = top; py <= bottom; py++) {
		const float up = Clamp01((base - static_cast<float>(py)) / 900.0F);
		const float fade = (1.0F - up) * (0.75F + 0.25F * std::sin(static_cast<float>(py) * 0.07F - age * 9.0F));
		const float v = envelope * fade;
		const int core = static_cast<int>(v * 120.0F);
		uint32_t *row = splats == nullptr ? out.at<uint32_t>(0, py) : nullptr;
		for (int px = x0; px <= x1; px++) {
			const int dx = px - column;
			const float f = v * across[dx + hw] * 0.75F;
			const int c = (dx < 0 ? -dx : dx) <= hw / 5 ? core : 0;
			const int r = static_cast<int>(255.0F * f) + c, g = static_cast<int>(222.0F * f) + c, b = static_cast<int>(165.0F * f) + c;
			if ((r | g | b) <= 0)
				continue;
			if (row != nullptr) {
				const uint32_t was = row[px];
				row[px] = SFX_ADD(was, r, g, b);
			} else {
				splats->push_back({ static_cast<float>(px), static_cast<float>(py), static_cast<float>(SFX_SAT(r)) / 255.0F,
				    static_cast<float>(SFX_SAT(g)) / 255.0F, static_cast<float>(SFX_SAT(b)) / 255.0F, 0.0F });
			}
		}
	}
	// On the floor: a glow under it, and a ring of light running out across the ground (until it fades: audit round 1,
	// the loop kept the ring's size long after it was gone).
	const float ring = 24.0F + age * 150.0F;
	const float ringFade = 1.0F - Smooth(age / 1.3F);
	const int rx = ringFade > 0 ? static_cast<int>(ring > 90.0F ? ring : 90.0F) + 8 : 98;
	for (int dy = -rx / 2; dy <= rx / 2; dy++) {
		const int py = static_cast<int>(base) + dy;
		if (py < 0 || py >= h)
			continue;
		for (int dx = -rx; dx <= rx; dx++) {
			const int d2 = dx * dx + dy * dy * 4;
			const float ringIn = ring > 6.0F ? ring - 6.0F : 0.0F;
			if (d2 >= 8100 && (ringFade <= 0 || static_cast<float>(d2) < ringIn * ringIn || static_cast<float>(d2) > (ring + 6.0F) * (ring + 6.0F)))
				continue; // neither the glow nor the ring's edge reaches here
			const float d = std::sqrt(static_cast<float>(d2));
			float f = Clamp01(1.0F - d / 90.0F) * envelope * 0.55F;
			if (ringFade > 0) {
				const float edge = d > ring ? d - ring : ring - d;
				if (edge < 6.0F)
					f += (1.0F - edge / 6.0F) * ringFade * 0.8F;
			}
			if (f <= 0.01F)
				continue;
			light(static_cast<int>(cx) + dx, py, static_cast<int>(240.0F * f), static_cast<int>(200.0F * f), static_cast<int>(140.0F * f));
		}
	}
}

void BeginFoliageMicro(const Surface &out, int x, int y, int w, int h)
{
	FoliageKeep.w = 0;
	if (out.isIndexed())
		return;
	const int x0 = x < 0 ? 0 : x;
	const int y0 = y < 0 ? 0 : y;
	const int x1 = x + w > out.w() ? out.w() : x + w;
	const int y1 = y + h > out.h() ? out.h() : y + h;
	if (x1 <= x0 || y1 <= y0)
		return;
	FoliageKeep.x = x0;
	FoliageKeep.y = y0;
	FoliageKeep.w = x1 - x0;
	FoliageKeep.h = y1 - y0;
	// Every pixel of the box marked in its unused top byte: the tree's draw writes whole palette words (top byte 0), so
	// after it a pixel without the mark is the tree's - even where it drew the very colour that was there (round 3 audit:
	// comparing colours missed those).
	for (int r = 0; r < FoliageKeep.h; r++) {
		uint32_t *row = out.at<uint32_t>(x0, y0 + r);
		for (int c = 0; c < FoliageKeep.w; c++)
			row[c] |= 0x01000000U;
	}
}

void EndFoliageMicro(const Surface &out, int baseY)
{
	if (FoliageKeep.w <= 0 || out.isIndexed())
		return;
	if (Foliage.w != out.w() || Foliage.h != out.h()) {
		Foliage.w = out.w();
		Foliage.h = out.h();
		Foliage.colour.assign(static_cast<size_t>(Foliage.w) * Foliage.h, 0);
		Foliage.base.assign(static_cast<size_t>(Foliage.w) * Foliage.h, 0);
		Foliage.any = false;
	}
	if (!Foliage.any) {
		Foliage.y0 = Foliage.h;
		Foliage.y1 = -1;
		Foliage.x0 = Foliage.w;
		Foliage.x1 = -1;
	}
	const int32_t mark = baseY + 1;
	for (int r = 0; r < FoliageKeep.h; r++) {
		const int y = FoliageKeep.y + r;
		uint32_t *now = out.at<uint32_t>(FoliageKeep.x, y);
		uint32_t *colour = &Foliage.colour[static_cast<size_t>(y) * Foliage.w + FoliageKeep.x];
		int32_t *base = &Foliage.base[static_cast<size_t>(y) * Foliage.w + FoliageKeep.x];
		for (int x = 0; x < FoliageKeep.w; x++) {
			const uint32_t c = now[x];
			const bool tree = (c >> 24) == 0;
			now[x] = c & 0xFFFFFFU; // the mark off again
			colour[x] = tree ? c : colour[x];
			base[x] = tree ? mark : base[x];
		}
	}
	Foliage.any = true;
	Foliage.y0 = FoliageKeep.y < Foliage.y0 ? FoliageKeep.y : Foliage.y0;
	Foliage.y1 = FoliageKeep.y + FoliageKeep.h - 1 > Foliage.y1 ? FoliageKeep.y + FoliageKeep.h - 1 : Foliage.y1;
	Foliage.x0 = FoliageKeep.x < Foliage.x0 ? FoliageKeep.x : Foliage.x0;
	Foliage.x1 = FoliageKeep.x + FoliageKeep.w - 1 > Foliage.x1 ? FoliageKeep.x + FoliageKeep.w - 1 : Foliage.x1;
	FoliageKeep.w = 0;
}

namespace {

/**
 * The sway: each tree pixel still showing its tree moved by its height above its floor line, the wind's phase running
 * continuously with the world's x (round 3 audit: a phase per run seamed every 32 px). Every term comes from tables made
 * once a frame (no calls in the pixel loop), and a pixel samples its neighbours only where they are still tree.
 */
void ApplyFoliageSway(const Surface &out, Point firstTile, Point firstTileBuffer)
{
	const float t = Now();
	const int w = out.w();
	const int worldX0 = (firstTile.x - firstTile.y) * 32 - firstTileBuffer.x;
	// By column: the two waves' sines and cosines at this x; by height above the floor line: the amplitude (full from
	// 64 px up, so the steps between pieces' floor lines barely change it) and the height's own phase.
	static std::vector<float> SinA, CosA, Second;
	SinA.resize(static_cast<size_t>(w));
	CosA.resize(static_cast<size_t>(w));
	Second.resize(static_cast<size_t>(w));
	for (int x = Foliage.x0; x <= Foliage.x1; x++) {
		const float phase = static_cast<float>(worldX0 + x) * 0.012F;
		SinA[static_cast<size_t>(x)] = std::sin(t * 1.1F + phase);
		CosA[static_cast<size_t>(x)] = std::cos(t * 1.1F + phase);
		Second[static_cast<size_t>(x)] = 0.3F * std::sin(t * 2.7F + phase * 2.0F);
	}
	static std::array<float, 401> Amp, SinB, CosB;
	static bool Built = false;
	if (!Built) {
		for (int hgt = 0; hgt <= 400; hgt++) {
			float up = static_cast<float>(hgt) / 64.0F;
			up = up > 1.0F ? 1.0F : up;
			Amp[static_cast<size_t>(hgt)] = std::pow(up, 1.3F) * 1.4F;
			SinB[static_cast<size_t>(hgt)] = std::sin(static_cast<float>(hgt) * 0.015F);
			CosB[static_cast<size_t>(hgt)] = std::cos(static_cast<float>(hgt) * 0.015F);
		}
		Built = true;
	}
	const float *sinA = SinA.data();
	const float *cosA = CosA.data();
	const float *second = Second.data();
	static std::vector<uint32_t> Line;
	Line.resize(static_cast<size_t>(w));
	const int span = Foliage.x1 - Foliage.x0 + 1;
	for (int y = Foliage.y0; y <= Foliage.y1; y++) {
		uint32_t *row = out.at<uint32_t>(0, y);
		const uint32_t *colour = &Foliage.colour[static_cast<size_t>(y) * Foliage.w];
		int32_t *base = &Foliage.base[static_cast<size_t>(y) * Foliage.w];
		std::memcpy(Line.data() + Foliage.x0, row + Foliage.x0, static_cast<size_t>(span) * 4);
		const uint32_t *line = Line.data();
		for (int x = Foliage.x0; x <= Foliage.x1; x++) {
			const int32_t mark = base[x];
			if (mark == 0 || row[x] != colour[x])
				continue; // not a tree, or drawn over since: it stays put
			int hgt = mark - 1 - y;
			hgt = hgt < 0 ? 0 : (hgt > 400 ? 400 : hgt);
			const float amp = Amp[static_cast<size_t>(hgt)];
			if (amp < 0.05F)
				continue;
			// sin(A + B) by the angle addition: no call here.
			const float shift = amp * (sinA[x] * CosB[static_cast<size_t>(hgt)] + cosA[x] * SinB[static_cast<size_t>(hgt)] + second[x]);
			if (shift > -0.0625F && shift < 0.0625F)
				continue;
			const int whole = static_cast<int>(shift + 8.0F) - 8; // the floor, for |shift| < 8
			const int frac = static_cast<int>((shift - static_cast<float>(whole)) * 256.0F);
			int ia = x - whole - 1;
			int ib = x - whole;
			ia = ia < Foliage.x0 ? Foliage.x0 : (ia > Foliage.x1 ? Foliage.x1 : ia);
			ib = ib < Foliage.x0 ? Foliage.x0 : (ib > Foliage.x1 ? Foliage.x1 : ib);
			// A neighbour that is no longer tree gives this pixel's own colour.
			const uint32_t ca = base[ia] != 0 && line[ia] == colour[ia] ? line[ia] : line[x];
			const uint32_t cb = base[ib] != 0 && line[ib] == colour[ib] ? line[ib] : line[x];
			row[x] = SFX_MIX(cb, SFX_R(ca), SFX_G(ca), SFX_B(ca), frac);
		}
		// Cleared behind, for the next frame: only the marks decide (round 3 audit: the colours need no clearing).
		std::memset(base + Foliage.x0, 0, static_cast<size_t>(span) * 4);
	}
	Foliage.any = false;
}

} // namespace

void ApplyTownSway(const Surface &out, Point firstTile, Point firstTileBuffer)
{
	if (!Foliage.any)
		return;
	if (leveltype != DTYPE_TOWN || !*sgOptions.Oracool.livingTown || out.isIndexed() || Foliage.w != out.w() || Foliage.h != out.h()) {
		// Nothing sways, but what was noted is cleared, or it would linger into the next frame.
		for (int y = Foliage.y0; y <= Foliage.y1 && Foliage.w > 0; y++) {
			std::memset(&Foliage.base[static_cast<size_t>(y) * Foliage.w + Foliage.x0], 0, static_cast<size_t>(Foliage.x1 - Foliage.x0 + 1) * 4);
		}
		Foliage.any = false;
		return;
	}
	ApplyFoliageSway(out, firstTile, firstTileBuffer);
}

// ==== After the world ===================================================================================================

void KickScreenShake(float amplitude, float seconds)
{
	Kick = { Now(), amplitude, seconds };
}

Displacement ScreenShakeOffset()
{
	ExpireEntrance();
	float x = 0;
	float y = 0;
	const auto shake = [&x, &y](float age, float length, float amplitude) {
		if (age < 0 || age > length || length <= 0)
			return;
		const float amp = amplitude * std::pow(1.0F - age / length, 1.6F);
		x += amp * (std::sin(age * 57.0F) * 0.7F + std::sin(age * 91.0F) * 0.3F);
		y += amp * 0.6F * (std::cos(age * 63.0F) * 0.7F + std::sin(age * 79.0F) * 0.3F);
	};
	if (TheEntrance.active && *sgOptions.Oracool.bossEntrance)
		shake(EntranceAge() - 0.15F, ShakeLength, 7.0F);
	shake(Now() - Kick.start, Kick.length, Kick.amplitude); // the fourth batch's kick (a guardian's fall)
	// And the fifth's camera glide after a jump (oracool/scene_feel.h): the picture eases from where it stood.
	const Displacement glide = CameraGlideOffset();
	return { static_cast<int>(std::floor(x + 0.5F)) + glide.deltaX, static_cast<int>(std::floor(y + 0.5F)) + glide.deltaY };
}

void ApplyAfterWorld(const Surface &world)
{
	if (world.isIndexed() || world.w() <= 0 || world.h() <= 0)
		return;
	const int w = world.w();
	const int h = world.h();

	// The shake: the finished picture moved, its edge rows and columns repeated into the gap (audit round 1: moving the
	// draw instead left undrawn strips at the screen's edges).
	const Displacement shake = ScreenShakeOffset();
	if ((shake.deltaX != 0 || shake.deltaY != 0) && shake.deltaX > -w && shake.deltaX < w && shake.deltaY > -h && shake.deltaY < h) {
		const size_t rowBytes = static_cast<size_t>(w) * 4;
		if (shake.deltaY > 0) {
			for (int y = h - 1; y >= shake.deltaY; y--)
				std::memcpy(world.at<uint32_t>(0, y), world.at<uint32_t>(0, y - shake.deltaY), rowBytes);
			for (int y = 0; y < shake.deltaY; y++)
				std::memcpy(world.at<uint32_t>(0, y), world.at<uint32_t>(0, shake.deltaY), rowBytes);
		} else if (shake.deltaY < 0) {
			const int d = -shake.deltaY;
			for (int y = 0; y < h - d; y++)
				std::memcpy(world.at<uint32_t>(0, y), world.at<uint32_t>(0, y + d), rowBytes);
			for (int y = h - d; y < h; y++)
				std::memcpy(world.at<uint32_t>(0, y), world.at<uint32_t>(0, h - d - 1), rowBytes);
		}
		if (shake.deltaX != 0) {
			const int d = shake.deltaX > 0 ? shake.deltaX : -shake.deltaX;
			for (int y = 0; y < h; y++) {
				uint32_t *row = world.at<uint32_t>(0, y);
				if (shake.deltaX > 0) {
					std::memmove(row + d, row, static_cast<size_t>(w - d) * 4);
					for (int x = 0; x < d; x++)
						row[x] = row[d];
				} else {
					std::memmove(row, row + d, static_cast<size_t>(w - d) * 4);
					for (int x = w - d; x < w; x++)
						row[x] = row[w - d - 1];
				}
			}
		}
	}

	// Falling rain or snow over Tristram, after the light passes, dimmed by the hour (audit round 1: drawn before them, the
	// relief and Lit Sprites took the streaks for sprites).
	if (WeatherShows()) {
		const float t = Now();
		float dim = 1.0F;
		if (*sgOptions.Oracool.dayNightCycle) {
			const LightTint now = TownAmbientAt(TownHour());
			const LightTint noon = TownAmbientAt(12.0F);
			const float a = static_cast<float>(now.r + now.g + now.b);
			const float b = static_cast<float>(noon.r + noon.g + noon.b);
			dim = b > 0 ? Clamp01(a / b) : 1.0F;
			dim = dim < 0.3F ? 0.3F : dim;
		}
		if (Current == Weather::Rain) {
			const int cr = static_cast<int>(178.0F * dim), cg = static_cast<int>(192.0F * dim), cb = static_cast<int>(212.0F * dim);
			const int drops = static_cast<int>(420.0F * Intensity);
			const float span = static_cast<float>(h + 60);
			const float spanX = static_cast<float>(w + 80);
			for (int i = 0; i < drops; i++) {
				const uint32_t s = Hash(static_cast<uint32_t>(i), 31);
				const float speed = 520.0F + Unit(s) * 260.0F;
				const int len = 9 + static_cast<int>(Unit(s >> 3) * 7.0F);
				const float y = std::fmod(Unit(Hash(s, 2)) * span + t * speed, span) - 30.0F;
				float x = Unit(Hash(s, 3)) * spanX - y * 0.18F;
				x = std::fmod(x + spanX * 4.0F, spanX) - 40.0F;
				for (int k = 0; k < len; k++) {
					const int py = static_cast<int>(y) + k;
					const int px = static_cast<int>(x + static_cast<float>(k) * 0.18F);
					if (px < 0 || py < 0 || px >= w || py >= h)
						continue;
					uint32_t &c = *world.at<uint32_t>(px, py);
					c = SFX_MIX(c, cr, cg, cb, 60 + k * 6);
				}
			}
		} else {
			const int cr = static_cast<int>(240.0F * dim), cg = static_cast<int>(244.0F * dim), cb = static_cast<int>(250.0F * dim);
			const int flakes = static_cast<int>(320.0F * Intensity);
			const float span = static_cast<float>(h + 20);
			for (int i = 0; i < flakes; i++) {
				const uint32_t s = Hash(static_cast<uint32_t>(i), 51);
				const float speed = 28.0F + Unit(s) * 34.0F;
				const float y = std::fmod(Unit(Hash(s, 2)) * span + t * speed, span) - 10.0F;
				const float drift = std::sin(t * (0.7F + Unit(s >> 5)) + Unit(Hash(s, 4)) * 6.28F) * 14.0F;
				const int px = static_cast<int>(Unit(Hash(s, 3)) * static_cast<float>(w) + drift);
				const int py = static_cast<int>(y);
				const int size = Unit(s >> 9) < 0.3F ? 2 : 1;
				for (int dy = 0; dy < size; dy++) {
					for (int dx = 0; dx < size; dx++) {
						const int x = px + dx, yy = py + dy;
						if (x >= 0 && yy >= 0 && x < w && yy < h) {
							uint32_t &c = *world.at<uint32_t>(x, yy);
							c = SFX_MIX(c, cr, cg, cb, 190);
						}
					}
				}
			}
		}
	}

	// Lightning far off on a rainy night: a double flash every half minute or so.
	if (IsUnderSky(leveltype) && *sgOptions.Oracool.weather && Current == Weather::Rain && Intensity > 0.6F && TownIsDark()) {
		const float now = Now();
		if (NextFlash < 0 || NextFlash - now > 60.0F)
			NextFlash = now + 15.0F + WeatherRand() * 30.0F;
		if (now >= NextFlash) {
			FlashAt = now;
			NextFlash = now + 18.0F + WeatherRand() * 32.0F;
		}
		const float age = now - FlashAt;
		int a = 0;
		if (age >= 0 && age < 0.07F)
			a = 97;
		else if (age >= 0.13F && age < 0.21F)
			a = 56;
		if (a > 0) {
			for (int y = 0; y < h; y++) {
				uint32_t *row = world.at<uint32_t>(0, y);
				for (int x = 0; x < w; x++) {
					const uint32_t c = row[x];
					row[x] = SFX_MIX(c, 215, 225, 255, a);
				}
			}
		}
	}

	// The low health warning (user, 2026-10-11): under a third of his life a red glow bleeds in from the edges, beating like
	// a heart - deeper, stronger and faster the nearer he is to death. Its inner edge is uneven and creeps, like blood.
	float severity = 0;
	const float beat = LowHealthHeartbeat(&severity);
	if (severity > 0) {
		const float now = Now();
		const float pulse = 0.55F + 0.45F * beat;
		const float depth = static_cast<float>(w < h ? w : h) * (0.10F + 0.22F * severity) * (0.85F + 0.15F * pulse);
		const float strength = (0.30F + 0.45F * severity) * pulse;
		const int band = static_cast<int>(depth * 1.35F) + 1;
		// The uneven inner edge, one reciprocal reach per column (top and bottom) and per row (the sides).
		static std::vector<float> ColumnInv, RowInv;
		ColumnInv.resize(static_cast<size_t>(w));
		RowInv.resize(static_cast<size_t>(h));
		for (int x = 0; x < w; x++) {
			const float along = static_cast<float>(x);
			const float edge = 0.75F + 0.5F * ValueNoise(along / 70.0F, now * 0.35F) - 0.25F * ValueNoise(along / 23.0F + 40.0F, now * 0.8F);
			ColumnInv[static_cast<size_t>(x)] = 1.0F / (depth * edge);
		}
		for (int y = 0; y < h; y++) {
			const float along = static_cast<float>(y) + 5000.0F;
			const float edge = 0.75F + 0.5F * ValueNoise(along / 70.0F, now * 0.35F) - 0.25F * ValueNoise(along / 23.0F + 40.0F, now * 0.8F);
			RowInv[static_cast<size_t>(y)] = 1.0F / (depth * edge);
		}
		const float *colInv = ColumnInv.data();
		const float *rowInv = RowInv.data();
		const float scale = strength * 256.0F;
		for (int y = 0; y < h; y++) {
			uint32_t *row = world.at<uint32_t>(0, y);
			const int fromTopOrBottom = y < h - 1 - y ? y : h - 1 - y;
			const bool inBand = fromTopOrBottom < band;
			const float rInv = rowInv[y];
			const float vRow = static_cast<float>(fromTopOrBottom);
			for (int x = 0; x < w; x++) {
				const int fromSide = x < w - 1 - x ? x : w - 1 - x;
				if (!inBand && fromSide >= band) {
					x = w - 1 - band; // the middle of the row: nothing there
					continue;
				}
				const float v = 1.0F - vRow * colInv[x];
				const float u = 1.0F - static_cast<float>(fromSide) * rInv;
				const float edge = v > u ? v : u;
				if (edge <= 0)
					continue;
				int a = static_cast<int>(edge * edge * scale);
				a = a > 230 ? 230 : a;
				const uint32_t c = row[x];
				row[x] = SFX_MIX(c, 120, 0, 4, a);
			}
		}
	}
}

void DrawGuardianEntranceCard(const Surface &out)
{
	ExpireEntrance();
	if (!TheEntrance.active || !*sgOptions.Oracool.bossEntrance || out.isIndexed())
		return;
	const float age = EntranceAge();
	if (age > CardLength)
		return;
	const float in = Smooth(age / 0.35F);
	const float outF = 1.0F - Smooth((age - (CardLength - 0.4F)) / 0.4F);
	const float k = in < outF ? in : outF;
	const int bar = static_cast<int>(static_cast<float>(out.h()) * 0.075F * k);
	const int w = out.w();
	for (int y = 0; y < bar; y++) {
		uint32_t *topRow = out.at<uint32_t>(0, y);
		uint32_t *bottomRow = out.at<uint32_t>(0, out.h() - 1 - y);
		std::fill(topRow, topRow + w, 0U);
		std::fill(bottomRow, bottomRow + w, 0U);
	}
	if (age >= 0.55F && age <= CardLength - 0.5F && !TheEntrance.name.empty()) {
		const int y = static_cast<int>(static_cast<float>(out.h()) * 0.2F);
		DrawString(out, "GUARDIAN OF THE RIFT", Rectangle { Point { 0, y - 26 }, Size { w, 20 } }, { UiFlags::AlignCenter | UiFlags::ColorWhitegold });
		DrawString(out, TheEntrance.name, Rectangle { Point { 0, y }, Size { w, 40 } }, { UiFlags::FontSize30 | UiFlags::AlignCenter | UiFlags::ColorGold });
	}
}

void ApplyCrtLook(const Surface &out, bool wholeFrameDrawn)
{
	// Only over a frame drawn whole: a part not redrawn this frame would darken again and again (audit round 1).
	if (!*sgOptions.Oracool.crtLook || !wholeFrameDrawn || out.isIndexed() || out.w() <= 0 || out.h() <= 0)
		return;
	const int w = out.w();
	const int h = out.h();
	// Per column: the corners' fall-off times each channel's phosphor stripe, ready to multiply (audit round 1).
	static std::vector<uint32_t> ColR, ColG, ColB;
	static int ColumnFor = -1;
	if (ColumnFor != w) {
		ColumnFor = w;
		ColR.resize(static_cast<size_t>(w));
		ColG.resize(static_cast<size_t>(w));
		ColB.resize(static_cast<size_t>(w));
		for (int x = 0; x < w; x++) {
			const float u = (static_cast<float>(x) + 0.5F) / static_cast<float>(w) * 2.0F - 1.0F;
			const float fall = 1.0F - 0.28F * u * u * u * u;
			const int m = x % 3;
			ColR[static_cast<size_t>(x)] = static_cast<uint32_t>(fall * (m == 0 ? 268.0F : 240.0F));
			ColG[static_cast<size_t>(x)] = static_cast<uint32_t>(fall * (m == 1 ? 268.0F : 240.0F));
			ColB[static_cast<size_t>(x)] = static_cast<uint32_t>(fall * (m == 2 ? 268.0F : 240.0F));
		}
	}
	const uint32_t *colR = ColR.data();
	const uint32_t *colG = ColG.data();
	const uint32_t *colB = ColB.data();
	for (int y = 0; y < h; y++) {
		uint32_t *row = out.at<uint32_t>(0, y);
		const float v = (static_cast<float>(y) + 0.5F) / static_cast<float>(h) * 2.0F - 1.0F;
		const uint32_t rowK = static_cast<uint32_t>((1.0F - 0.28F * v * v * v * v) * ((y & 1) != 0 ? 0.80F : 1.0F) * 256.0F);
		for (int x = 0; x < w; x++) {
			const uint32_t c = row[x];
			uint32_t r = (((c >> 16) & 0xFF) * rowK * colR[x]) >> 16;
			uint32_t g = (((c >> 8) & 0xFF) * rowK * colG[x]) >> 16;
			uint32_t b = ((c & 0xFF) * rowK * colB[x]) >> 16;
			r = r > 255 ? 255 : r;
			g = g > 255 ? 255 : g;
			b = b > 255 ? 255 : b;
			row[x] = (r << 16) | (g << 8) | b;
		}
	}
}

// ==== The guardian's entrance ===========================================================================================

void BeginGuardianEntrance(const Monster &guardian)
{
	if (!*sgOptions.Oracool.bossEntrance)
		return;
	TheEntrance.active = true;
	TheEntrance.monsterId = static_cast<int>(guardian.getId());
	TheEntrance.tile = guardian.position.tile;
	TheEntrance.start = Now();
	TheEntrance.level = LevelStamp();
	TheEntrance.name = RiftBossName();
}

bool GuardianEntranceActive()
{
	ExpireEntrance();
	return TheEntrance.active;
}

// ==== Weather ===========================================================================================================

Weather CurrentWeather(float *intensity)
{
	if (intensity != nullptr)
		*intensity = Intensity;
	return Current;
}

void SetWeather(Weather weather)
{
	Current = Target = weather;
	Intensity = weather == Weather::Clear ? 0.0F : 1.0F;
	NextChange = Now() + 300.0F;
}

int ExplorationMemoryLight(Point tile, int light)
{
	// Any explored tile is at least this bright - a faint lit edge too, or it would ring the light darker than the memory
	// beyond it (round 2 audit).
	if (light <= LightsMax - 3 || leveltype == DTYPE_TOWN || !*sgOptions.Oracool.explorationMemory)
		return light;
	if (tile.x < 0 || tile.y < 0 || tile.x >= MAXDUNX || tile.y >= MAXDUNY || !HasAnyOf(dFlags[tile.x][tile.y], DungeonFlag::Explored))
		return light;
	return LightsMax - 3;
}

void ForgetSceneFx()
{
	Ghosts.clear();
	LastHeroValid = false;
	TheEntrance.active = false;
	std::fill(Foliage.colour.begin(), Foliage.colour.end(), 0U);
	std::fill(Foliage.base.begin(), Foliage.base.end(), 0);
	Foliage.any = false;
	MonsterLastDrawn.fill(0);
	MonsterAppeared.fill(0);
	Kick = {};
	ForgetSceneLife(); // the fourth batch's records go with the third's
	ForgetSceneFeel(); // and the fifth's
	ForgetSceneGlow(); // and the sixth's
}

} // namespace devilution::oracool

#undef SFX_R
#undef SFX_G
#undef SFX_B
#undef SFX_PACK
#undef SFX_MIX
#undef SFX_SCALE
#undef SFX_SAT
#undef SFX_ADD

#ifdef _MSC_VER
#pragma optimize("", on)
#pragma runtime_checks("", restore)
#endif
