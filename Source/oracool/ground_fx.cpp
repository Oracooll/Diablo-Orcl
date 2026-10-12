/**
 * @file oracool/ground_fx.cpp
 *
 * Floor marks and loot beams - see ground_fx.h.
 */
#include "oracool/ground_fx.h"

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "diablo.h"
#include "engine/surface.hpp"
#include "items.h"
#include "levels/gendung.h"
#include "lighting.h"
#include "missiles.h"
#include "monster.h"
#include "monstdat.h"
#include "options.h"
#include "oracool/companion.h"
#include "oracool/missile_tint.h"
#include "oracool/particles.h"
#include "oracool/runewords.h"
#include "oracool/splats.h"

// Every mark and beam is a loop over its pixels, from the Debug build: optimised even there (see colour_light.cpp).
#ifdef _MSC_VER
#pragma runtime_checks("", off)
#pragma optimize("gt", on)
#endif

namespace devilution::oracool {

namespace {

constexpr int RimBins = 32;

struct Mark {
	float x;
	float y;
	float radius;
	float born; // on the marks' clock
	float life; // seconds
	uint32_t seed;
	FloorMarkKind kind;
	std::array<uint8_t, RimBins> rim; // the ragged edge: the blot's reach at each angle, 128 = its radius
};

std::array<Mark, MaxFloorMarks> Marks;
size_t Count = 0;
size_t Oldest = 0; // when full, the next to give way
float Clock = 0;
uint32_t LastTicks = 0;
uint32_t LevelKey = 0xFFFFFFFFU;
uint32_t Rng = 0x9E3779B9U;
uint64_t LootKey = 0;

uint32_t Next()
{
	Rng ^= Rng << 13;
	Rng ^= Rng >> 17;
	Rng ^= Rng << 5;
	return Rng;
}

float Frand()
{
	return static_cast<float>(Next() & 0xFFFFFF) / 16777216.0F;
}

uint32_t CurrentLevelKey()
{
	return static_cast<uint32_t>(leveltype) | (static_cast<uint32_t>(currlevel) << 8) | (static_cast<uint32_t>(setlevel ? 1 : 0) << 16)
	    | (static_cast<uint32_t>(setlvlnum) << 24);
}

float LifeOf(FloorMarkKind kind)
{
	switch (kind) {
	case FloorMarkKind::Blood: return 120.0F;
	case FloorMarkKind::BoneDust: return 60.0F;
	case FloorMarkKind::Scorch: return 90.0F;
	case FloorMarkKind::Frost: return 45.0F; // it melts
	case FloorMarkKind::Acid: return 60.0F;
	case FloorMarkKind::Clay: return 40.0F;
	}
	return 60.0F;
}

void Prune();

/** The marks' clock: real time, a step clamped to 50 ms, stopped while paused. */
void AdvanceClock()
{
	const uint32_t now = SDL_GetTicks();
	float dt = LastTicks == 0 ? 0.0F : static_cast<float>(now - LastTicks) / 1000.0F;
	LastTicks = now == 0 ? 1 : now;
	dt = dt < 0 ? 0 : (dt > 0.05F ? 0.05F : dt);
	if (PauseMode != 0)
		dt = 0;
	Clock += dt;
	Prune();
}

/** The dead fall out of the pool, keeping its order oldest first. */
void Prune()
{
	size_t kept = 0;
	for (size_t i = 0; i < Count; i++) {
		const size_t at = (Oldest + i) % MaxFloorMarks;
		if (Clock - Marks[at].born >= Marks[at].life)
			continue;
		const size_t to = (Oldest + kept) % MaxFloorMarks;
		if (to != at)
			Marks[to] = Marks[at];
		kept++;
	}
	Count = kept;
}

void NoteLevel()
{
	const uint32_t key = CurrentLevelKey();
	if (key != LevelKey) {
		LevelKey = key;
		ForgetFloorMarks();
	}
}

/** Where a tile coordinate lands: the world draw's first tile's centre is 32 px right of and 14.5 px above its start. */
struct Projection {
	float cx, cy, fx, fy;
	[[nodiscard]] float sx(float x, float y) const { return cx + ((x - fx) - (y - fy)) * 32.0F; }
	[[nodiscard]] float sy(float x, float y) const { return cy + ((x - fx) + (y - fy)) * 16.0F; }
};

Projection MakeProjection(Point firstTile, Point firstTileBuffer)
{
	return { static_cast<float>(firstTileBuffer.x) + 32.0F, static_cast<float>(firstTileBuffer.y) - 14.5F, static_cast<float>(firstTile.x), static_cast<float>(firstTile.y) };
}

void DrawMark(const Surface &out, const Projection &p, const Mark &m)
{
	const float age = Clock - m.born;
	// Blood spreads for its first second and a half; the rest are there at once. Every mark fades over its last quarter.
	float grow = 1.0F;
	if (m.kind == FloorMarkKind::Blood && age < 1.5F)
		grow = 0.35F + 0.65F * (age / 1.5F);
	float fade = (m.life - age) / (m.life * 0.25F);
	fade = fade > 1 ? 1 : (fade < 0 ? 0 : fade);
	if (age < 0.25F)
		fade *= age / 0.25F; // and it comes in, not pops
	if (fade <= 0.01F)
		return;
	const float rx = m.radius * grow * 32.0F;
	const float ry = m.radius * grow * 16.0F;
	const float sx = p.sx(m.x, m.y);
	const float sy = p.sy(m.x, m.y);
	const int w = out.w();
	const int h = out.h();
	const int left = static_cast<int>(sx - rx * 1.3F);
	const int right = static_cast<int>(sx + rx * 1.3F) + 1;
	const int top = static_cast<int>(sy - ry * 1.3F);
	const int bottom = static_cast<int>(sy + ry * 1.3F) + 1;
	if (right < 0 || bottom < 0 || left >= w || top >= h || rx < 1 || ry < 1)
		return;

	// The floor tiles around it (a mark never lies on a wall, lava or water), as a 7x7 window on dPiece.
	const int tx = static_cast<int>(std::floor(m.x + 0.5F));
	const int ty = static_cast<int>(std::floor(m.y + 0.5F));
	bool floor[7][7];
	for (int i = 0; i < 7; i++) {
		for (int j = 0; j < 7; j++) {
			const int x = tx + i - 3;
			const int y = ty + j - 3;
			floor[i][j] = x >= 0 && y >= 0 && x < MAXDUNX && y < MAXDUNY && !TileHasAny(dPiece[x][y], TileProperties::Solid);
		}
	}

	const int fadeK = static_cast<int>(fade * 256.0F);
	const float invRx = 1.0F / rx;
	const float invRy = 1.0F / ry;
	const int x0 = left < 0 ? 0 : left;
	const int x1 = right > w ? w : right;
	const int y0 = top < 0 ? 0 : top;
	const int y1 = bottom > h ? h : bottom;
	const FloorMarkKind kind = m.kind;
	const int isx = static_cast<int>(sx);
	const int isy = static_cast<int>(sy);
	for (int py = y0; py < y1; py++) {
		uint32_t *row = out.at<uint32_t>(0, py);
		const float ny = (static_cast<float>(py) + 0.5F - sy) * invRy;
		for (int px = x0; px < x1; px++) {
			const float nx = (static_cast<float>(px) + 0.5F - sx) * invRx;
			const float d2 = nx * nx + ny * ny;
			if (d2 > 1.69F)
				continue;
			// The floor tile under this pixel: the inverse of the projection, relative to the window's corner (which keeps
			// it positive, so a cast floors it). Written out, as every per-pixel step here is: /Ob0 inlines no call.
			const float rxp = static_cast<float>(px) + 0.5F - p.cx;
			const float ryp = static_cast<float>(py) + 0.5F - p.cy;
			const float ru = p.fx + rxp * (1.0F / 64.0F) + ryp * (1.0F / 32.0F) + 0.5F - static_cast<float>(tx - 3);
			const float rv = p.fy + ryp * (1.0F / 32.0F) - rxp * (1.0F / 64.0F) + 0.5F - static_cast<float>(ty - 3);
			if (ru < 0 || rv < 0 || ru >= 7 || rv >= 7)
				continue;
			if (!floor[static_cast<int>(ru)][static_cast<int>(rv)])
				continue;
			const float d = std::sqrt(d2);
			// The rim at this angle, between its two bins: a pseudo-angle in [0, 4), no trigonometry.
			const float ax = nx < 0 ? -nx : nx;
			const float ay = ny < 0 ? -ny : ny;
			const float sum = ax + ay;
			float diamond = 0;
			if (sum > 1e-6F) {
				if (ny >= 0)
					diamond = nx >= 0 ? ny / sum : 1.0F - nx / sum;
				else
					diamond = nx < 0 ? 2.0F - ny / sum : 3.0F + nx / sum;
			}
			const float a = diamond * (RimBins / 4.0F);
			const int ai = static_cast<int>(a);
			const int b0 = ai & (RimBins - 1);
			const int b1 = (b0 + 1) & (RimBins - 1);
			const float fa = a - static_cast<float>(ai);
			const float rim = (static_cast<float>(m.rim[b0]) * (1.0F - fa) + static_cast<float>(m.rim[b1]) * fa) / 128.0F;
			float alpha = (rim - d) * 5.0F; // a soft edge a fifth of the radius wide
			if (alpha <= 0)
				continue;
			alpha = alpha > 1 ? 1 : alpha;
			// A grain, fixed to the mark (not the screen), so it holds still as the view scrolls.
			uint32_t grain = static_cast<uint32_t>((px - isx) >> 1) * 374761393U + static_cast<uint32_t>((py - isy) >> 1) * 668265263U + m.seed * 2246822519U;
			grain = (grain ^ (grain >> 13)) * 1274126177U;
			grain = (grain ^ (grain >> 16)) & 0xFF;
			float k;
			switch (kind) {
			case FloorMarkKind::Scorch: k = alpha * (1.0F - 0.45F * d) * (0.75F + static_cast<float>(grain) / 1020.0F); break;
			case FloorMarkKind::BoneDust: k = alpha * 0.55F * (0.6F + static_cast<float>(grain) / 640.0F); break;
			default: k = alpha * (0.82F + static_cast<float>(grain) / 1400.0F); break;
			}
			const int ka = static_cast<int>(k * static_cast<float>(fadeK));
			if (ka <= 0)
				continue;
			const uint32_t c = row[px];
			const int r = static_cast<int>((c >> 16) & 0xFF);
			const int g = static_cast<int>((c >> 8) & 0xFF);
			const int bl = static_cast<int>(c & 0xFF);
			// The mark's colour is worked out from the floor's own, so the light already on the floor stays on it.
			int tr, tg, tb;
			switch (kind) {
			case FloorMarkKind::Blood:
				tr = r * 150 >> 8, tg = g * 22 >> 8, tb = bl * 20 >> 8;
				break;
			case FloorMarkKind::BoneDust: {
				const int l = (r * 77 + g * 150 + bl * 29) >> 8;
				tr = l * 300 >> 8, tg = l * 288 >> 8, tb = l * 250 >> 8;
				break;
			}
			case FloorMarkKind::Scorch:
				tr = r * 54 >> 8, tg = g * 48 >> 8, tb = bl * 44 >> 8;
				break;
			case FloorMarkKind::Frost: {
				const int l = (r * 77 + g * 150 + bl * 29) >> 8;
				tr = l * 230 >> 8, tg = l * 300 >> 8, tb = l * 390 >> 8;
				break;
			}
			case FloorMarkKind::Clay:
				tr = 118 + (r >> 2), tg = 84 + (g >> 2), tb = 54 + (bl >> 2);
				break;
			case FloorMarkKind::Acid:
			default: {
				const int l = (r * 77 + g * 150 + bl * 29) >> 8;
				tr = l * 120 >> 8, tg = l * 290 >> 8, tb = l * 90 >> 8;
				break;
			}
			}
			tr = tr > 255 ? 255 : tr;
			tg = tg > 255 ? 255 : tg;
			tb = tb > 255 ? 255 : tb;
			const int nr = r + (((tr - r) * ka) >> 8);
			const int ng = g + (((tg - g) * ka) >> 8);
			const int nb = bl + (((tb - bl) * ka) >> 8);
			row[px] = (static_cast<uint32_t>(nr) << 16) | (static_cast<uint32_t>(ng) << 8) | static_cast<uint32_t>(nb);
		}
	}
}

/** Additive light onto a pixel, each channel saturating. */
inline void AddLight(uint32_t &c, int ar, int ag, int ab)
{
	int r = static_cast<int>((c >> 16) & 0xFF) + ar;
	int g = static_cast<int>((c >> 8) & 0xFF) + ag;
	int b = static_cast<int>(c & 0xFF) + ab;
	r = r > 255 ? 255 : r;
	g = g > 255 ? 255 : g;
	b = b > 255 ? 255 : b;
	c = (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
}

float Pulse(const Item &item)
{
	const float phase = static_cast<float>(static_cast<uint32_t>(item._iSeed) % 6283U) / 1000.0F;
	return 0.85F + 0.15F * std::sin(static_cast<float>(SDL_GetTicks() % 3600000U) / 1000.0F * 2.6F + phase);
}

void DrawLootGlow(const Surface &out, const Projection &p, const Item &item, const LootLook &look)
{
	const float sx = p.sx(static_cast<float>(item.position.x), static_cast<float>(item.position.y));
	const float sy = p.sy(static_cast<float>(item.position.x), static_cast<float>(item.position.y));
	constexpr float Rx = 26.0F;
	constexpr float Ry = 13.0F;
	const int w = out.w();
	const int h = out.h();
	const int x0 = std::max(static_cast<int>(sx - Rx), 0);
	const int x1 = std::min(static_cast<int>(sx + Rx) + 1, w);
	const int y0 = std::max(static_cast<int>(sy - Ry), 0);
	const int y1 = std::min(static_cast<int>(sy + Ry) + 1, h);
	const float k = look.strength * Pulse(item) * 0.55F;
	const float cr = static_cast<float>((look.rgb >> 16) & 0xFF) * k;
	const float cg = static_cast<float>((look.rgb >> 8) & 0xFF) * k;
	const float cb = static_cast<float>(look.rgb & 0xFF) * k;
	for (int py = y0; py < y1; py++) {
		uint32_t *row = out.at<uint32_t>(0, py);
		const float ny = (static_cast<float>(py) + 0.5F - sy) / Ry;
		for (int px = x0; px < x1; px++) {
			const float nx = (static_cast<float>(px) + 0.5F - sx) / Rx;
			float f = 1.0F - (nx * nx + ny * ny);
			if (f <= 0)
				continue;
			f *= f;
			AddLight(row[px], static_cast<int>(cr * f), static_cast<int>(cg * f), static_cast<int>(cb * f));
		}
	}
}

void DrawLootBeam(const Surface &out, const Projection &p, const Item &item, const LootLook &look)
{
	const float sx = p.sx(static_cast<float>(item.position.x), static_cast<float>(item.position.y));
	const float base = p.sy(static_cast<float>(item.position.x), static_cast<float>(item.position.y));
	const float height = 180.0F * look.height;
	constexpr int HalfWidth = 9;
	const int w = out.w();
	const int h = out.h();
	const int cx = static_cast<int>(sx);
	const int top = std::max(static_cast<int>(base - height), 0);
	const int bottom = std::min(static_cast<int>(base), h - 1);
	if (cx + HalfWidth < 0 || cx - HalfWidth >= w || top > bottom)
		return;
	const float pulse = Pulse(item);
	const float seconds = static_cast<float>(SDL_GetTicks() % 3600000U) / 1000.0F;
	const float k = look.strength * pulse;
	const float cr = static_cast<float>((look.rgb >> 16) & 0xFF);
	const float cg = static_cast<float>((look.rgb >> 8) & 0xFF);
	const float cb = static_cast<float>(look.rgb & 0xFF);
	// The horizontal profile: a soft column, brightest down its middle.
	float across[2 * HalfWidth + 1];
	for (int dx = -HalfWidth; dx <= HalfWidth; dx++) {
		const float t = 1.0F - static_cast<float>(dx < 0 ? -dx : dx) / static_cast<float>(HalfWidth + 1);
		across[dx + HalfWidth] = t * t;
	}
	std::vector<Splat> *splats = SplatCollection();
	for (int py = top; py <= bottom; py++) {
		uint32_t *row = out.at<uint32_t>(0, py);
		const float t = (base - static_cast<float>(py)) / height; // 0 at the floor, 1 at the top
		float up = 1.0F - t;
		up = up < 0 ? 0 : (up > 1 ? 1 : up); // the top row is a fraction past the height (audit round 1: a NaN)
		up = up * std::sqrt(up); // fades as it rises
		// Light running up the beam.
		const float shimmer = 0.85F + 0.15F * std::sin(static_cast<float>(py) * 0.13F + seconds * 5.5F);
		const float v = k * up * shimmer;
		for (int dx = -HalfWidth; dx <= HalfWidth; dx++) {
			const int px = cx + dx;
			if (px < 0 || px >= w)
				continue;
			const float f = v * across[dx + HalfWidth] * 0.6F;
			// The core runs toward white.
			const float white = (dx >= -1 && dx <= 1) ? v * 70.0F : 0.0F;
			const int ar = static_cast<int>(cr * f + white), ag = static_cast<int>(cg * f + white), ab = static_cast<int>(cb * f + white);
			if (splats != nullptr) {
				// The card adds it, saturating as AddLight does (oracool/splats.h).
				splats->push_back({ static_cast<float>(px), static_cast<float>(py), static_cast<float>(std::min(ar, 255)) / 255.0F,
				    static_cast<float>(std::min(ag, 255)) / 255.0F, static_cast<float>(std::min(ab, 255)) / 255.0F, 0.0F });
				continue;
			}
			AddLight(row[px], ar, ag, ab);
		}
	}
}

} // namespace

namespace {
std::vector<Splat> *Splats = nullptr;
} // namespace

void BeginSplatCollection(std::vector<Splat> *into)
{
	Splats = into;
}

void EndSplatCollection()
{
	Splats = nullptr;
}

std::vector<Splat> *SplatCollection()
{
	return Splats;
}

void AddFloorMark(float x, float y, FloorMarkKind kind, float radius)
{
	if (!*sgOptions.Oracool.floorMarks || radius <= 0)
		return;
	NoteLevel();
	// A fire wall or a run of explosions on one spot: one mark of a kind per spot per second, or the pool fills there.
	for (size_t i = 0; i < Count; i++) {
		const Mark &m = Marks[(Oldest + i) % MaxFloorMarks];
		if (m.kind == kind && Clock - m.born < 1.0F && (m.x - x) * (m.x - x) + (m.y - y) * (m.y - y) < 0.36F)
			return;
	}
	size_t at;
	if (Count < MaxFloorMarks) {
		at = (Oldest + Count) % MaxFloorMarks;
		Count++;
	} else {
		at = Oldest;
		Oldest = (Oldest + 1) % MaxFloorMarks;
	}
	Mark &m = Marks[at];
	m.x = x;
	m.y = y;
	m.radius = radius;
	m.born = Clock;
	m.life = LifeOf(kind);
	m.seed = Next();
	m.kind = kind;
	// The rim: a few lobes from the seed, smoothed round the circle. Scorch is the most ragged (a splash of soot).
	const float ragged = kind == FloorMarkKind::Scorch ? 0.32F : (kind == FloorMarkKind::Frost ? 0.24F : 0.18F);
	const float p1 = Frand() * 6.2831853F, p2 = Frand() * 6.2831853F, p3 = Frand() * 6.2831853F;
	for (int i = 0; i < RimBins; i++) {
		const float a = static_cast<float>(i) / RimBins * 6.2831853F;
		float r = 1.0F + ragged * (0.5F * std::sin(a * 2.0F + p1) + 0.3F * std::sin(a * 3.0F + p2) + 0.2F * std::sin(a * 5.0F + p3));
		r += ragged * 0.5F * (Frand() - 0.5F);
		// Within the 1.3 the pixel loop reaches, less the soft edge (audit round 1: a lobe was cut off square).
		r = r < 0.4F ? 0.4F : (r > 1.22F ? 1.22F : r);
		m.rim[static_cast<size_t>(i)] = static_cast<uint8_t>(r * 128.0F);
	}
}

void AddDeathMark(const Monster &monster)
{
	if (!*sgOptions.Oracool.floorMarks)
		return;
	// The hero's own - a golem recast, a minion dismissed, a Decoy or a companion whose time is up - do not bleed (audit
	// round 1): MonsterDeath is how they leave too.
	if (monster.isPlayerMinion() || IsCompanion(monster))
		return;
	const bool undead = monster.data().monsterClass == MonsterClass::Undead;
	const float jitter = (Frand() - 0.5F) * 0.3F;
	const float radius = (monster.isUnique() ? 0.95F : 0.6F) + Frand() * 0.2F;
	AddFloorMark(static_cast<float>(monster.position.tile.x) + jitter, static_cast<float>(monster.position.tile.y) - jitter,
	    undead ? FloorMarkKind::BoneDust : FloorMarkKind::Blood, undead ? radius * 0.85F : radius);
}

bool ExplosionMarkFor(uint32_t rgb, FloorMarkKind &kind, float &radius)
{
	if (rgb == hue::LightningBlue) {
		kind = FloorMarkKind::Scorch;
		radius = 0.45F;
		return true;
	}
	if (rgb == hue::IceBlue) {
		kind = FloorMarkKind::Frost;
		radius = 0.85F;
		return true;
	}
	if (rgb == hue::FireOrange || rgb == hue::Infrared) {
		kind = FloorMarkKind::Scorch;
		radius = 0.8F;
		return true;
	}
	if (rgb == hue::PoisonGreen) {
		kind = FloorMarkKind::Acid;
		radius = 0.7F;
		return true;
	}
	if (rgb == hue::BloodRed) {
		kind = FloorMarkKind::Blood;
		radius = 0.6F;
		return true;
	}
	// A tinted missile's own hue: by its strongest channel. Gold, violet, white and steel leave nothing.
	const int r = static_cast<int>((rgb >> 16) & 0xFF);
	const int g = static_cast<int>((rgb >> 8) & 0xFF);
	const int b = static_cast<int>(rgb & 0xFF);
	if (r > 200 && g < r * 6 / 10 && b < r * 4 / 10) {
		kind = FloorMarkKind::Scorch;
		radius = 0.75F;
		return true;
	}
	if (g > r + 40 && g > b + 40) {
		kind = FloorMarkKind::Acid;
		radius = 0.65F;
		return true;
	}
	if (b > 200 && g > 170 && r < g) {
		kind = FloorMarkKind::Frost;
		radius = 0.8F;
		return true;
	}
	return false;
}

void AddExplosionMark(const Missile &missile)
{
	if (!*sgOptions.Oracool.floorMarks)
		return;
	FloorMarkKind kind;
	float radius;
	if (!ExplosionMarkFor(MissileSparkRgb(missile), kind, radius))
		return;
	// The sprite's pixel offset from its tile, back into tile units (one tile +x is (+32, +16) on screen).
	const float ox = static_cast<float>(missile.position.offset.deltaX);
	const float oy = static_cast<float>(missile.position.offset.deltaY);
	AddFloorMark(static_cast<float>(missile.position.tile.x) + ox / 64.0F + oy / 32.0F, static_cast<float>(missile.position.tile.y) + oy / 32.0F - ox / 64.0F, kind, radius);
}

void ForgetFloorMarks()
{
	Count = 0;
	Oldest = 0;
}

size_t FloorMarkCount()
{
	return Count;
}

void AgeFloorMarks(float seconds)
{
	Clock += seconds;
	Prune();
}

LootLook LootLookFor(const Item &item)
{
	if (item.isEmpty())
		return { 0, 0, 0 };
	if (item.hasOracoolTier()) {
		switch (item._iOracoolTier) {
		case OracoolItemTier::Set: return { 0x46E35A, 1.0F, 1.0F };
		case OracoolItemTier::Primal: return { 0xFF8F2E, 1.15F, 1.0F };
		case OracoolItemTier::BuffedUnique: return { 0xFFF1C2, 1.05F, 1.0F };
		case OracoolItemTier::Rare: return { 0xFFDF4A, 0.6F, 0.6F };
		case OracoolItemTier::None: break;
		}
	}
	if (item._iSocketCount > 0 && GetActiveRuneword(item) != nullptr)
		return { 0xE8C468, 1.0F, 1.0F };
	if (item._iMagical == ITEM_QUALITY_UNIQUE)
		return { 0xE8C468, 1.0F, 1.0F };
	// Magic items too (user, 2026-10-11: "add glow and loot beams also to blue items"): the label's blue, the smallest beam.
	if (item._iMagical == ITEM_QUALITY_MAGIC)
		return { 0x6A8CFF, 0.5F, 0.5F };
	return { 0, 0, 0 };
}

bool LootShows(const Item &item)
{
	if (item._iAnimFlag) // still tumbling: the beam rises when it lands
		return false;
	const Point at = item.position;
	if (at.x < 0 || at.y < 0 || at.x >= MAXDUNX || at.y >= MAXDUNY)
		return false;
	return leveltype == DTYPE_TOWN || dLight[at.x][at.y] < LightsMax;
}

void DrawFloorMarks(const Surface &out, Point firstTile, Point firstTileBuffer)
{
	if (out.isIndexed() || out.w() <= 0 || out.h() <= 0)
		return;
	NoteLevel();
	const Projection p = MakeProjection(firstTile, firstTileBuffer);
	if (*sgOptions.Oracool.floorMarks) {
		AdvanceClock();
		for (size_t i = 0; i < Count; i++)
			DrawMark(out, p, Marks[(Oldest + i) % MaxFloorMarks]);
	} else {
		ForgetFloorMarks();
	}
	// The loot's coloured light lives in the colour field, rebuilt only when a light changes: an item landing or picked up
	// changes no light (audit round 1: a gold patch stayed on the floor where a unique had been). Hashed here, and a
	// change asks for the rebuild.
	uint64_t lootKey = 1469598103934665603ULL;
	if (*sgOptions.Oracool.lootBeams) {
		for (uint8_t i = 0; i < ActiveItemCount; i++) {
			const Item &item = Items[ActiveItems[i]];
			const LootLook look = LootLookFor(item);
			if (look.rgb == 0 || !LootShows(item))
				continue;
			DrawLootGlow(out, p, item, look);
			lootKey = (lootKey ^ (static_cast<uint64_t>(item.position.x) | (static_cast<uint64_t>(item.position.y) << 8) | (static_cast<uint64_t>(look.rgb) << 16))) * 1099511628211ULL;
		}
	}
	if (lootKey != LootKey) {
		LootKey = lootKey;
		UpdateLighting = true;
	}
}

void DrawLootBeams(const Surface &out, Point firstTile, Point firstTileBuffer)
{
	if (!*sgOptions.Oracool.lootBeams || out.isIndexed() || out.w() <= 0 || out.h() <= 0)
		return;
	const Projection p = MakeProjection(firstTile, firstTileBuffer);
	for (uint8_t i = 0; i < ActiveItemCount; i++) {
		const Item &item = Items[ActiveItems[i]];
		const LootLook look = LootLookFor(item);
		if (look.rgb != 0 && LootShows(item))
			DrawLootBeam(out, p, item, look);
	}
}

} // namespace devilution::oracool

#ifdef _MSC_VER
#pragma optimize("", on)
#pragma runtime_checks("", restore)
#endif
