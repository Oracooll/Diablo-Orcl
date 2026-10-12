/**
 * @file oracool/scene_glow.cpp
 *
 * The sixth graphics batch (oracool/scene_glow.h). Visual only: it reads the game, never writes it, and its random numbers
 * are its own. The pixel loops do their channel arithmetic in macros, as the other scene_* files do (the Debug build is
 * compiled with /Ob0).
 */
#include "oracool/scene_glow.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <vector>

#include "dead.h"
#include "diablo.h"
#include "engine/render/clx_render.hpp"
#include "engine/render/scrollrt.h"
#include "engine/surface.hpp"
#include "items.h"
#include "levels/gendung.h"
#include "levels/trigs.h"
#include "lighting.h"
#include "missiles.h"
#include "monster.h"
#include "objects.h"
#include "options.h"
#include "oracool/aura_ground.h"
#include "oracool/chill.h"
#include "oracool/colour_light.h"
#include "oracool/gpu_post.h"
#include "oracool/ground_fx.h"
#include "oracool/rfa12_actives.h"
#include "oracool/rift.h"
#include "oracool/scene_fx.h"
#include "oracool/scene_life.h"
#include "oracool/splats.h"
#include "oracool/whirl_arcs.h"
#include "oracool/whirlwind.h"
#include "oracool/furious_charge.h"
#include "oracool/warcries.h"
#include "player.h"
#include "spelldat.h"
#include "towners.h"

#ifdef _MSC_VER
#pragma runtime_checks("", off)
#pragma optimize("gt", on)
#endif

#define SG_R(c) (static_cast<int>(((c) >> 16) & 0xFF))
#define SG_G(c) (static_cast<int>(((c) >> 8) & 0xFF))
#define SG_B(c) (static_cast<int>((c) & 0xFF))
#define SG_PACK(r, g, b) ((static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b))
#define SG_MIX(c, r, g, b, a) \
	SG_PACK(SG_R(c) + ((((r)-SG_R(c)) * (a)) >> 8), SG_G(c) + ((((g)-SG_G(c)) * (a)) >> 8), SG_B(c) + ((((b)-SG_B(c)) * (a)) >> 8))
#define SG_SAT(v) ((v) > 255 ? 255 : (v))
#define SG_ADD(c, r, g, b) SG_PACK(SG_SAT(SG_R(c) + (r)), SG_SAT(SG_G(c) + (g)), SG_SAT(SG_B(c) + (b)))

namespace devilution::oracool {

namespace {

constexpr float Pi = 3.14159265F;
constexpr float TwoPi = 6.2831853F;

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

float Frac(float v)
{
	return v - std::floor(v);
}

float Now()
{
	return SceneSeconds();
}

uint32_t Rng = 0x2F6B1D93U;

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

float WorldX(float x, float y)
{
	return (x - y) * 32.0F;
}

float WorldY(float x, float y)
{
	return (x + y) * 16.0F;
}

float WorldX(Point tile)
{
	return static_cast<float>(tile.x - tile.y) * 32.0F;
}

float WorldY(Point tile)
{
	return static_cast<float>(tile.x + tile.y) * 16.0F;
}

bool InMap(Point tile)
{
	return tile.x >= 0 && tile.y >= 0 && tile.x < MAXDUNX && tile.y < MAXDUNY;
}

bool Walkable(Point tile)
{
	return InMap(tile) && !TileHasAny(dPiece[tile.x][tile.y], TileProperties::Solid);
}

int Chebyshev(Point a, Point b)
{
	return std::max(std::abs(a.x - b.x), std::abs(a.y - b.y));
}

/** How lit a tile is now, 0 (dark) .. 1 (full light); town is lit. */
float LitAt(Point tile)
{
	if (!InMap(tile))
		return 0;
	if (leveltype == DTYPE_TOWN)
		return 1;
	const float l = 1.0F - static_cast<float>(dLight[tile.x][tile.y]) / static_cast<float>(LightsMax);
	return l < 0 ? 0 : l;
}

struct View {
	float ox = 0, oy = 0;
	int w = 0, h = 0;
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

bool OnScreen(const View &v, float sx, float sy, float margin)
{
	return sx > -margin && sy > -margin && sx < static_cast<float>(v.w) + margin && sy < static_cast<float>(v.h) + margin;
}

struct LightPen {
	const Surface *out = nullptr;
	std::vector<Splat> *splats = nullptr;
	int w = 0, h = 0;
	void add(int px, int py, int r, int g, int b) const
	{
		if (px < 0 || py < 0 || px >= w || py >= h || (r | g | b) <= 0)
			return;
		r = SG_SAT(r);
		g = SG_SAT(g);
		b = SG_SAT(b);
		if (splats != nullptr) {
			splats->push_back({ static_cast<float>(px), static_cast<float>(py), static_cast<float>(r) / 255.0F, static_cast<float>(g) / 255.0F,
			    static_cast<float>(b) / 255.0F, 0.0F });
			return;
		}
		uint32_t &c = *out->at<uint32_t>(px, py);
		c = SG_ADD(c, r, g, b);
	}
	void addF(int px, int py, uint32_t rgb, float k) const
	{
		if (k <= 0.004F)
			return;
		add(px, py, static_cast<int>(static_cast<float>(SG_R(rgb)) * k), static_cast<int>(static_cast<float>(SG_G(rgb)) * k), static_cast<int>(static_cast<float>(SG_B(rgb)) * k));
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

/** Splats are written here first and appended in one copy (audit round 2: resize() built each Splat by a call in Debug). */
std::vector<Splat> Scratch;

Splat *ScratchFor(size_t n)
{
	if (Scratch.size() < n)
		Scratch.resize(n); // grows a few times, then never again
	return Scratch.data();
}

void AppendScratch(std::vector<Splat> *splats, const Splat *end)
{
	if (end > Scratch.data())
		splats->insert(splats->end(), static_cast<const Splat *>(Scratch.data()), end);
}

/**
 * A soft elliptical pool of light: (1 - d)^2 from the middle, @p rx across and @p ry down. Written straight onto the
 * picture, row by row, when nothing collects splats (the per-pixel work inline: the Debug build makes no inline calls).
 */
void Pool(const LightPen &pen, int cx, int cy, int rx, int ry, uint32_t rgb, float k)
{
	if (k <= 0.004F || rx <= 0 || ry <= 0)
		return;
	if (cx + rx < 0 || cy + ry < 0 || cx - rx >= pen.w || cy - ry >= pen.h)
		return;
	const int kr = static_cast<int>(static_cast<float>(SG_R(rgb)) * k * 256.0F);
	const int kg = static_cast<int>(static_cast<float>(SG_G(rgb)) * k * 256.0F);
	const int kb = static_cast<int>(static_cast<float>(SG_B(rgb)) * k * 256.0F);
	const float invX = 1.0F / static_cast<float>(rx);
	const float invY = 1.0F / static_cast<float>(ry);
	// On the GPU path the splats are written through a raw pointer into room made once (audit round 1: a push_back a
	// pixel cost ~35 ns in Debug, ~1.5 ms a frame with a pack of burning monsters), and the vector cut to what was written.
	std::vector<Splat> *splats = pen.splats;
	Splat *next = splats != nullptr ? ScratchFor(static_cast<size_t>(2 * rx + 1) * static_cast<size_t>(2 * ry + 1)) : nullptr;
	for (int dy = -ry; dy <= ry; dy++) {
		const int py = cy + dy;
		if (py < 0 || py >= pen.h)
			continue;
		const float fy = static_cast<float>(dy) * invY;
		const float fy2 = fy * fy;
		uint32_t *row = splats == nullptr ? pen.out->at<uint32_t>(0, py) : nullptr;
		const int x0 = cx - rx < 0 ? -cx : -rx;
		const int x1 = cx + rx >= pen.w ? pen.w - 1 - cx : rx;
		for (int dx = x0; dx <= x1; dx++) {
			const float fx = static_cast<float>(dx) * invX;
			const float d2 = fx * fx + fy2;
			if (d2 >= 1.0F)
				continue;
			// (1 - d)^2 without the root: 1 - d2 squared is close enough for a soft pool, and it is free.
			const float f = (1.0F - d2) * (1.0F - d2);
			const int f8 = static_cast<int>(f * 256.0F);
			int ar = (kr * f8) >> 16, ag = (kg * f8) >> 16, ab = (kb * f8) >> 16;
			if ((ar | ag | ab) <= 0)
				continue;
			const int px = cx + dx;
			if (row != nullptr) {
				const uint32_t c = row[px];
				row[px] = SG_ADD(c, ar, ag, ab);
			} else {
				ar = SG_SAT(ar);
				ag = SG_SAT(ag);
				ab = SG_SAT(ab);
				next->x = static_cast<float>(px);
				next->y = static_cast<float>(py);
				next->r = static_cast<float>(ar) * (1.0F / 255.0F);
				next->g = static_cast<float>(ag) * (1.0F / 255.0F);
				next->b = static_cast<float>(ab) * (1.0F / 255.0F);
				next->cover = 0.0F;
				next++;
			}
		}
	}
	if (splats != nullptr)
		AppendScratch(splats, next);
}

/** A small round dot of light, @p r pixels. */
void Dot(const LightPen &pen, int cx, int cy, int r, uint32_t rgb, float k)
{
	if (r <= 0) {
		pen.addF(cx, cy, rgb, k);
		return;
	}
	for (int dy = -r; dy <= r; dy++)
		for (int dx = -r; dx <= r; dx++) {
			const int d2 = dx * dx + dy * dy;
			if (d2 <= r * r)
				pen.addF(cx + dx, cy + dy, rgb, k * (1.0F - static_cast<float>(d2) / static_cast<float>(r * r + 1)));
		}
}

/** A four-pointed star: a glint. */
void Star(const LightPen &pen, int cx, int cy, int arm, uint32_t rgb, float k)
{
	pen.addF(cx, cy, SG_PACK(255, 255, 255), k);
	for (int i = 1; i <= arm; i++) {
		const float f = k * (1.0F - static_cast<float>(i) / static_cast<float>(arm + 1));
		pen.addF(cx + i, cy, rgb, f);
		pen.addF(cx - i, cy, rgb, f);
		pen.addF(cx, cy + i, rgb, f * 0.8F);
		pen.addF(cx, cy - i, rgb, f * 0.8F);
	}
}

void Blend(const Surface &out, int x, int y, uint32_t rgb, int a)
{
	if (x < 0 || y < 0 || x >= out.w() || y >= out.h() || a <= 0)
		return;
	a = a > 256 ? 256 : a;
	uint32_t &c = *out.at<uint32_t>(x, y);
	c = SG_MIX(c, SG_R(rgb), SG_G(rgb), SG_B(rgb), a);
}

/** A hero's place in world pixels, his walk between tiles included (as scene_feel's camera has it). */
void HeroWorld(const Player &player, float &x, float &y)
{
	const Displacement walk = player.isWalking() ? GetOffsetForWalking(player.AnimInfo, player._pdir) : Displacement {};
	x = WorldX(player.position.tile) + static_cast<float>(walk.deltaX);
	y = WorldY(player.position.tile) + static_cast<float>(walk.deltaY);
}

/**
 * A monster's place in world pixels, where DrawMonsterHelper puts his body (audit round 1: a slide from his tile toward the
 * next stood still on the southward steps, whose tile is already the new one): the tile he is drawn on - the one holding
 * his number, or for a step sideways to the west the one holding its negative - plus the walk's offset.
 */
void MonsterWorld(const Monster &monster, float &x, float &y)
{
	Point at = monster.position.tile;
	Displacement offset {};
	if (monster.isWalking()) {
		const bool sideLeft = monster.mode == MonsterMode::MoveSideways && monster.direction == Direction::West;
		const int want = sideLeft ? -(static_cast<int>(monster.getId()) + 1) : static_cast<int>(monster.getId()) + 1;
		for (const Point t : { monster.position.tile, monster.position.future, monster.position.old }) {
			if (InMap(t) && dMonster[t.x][t.y] == want) {
				at = t;
				break;
			}
		}
		offset = GetOffsetForWalking(monster.animInfo, monster.direction);
		if (sideLeft)
			offset -= Displacement { 64, 0 };
	}
	x = WorldX(at) + static_cast<float>(offset.deltaX);
	y = WorldY(at) + static_cast<float>(offset.deltaY);
}

bool MonsterShows(const Monster &monster)
{
	// Not while hidden (audit round 1: a ring under an unseen champion gave him away).
	return monster.hitPoints > 0 && monster.mode != MonsterMode::Death && (monster.flags & MFLAG_HIDDEN) == 0 && InMap(monster.position.tile)
	    && LitAt(monster.position.tile) > 0.05F
	    && (dFlags[monster.position.tile.x][monster.position.tile.y] & DungeonFlag::Lit) == DungeonFlag::Lit;
}

uint32_t ElementColour(DamageType type)
{
	switch (type) {
	case DamageType::Fire:
		return SG_PACK(255, 140, 50);
	case DamageType::Lightning:
		return SG_PACK(150, 190, 255);
	case DamageType::Magic:
		return SG_PACK(200, 150, 255);
	case DamageType::Acid:
		return SG_PACK(140, 230, 80);
	case DamageType::Cold:
		return SG_PACK(150, 215, 255);
	default:
		return SG_PACK(255, 230, 190);
	}
}

/** The colour a spell casts in: its first missile's trail colour, else its school's. */
uint32_t SpellColour(SpellID spell)
{
	if (static_cast<int>(spell) <= 0)
		return SG_PACK(220, 200, 255);
	const SpellData &data = GetSpellData(spell);
	const uint32_t trail = data.sMissiles[0] != MissileID::Null ? MissileTrailColour(static_cast<int>(data.sMissiles[0])) : 0;
	if (trail != 0)
		return trail;
	switch (data.type()) {
	case MagicType::Fire:
		return SG_PACK(255, 140, 50);
	case MagicType::Lightning:
		return SG_PACK(150, 190, 255);
	default:
		return SG_PACK(200, 160, 255);
	}
}

// ---- Particles ------------------------------------------------------------------------------------------------------------

enum class Kind : uint8_t {
	Spark, // additive, falls, a short streak
	Mote,  // additive, floats
	Dust,  // covering, spreads and settles
};

struct Particle {
	float x, y, z;    // world pixels; z above the floor
	float vx, vy, vz; // per second
	float born, life;
	uint32_t rgb;
	Kind kind;
};

constexpr size_t MaxParticles = 700;
std::vector<Particle> Particles;

void Emit(const Particle &p)
{
	if (Particles.size() < MaxParticles)
		Particles.push_back(p);
}

void EmitSparks(float x, float y, float z, int count, uint32_t rgb, float speed)
{
	const float now = Now();
	for (int i = 0; i < count; i++) {
		const float a = Rand() * TwoPi;
		const float s = speed * (0.4F + Rand() * 0.8F);
		Particle p {};
		p.x = x + (Rand() - 0.5F) * 6.0F;
		p.y = y + (Rand() - 0.5F) * 3.0F;
		p.z = z + (Rand() - 0.5F) * 8.0F;
		p.vx = std::cos(a) * s;
		p.vy = std::sin(a) * s * 0.5F;
		p.vz = 30.0F + Rand() * speed;
		p.born = now;
		p.life = 0.35F + Rand() * 0.35F;
		p.rgb = rgb;
		p.kind = Kind::Spark;
		Emit(p);
	}
}

void EmitMotes(float x, float y, float z, int count, uint32_t rgb, float rise)
{
	const float now = Now();
	for (int i = 0; i < count; i++) {
		Particle p {};
		p.x = x + (Rand() - 0.5F) * 20.0F;
		p.y = y + (Rand() - 0.5F) * 10.0F;
		p.z = z + Rand() * 10.0F;
		p.vx = (Rand() - 0.5F) * 10.0F;
		p.vy = (Rand() - 0.5F) * 5.0F;
		p.vz = rise * (0.6F + Rand() * 0.8F);
		p.born = now;
		p.life = 0.6F + Rand() * 0.5F;
		p.rgb = rgb;
		p.kind = Kind::Mote;
		Emit(p);
	}
}

void EmitDust(float x, float y, int count, float spread)
{
	const float now = Now();
	for (int i = 0; i < count; i++) {
		const float a = Rand() * TwoPi;
		const float s = spread * (0.5F + Rand() * 0.8F);
		Particle p {};
		p.x = x + (Rand() - 0.5F) * 16.0F;
		p.y = y + (Rand() - 0.5F) * 8.0F;
		p.z = 2.0F + Rand() * 10.0F;
		p.vx = std::cos(a) * s;
		p.vy = std::sin(a) * s * 0.5F;
		p.vz = 6.0F + Rand() * 12.0F;
		p.born = now;
		p.life = 0.6F + Rand() * 0.4F;
		p.rgb = SG_PACK(140, 128, 110);
		p.kind = Kind::Dust;
		Emit(p);
	}
}

void TickParticles(float dt)
{
	const float now = Now();
	for (Particle &p : Particles) {
		p.x += p.vx * dt;
		p.y += p.vy * dt;
		p.z += p.vz * dt;
		switch (p.kind) {
		case Kind::Spark:
			p.vz -= 360.0F * dt;
			if (p.z <= 0) {
				p.z = 0;
				p.vz = -p.vz * 0.3F; // a small bounce
				p.vx *= 0.5F;
				p.vy *= 0.5F;
			}
			break;
		case Kind::Dust: {
			const float drag = 1.0F - std::min(1.0F, dt * 3.0F);
			p.vx *= drag;
			p.vy *= drag;
			p.vz -= 8.0F * dt;
			break;
		}
		case Kind::Mote:
			break;
		}
	}
	Particles.erase(std::remove_if(Particles.begin(), Particles.end(), [now](const Particle &p) { return now - p.born > p.life || now < p.born; }), Particles.end());
}

// ---- Flashes: a short burst of light somewhere (a chest, a pickup) ------------------------------------------------------

struct Flash {
	float x, y, z;
	float born, life;
	uint32_t rgb;
	float strength;
};
std::vector<Flash> Flashes;

void PushFlash(float x, float y, float z, uint32_t rgb, float strength, float life)
{
	if (Flashes.size() >= 12)
		Flashes.erase(Flashes.begin());
	Flashes.push_back({ x, y, z, Now(), life, rgb, strength });
}

// ---- The hero: level-ups, potions, hurts --------------------------------------------------------------------------------

uint32_t HeroKey = 0;
int HeroLastLevel = -1;
int HeroLastHp = -1;
int HeroLastMana = -1;
int HeroLastMaxHp = -1;
int HeroLastMaxMana = -1;
uint32_t HeroLastLevelStamp = 0;
float LevelUpAt = -100;
float PotionAt = -100;
bool PotionLife = false, PotionMana = false;
float HurtAt = -100;
float HurtAmount = 0;

uint32_t KeyFor(const Player &player)
{
	uint32_t h = 2166136261U;
	for (const char *c = player._pName; *c != '\0'; c++)
		h = (h ^ static_cast<uint8_t>(*c)) * 16777619U;
	return h ^ (static_cast<uint32_t>(player._pClass) << 24);
}

void TickHero()
{
	if (MyPlayer == nullptr || !MyPlayer->plractive)
		return;
	const Player &player = *MyPlayer;
	const uint32_t key = KeyFor(player);
	const int level = player._pLevel;
	const int hp = player._pHitPoints;
	const int mana = player._pMana;
	// A new hero, or a new level of the dungeon: only note where things stand (a load is not a level-up, nor a hurt).
	// Gear or points changing the pools' size moves life and mana with them (CalcPlrItemVals): not a blow, not a gulp (audit
	// round 1: taking off a +life ring flashed him red).
	if (key != HeroKey || HeroLastLevel < 0 || HeroLastLevelStamp != LevelStamp() || player._pmode == PM_DEATH || player._pMaxHP != HeroLastMaxHp
	    || player._pMaxMana != HeroLastMaxMana) {
		const bool levelled = key == HeroKey && HeroLastLevel >= 0 && level > HeroLastLevel && level - HeroLastLevel <= 3
		    && HeroLastLevelStamp == LevelStamp() && player._pmode != PM_DEATH;
		if (levelled && *sgOptions.Oracool.levelUpBurst) {
			// A level-up raises the pools too: the burst still fires.
			LevelUpAt = Now();
			float x, y;
			HeroWorld(player, x, y);
			EmitMotes(x, y, 10.0F, 30, SG_PACK(255, 214, 120), 70.0F);
		}
		HeroLastMaxHp = player._pMaxHP;
		HeroLastMaxMana = player._pMaxMana;
		HeroKey = key;
		HeroLastLevel = level;
		HeroLastHp = hp;
		HeroLastMana = mana;
		HeroLastLevelStamp = LevelStamp();
		return;
	}
	const float now = Now();
	const bool levelled = level > HeroLastLevel && level - HeroLastLevel <= 3;
	if (levelled && *sgOptions.Oracool.levelUpBurst) {
		LevelUpAt = now;
		float x, y;
		HeroWorld(player, x, y);
		EmitMotes(x, y, 10.0F, 30, SG_PACK(255, 214, 120), 70.0F);
	}
	const int maxHp = std::max(player._pMaxHP, 64);
	const int maxMana = std::max(player._pMaxMana, 64);
	if (!levelled) {
		// A blow that hurt: a twentieth of his life or more at once, or at least two whole points.
		const int lost = HeroLastHp - hp;
		if (lost >= std::max(maxHp / 33, 128) && *sgOptions.Oracool.heroHurtFlash) {
			HurtAt = now;
			HurtAmount = Clamp01(0.35F + static_cast<float>(lost) / static_cast<float>(maxHp) * 2.5F);
		}
		// A gulp: an eighth of the pool or more in one go (a potion, a heal, a shrine, a fountain).
		const bool life = hp - HeroLastHp >= maxHp / 8 && hp > HeroLastHp;
		const bool manaUp = mana - HeroLastMana >= maxMana / 8 && mana > HeroLastMana && player._pMaxMana > 0;
		if ((life || manaUp) && *sgOptions.Oracool.potionEffects) {
			PotionAt = now;
			PotionLife = life;
			PotionMana = manaUp;
		}
	}
	HeroLastLevel = level;
	HeroLastHp = hp;
	HeroLastMana = mana;
}

// ---- Floor items: glints and pickups ------------------------------------------------------------------------------------

struct FloorItem {
	uint32_t seed = 0;
	Point position {};
	uint32_t rgb = 0;
	bool landed = false;
};
std::array<FloorItem, MAXITEMS + 1> FloorItems {};
std::array<bool, MAXITEMS + 1> SeenNow {};

uint32_t GlintColour(const Item &item)
{
	if (item._itype == ItemType::Gold)
		return SG_PACK(255, 220, 120);
	const LootLook look = LootLookFor(item);
	if (look.rgb != 0)
		return look.rgb;
	if (item._iMagical == ITEM_QUALITY_MAGIC)
		return SG_PACK(150, 170, 255);
	return 0;
}

void TickFloorItems()
{
	SeenNow.fill(false);
	for (uint8_t i = 0; i < ActiveItemCount; i++) {
		const int index = ActiveItems[i];
		if (index < 0 || index > MAXITEMS)
			continue;
		const Item &item = Items[index];
		SeenNow[static_cast<size_t>(index)] = true;
		FloorItem &f = FloorItems[static_cast<size_t>(index)];
		if (f.seed != item._iSeed) {
			f.seed = item._iSeed;
			f.landed = false;
		}
		f.position = item.position;
		f.rgb = GlintColour(item);
		f.landed = f.landed || !item._iAnimFlag;
	}
	// Gone from the floor beside the hero: picked up.
	const Player *hero = MyPlayer;
	for (size_t index = 0; index <= MAXITEMS; index++) {
		FloorItem &f = FloorItems[index];
		if (f.seed == 0 || SeenNow[index])
			continue;
		if (*sgOptions.Oracool.pickupGlints && f.landed && hero != nullptr && Chebyshev(f.position, hero->position.tile) <= 2 && InMap(f.position)
		    && LitAt(f.position) > 0.05F) {
			const uint32_t rgb = f.rgb != 0 ? f.rgb : SG_PACK(230, 220, 200);
			EmitMotes(WorldX(f.position), WorldY(f.position), 4.0F, 10, rgb, 55.0F);
			PushFlash(WorldX(f.position), WorldY(f.position), 10.0F, rgb, 0.45F, 0.3F);
		}
		f = FloorItem {};
	}
}

// ---- Objects: chests, doors ---------------------------------------------------------------------------------------------

struct ObjectState {
	_object_id type = OBJ_NULL;
	uint8_t sel = 0;
	uint32_t frame = 0;
	bool known = false;
};
std::array<ObjectState, MAXOBJECTS> ObjectStates {};

void TickObjects()
{
	const bool on = *sgOptions.Oracool.containerEffects;
	for (int i = 0; i < ActiveObjectCount; i++) {
		const int id = ActiveObjects[i];
		if (id < 0 || id >= MAXOBJECTS)
			continue;
		const Object &object = Objects[id];
		ObjectState &s = ObjectStates[static_cast<size_t>(id)];
		if (!s.known || s.type != object._otype) {
			s = { object._otype, object._oSelFlag, object._oAnimFrame, true };
			continue;
		}
		const bool lit = InMap(object.position) && LitAt(object.position) > 0.05F;
		if (on && lit && object.IsChest() && s.sel != 0 && object._oSelFlag == 0) {
			const float x = WorldX(object.position);
			const float y = WorldY(object.position);
			EmitSparks(x, y, 18.0F, 22, SG_PACK(255, 210, 110), 90.0F);
			EmitMotes(x, y, 12.0F, 8, SG_PACK(255, 230, 160), 40.0F);
			PushFlash(x, y, 16.0F, SG_PACK(255, 200, 110), 1.0F, 0.45F);
		}
		if (on && lit && object.isDoor() && object._oAnimFrame != s.frame)
			EmitDust(WorldX(object.position), WorldY(object.position), 12, 30.0F);
		s.sel = object._oSelFlag;
		s.frame = object._oAnimFrame;
	}
}

// ---- Monsters: drips, burning, the boss -----------------------------------------------------------------------------------

std::array<Point, MaxMonsters> MonsterLastTile {};
float BossNear = 0; // 0..1, eased

void TickMonsters(float dt)
{
	bool boss = false;
	const Player *hero = MyPlayer;
	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		const int id = ActiveMonsters[i];
		if (id < 0 || static_cast<size_t>(id) >= MaxMonsters)
			continue;
		const Monster &monster = Monsters[id];
		Point &last = MonsterLastTile[static_cast<size_t>(id)];
		const Point tile = monster.position.tile;
		if (*sgOptions.Oracool.bloodTrails && leveltype != DTYPE_TOWN && last != tile && last != Point {} && monster.hitPoints > 0
		    && monster.mode != MonsterMode::Death && monster.maxHitPoints > 0 && monster.hitPoints * 3 < monster.maxHitPoints && InMap(tile)
		    && Walkable(tile) && Rand() < 0.6F) {
			const bool bone = monster.data().monsterClass == MonsterClass::Undead;
			AddFloorMark(static_cast<float>(last.x) + (Rand() - 0.5F) * 0.4F, static_cast<float>(last.y) + (Rand() - 0.5F) * 0.4F,
			    bone ? FloorMarkKind::BoneDust : FloorMarkKind::Blood, 0.10F + Rand() * 0.05F);
		}
		last = tile;
		if (hero != nullptr && *sgOptions.Oracool.bossPresence && leveltype != DTYPE_TOWN && monster.hitPoints > 0 && monster.mode != MonsterMode::Death
		    && IsBossForGlow(monster) && Chebyshev(tile, hero->position.tile) <= 10)
			boss = true;
	}
	const float target = boss ? 1.0F : 0.0F;
	const float step = dt / 1.5F;
	BossNear = BossNear < target ? std::min(target, BossNear + step) : std::max(target, BossNear - step);
}

// ---- Missiles: scorch and frost -----------------------------------------------------------------------------------------

std::vector<uint8_t> Scorched; // a byte a tile: scorched by lingering fire this level

bool LingeringFire(MissileID id)
{
	// Not RingOfFire: its controller stands on the caster's tile; the walls it raises are FireWall (audit round 1).
	return IsAnyOf(id, MissileID::FireWall, MissileID::FlameWave, MissileID::Inferno);
}

struct ColdFlight {
	const Missile *missile;
	Point start;
	Point tile;
	Point next;
};
std::vector<ColdFlight> ColdFlights, ColdFlightsNext;

bool ByMissile(const ColdFlight &a, const ColdFlight &b)
{
	return std::less<const Missile *>()(a.missile, b.missile);
}

bool StruckSomething(Point tile, Point next)
{
	for (const Point t : { tile, next }) {
		if (!InMap(t))
			continue;
		if (dMonster[t.x][t.y] != 0 || TileHasAny(dPiece[t.x][t.y], TileProperties::Solid))
			return true;
	}
	return false;
}

void TickMissiles()
{
	const bool on = *sgOptions.Oracool.lingeringMarks && leveltype != DTYPE_TOWN;
	if (Scorched.size() != static_cast<size_t>(MAXDUNX) * MAXDUNY)
		Scorched.assign(static_cast<size_t>(MAXDUNX) * MAXDUNY, 0);
	ColdFlightsNext.clear();
	for (const Missile &m : Missiles) {
		const Point tile = m.position.tile;
		if (!InMap(tile))
			continue;
		if (on && LingeringFire(m._mitype) && Walkable(tile)) {
			uint8_t &done = Scorched[static_cast<size_t>(tile.x) * MAXDUNY + tile.y];
			if (done == 0) {
				done = 1;
				AddFloorMark(static_cast<float>(tile.x), static_cast<float>(tile.y), FloorMarkKind::Scorch, 0.30F + Rand() * 0.08F);
			}
		}
		const Displacement v = m.position.velocity;
		if (GetMissileData(m._mitype).damageType() == DamageType::Cold && v != Displacement {}) {
			const Point ahead = tile + Displacement { v.deltaX > 0 ? 1 : (v.deltaX < 0 ? -1 : 0), v.deltaY > 0 ? 1 : (v.deltaY < 0 ? -1 : 0) };
			ColdFlightsNext.push_back({ &m, m.position.start, tile, ahead });
		}
	}
	std::sort(ColdFlightsNext.begin(), ColdFlightsNext.end(), ByMissile);
	if (on) {
		for (const ColdFlight &f : ColdFlights) {
			const auto now = std::lower_bound(ColdFlightsNext.begin(), ColdFlightsNext.end(), f, ByMissile);
			const bool alive = now != ColdFlightsNext.end() && now->missile == f.missile && now->start == f.start;
			if (!alive && StruckSomething(f.tile, f.next) && Walkable(f.tile))
				AddFloorMark(static_cast<float>(f.tile.x), static_cast<float>(f.tile.y), FloorMarkKind::Frost, 0.30F);
		}
	}
	std::swap(ColdFlights, ColdFlightsNext);
}

// ---- Tristram: homes and the forge ---------------------------------------------------------------------------------------

struct Home {
	Point wall;  // the wall the window is in
	Point front; // the walkable tile before it
	bool forge;
};
std::vector<Home> Homes;
uint32_t HomesLevel = 0xFFFFFFFFU;
float NextForgeStrike = 0;
std::vector<HomeLamp> Lamps;

bool TownerStands(size_t t)
{
	const Towner &towner = Towners[t];
	const Point at = towner.position;
	return towner.anim && towner._ttype != TOWN_COW && at.x > 0 && at.y > 0 && at.x < MAXDUNX && at.y < MAXDUNY
	    && dMonster[at.x][at.y] == static_cast<int>(t) + 1;
}

void FindHomes()
{
	Homes.clear();
	if (leveltype != DTYPE_TOWN)
		return;
	for (size_t t = 0; t < NUM_TOWNERS; t++) {
		// Only those who keep a house or a shop (audit round 1: Cain, Farnham and Wirt stand by trees and fences).
		if (!TownerStands(t) || !IsAnyOf(Towners[t]._ttype, TOWN_SMITH, TOWN_HEALER, TOWN_TAVERN, TOWN_WITCH, TOWN_BMAID))
			continue;
		const Point at = Towners[t].position;
		// The nearest wall within four steps that has a walkable tile before it, toward the townsman.
		bool found = false;
		for (int r = 1; r <= 4 && !found; r++) {
			for (int dx = -r; dx <= r && !found; dx++) {
				for (int dy = -r; dy <= r && !found; dy++) {
					if (std::max(std::abs(dx), std::abs(dy)) != r)
						continue;
					const Point wall = at + Displacement { dx, dy };
					if (!InMap(wall) || !TileHasAny(dPiece[wall.x][wall.y], TileProperties::Solid))
						continue;
					const Point front = wall + Displacement { dx > 0 ? -1 : (dx < 0 ? 1 : 0), dy > 0 ? -1 : (dy < 0 ? 1 : 0) };
					if (!Walkable(front))
						continue;
					if (std::any_of(Homes.begin(), Homes.end(), [&](const Home &h) { return h.wall == wall; }))
						continue; // two folk at one door: one lamp
					Homes.push_back({ wall, front, Towners[t]._ttype == TOWN_SMITH });
					found = true;
				}
			}
		}
	}
}

void TickTown()
{
	if (leveltype != DTYPE_TOWN) {
		Homes.clear();
		HomesLevel = 0xFFFFFFFFU;
		return;
	}
	if (HomesLevel != LevelStamp() || Homes.empty()) {
		HomesLevel = LevelStamp();
		FindHomes();
	}
	const float now = Now();
	if (*sgOptions.Oracool.townForge && now >= NextForgeStrike) {
		NextForgeStrike = now + 1.4F + Rand() * 1.2F;
		for (size_t t = 0; t < NUM_TOWNERS; t++) {
			if (!TownerStands(t) || Towners[t]._ttype != TOWN_SMITH)
				continue;
			// The anvil before him: a strike, sparks flying.
			const Point at = Towners[t].position;
			EmitSparks(WorldX(at) + 10.0F, WorldY(at) + 4.0F, 26.0F, 14, SG_PACK(255, 170, 70), 80.0F);
		}
	}
}

// ---- Corpses ---------------------------------------------------------------------------------------------------------------

struct CorpseAge {
	int8_t id = 0;
	float born = 0;
};
std::vector<CorpseAge> CorpseAges;
float LevelEnteredAt = 0;
bool CorpsesSeeded = false;

/** Every body already lying on the level when it is entered is old (audit round 1: a body first seen later looked fresh). */
void SeedCorpses()
{
	CorpseAges.assign(static_cast<size_t>(MAXDUNX) * MAXDUNY, CorpseAge {});
	const float old = Now() - 120.0F;
	for (int x = 0; x < MAXDUNX; x++)
		for (int y = 0; y < MAXDUNY; y++)
			if (dCorpse[x][y] != 0)
				CorpseAges[static_cast<size_t>(x) * MAXDUNY + y] = { dCorpse[x][y], old };
	CorpsesSeeded = true;
}

/** Twice a second: a body laid out of sight since (a companion's kill) is dated now, not when it is first drawn (round 2). */
void DateNewCorpses()
{
	static float last = -100;
	const float now = Now();
	if (now - last < 0.5F && now >= last)
		return;
	last = now;
	if (CorpseAges.size() != static_cast<size_t>(MAXDUNX) * MAXDUNY)
		return;
	CorpseAge *ages = CorpseAges.data();
	for (int x = 0; x < MAXDUNX; x++) {
		for (int y = 0; y < MAXDUNY; y++) {
			const int8_t id = dCorpse[x][y];
			CorpseAge &age = ages[static_cast<size_t>(x) * MAXDUNY + y];
			if (age.id != id) {
				age.id = id;
				age.born = now;
			}
		}
	}
}
std::array<uint32_t, 256> CoolTable {};
bool CoolBuilt = false;
std::array<uint32_t, 256> HurtTable {};
bool HurtBuilt = false;

// ---- The orbs --------------------------------------------------------------------------------------------------------------

struct OrbState {
	int key = -1;
	int lastTop = -1;
	float energy = 0;
	float lastAt = -1;
};
std::array<OrbState, 8> Orbs {};

OrbState &OrbFor(int key)
{
	for (OrbState &o : Orbs)
		if (o.key == key)
			return o;
	for (OrbState &o : Orbs)
		if (o.key < 0) {
			o.key = key;
			return o;
		}
	Orbs[0] = OrbState {};
	Orbs[0].key = key;
	return Orbs[0];
}

float LastTick = -1;
uint32_t TickLevel = 0;

} // namespace

// ==== Skill light (the Skill Eye Candy page's verdicts, 2026-10-12) ========================================================================

namespace {

enum class SkillFxKind : uint8_t {
	Flash,
	Ring,
	Pillar,
	Heat,
	Beam,
	Lightning,
	Gather,
};

struct SkillFx {
	SkillFxKind kind;
	float x, y, x2, y2; // world pixels
	float born, life;
	uint32_t rgb;
	float k;
	int r, h;
	uint32_t seed;
	bool sparked;
};

std::vector<SkillFx> SkillFxs;
uint32_t SkillFxSeed = 1;
float PreviewNow = -1; // the preview's clock; -1 in the game

float FxNow()
{
	return PreviewNow >= 0 ? PreviewNow : Now();
}

bool SkillLightOn()
{
	return *sgOptions.Oracool.skillLight;
}

void PushSkillFx(SkillFxKind kind, Point tile, Point tile2, uint32_t rgb, float k, int r, int h, float life, float delay)
{
	if (!InMap(tile) && PreviewNow < 0)
		return;
	if (SkillFxs.size() >= 48)
		SkillFxs.erase(SkillFxs.begin());
	SkillFxs.push_back({ kind, WorldX(tile), WorldY(tile), WorldX(tile2), WorldY(tile2), FxNow() + delay, life, rgb, k, r, h, SkillFxSeed++ * 2654435761U, false });
}

/** One pixel of a bolt at strength @p k: a white-hot core, its colour one pixel round, a haze two out. */
void BoltBrush(const LightPen &pen, int x, int y, uint32_t rgb, float k)
{
	pen.addF(x, y, SG_PACK(255, 252, 240), k);
	pen.addF(x - 1, y, rgb, k * 0.6F);
	pen.addF(x + 1, y, rgb, k * 0.6F);
	pen.addF(x, y - 1, rgb, k * 0.45F);
	pen.addF(x, y + 1, rgb, k * 0.45F);
	pen.addF(x - 2, y, rgb, k * 0.22F);
	pen.addF(x + 2, y, rgb, k * 0.22F);
}

void BoltSegment(const LightPen &pen, float x0, float y0, float x1, float y1, uint32_t rgb, float k)
{
	const float dx = x1 - x0, dy = y1 - y0;
	const int steps = static_cast<int>(std::max(std::abs(dx), std::abs(dy))) + 1;
	for (int i = 0; i <= steps; i++) {
		const float t = static_cast<float>(i) / static_cast<float>(steps);
		BoltBrush(pen, static_cast<int>(x0 + dx * t), static_cast<int>(y0 + dy * t), rgb, k);
	}
}

/** A jagged path from (@p x0, @p y0) to (@p x1, @p y1) by midpoint displacement, @p levels deep, its jags from @p seed. */
void BoltPath(std::vector<float> &px, std::vector<float> &py, float x0, float y0, float x1, float y1, int levels, float rough, uint32_t seed)
{
	px.assign({ x0, x1 });
	py.assign({ y0, y1 });
	float spread = rough;
	for (int level = 0; level < levels; level++) {
		std::vector<float> nx, ny;
		for (size_t i = 0; i + 1 < px.size(); i++) {
			nx.push_back(px[i]);
			ny.push_back(py[i]);
			const float mx = (px[i] + px[i + 1]) * 0.5F, my = (py[i] + py[i + 1]) * 0.5F;
			const float sx = py[i + 1] - py[i], sy = -(px[i + 1] - px[i]); // across the segment
			const float len = std::max(1.0F, std::sqrt(sx * sx + sy * sy));
			const float off = (Unit(Hash(seed, static_cast<uint32_t>(level), static_cast<uint32_t>(i))) - 0.5F) * spread;
			nx.push_back(mx + sx / len * off);
			ny.push_back(my + sy / len * off);
		}
		nx.push_back(px.back());
		ny.push_back(py.back());
		px.swap(nx);
		py.swap(ny);
		spread *= 0.55F;
	}
}

void DrawLightning(const LightPen &pen, const SkillFx &f, float cx, float cy, float now)
{
	const float age = now - f.born;
	// Three strikes, each a flash that falls away fast; the shape is new each strike and quivers between.
	static constexpr float Strikes[] = { 0.0F, 0.13F, 0.29F };
	float bright = 0;
	int strike = 0;
	for (int i = 0; i < 3; i++) {
		const float a = age - Strikes[i];
		if (a < 0)
			continue;
		const float b = std::exp(-a / 0.07F) * (i == 0 ? 1.0F : 0.8F);
		if (b > bright) {
			bright = b;
			strike = i;
		}
	}
	const float k = f.k;
	// The floor's light, lingering a little past the last strike.
	const float floorK = std::max(bright, 0.35F * Clamp01(1.0F - age / f.life));
	Pool(pen, static_cast<int>(cx), static_cast<int>(cy), 110, 55, f.rgb, 0.55F * floorK * k);
	Pool(pen, static_cast<int>(cx), static_cast<int>(cy) - 6, 34, 18, SG_PACK(255, 250, 235), 0.6F * bright * k);
	if (bright < 0.06F)
		return;
	const uint32_t quiver = static_cast<uint32_t>(now * 30.0F);
	const uint32_t seed = f.seed + static_cast<uint32_t>(strike) * 7919U;
	static std::vector<float> px, py, bx, by;
	const float topX = cx + (Unit(Hash(seed, 1)) - 0.5F) * 70.0F;
	BoltPath(px, py, topX, cy - 300.0F, cx, cy - 4.0F, 6, 60.0F, seed + (quiver & 1));
	for (size_t i = 0; i + 1 < px.size(); i++)
		BoltSegment(pen, px[i], py[i], px[i + 1], py[i + 1], f.rgb, bright * k);
	// Branches off the trunk, reaching down and out.
	for (uint32_t b = 0; b < 3; b++) {
		const size_t at = 8 + static_cast<size_t>(Hash(seed, 50 + b) % std::max<size_t>(1, px.size() / 2));
		if (at >= px.size())
			continue;
		const float side = (Hash(seed, 60 + b) & 1) ? 1.0F : -1.0F;
		const float len = 50.0F + Unit(Hash(seed, 70 + b)) * 60.0F;
		BoltPath(bx, by, px[at], py[at], px[at] + side * len * 0.8F, py[at] + len * 0.6F, 4, 22.0F, seed + 100 + b);
		for (size_t i = 0; i + 1 < bx.size(); i++) {
			const float fade = 1.0F - static_cast<float>(i) / static_cast<float>(bx.size());
			BoltSegment(pen, bx[i], by[i], bx[i + 1], by[i + 1], f.rgb, bright * k * 0.55F * fade);
		}
	}
}

void DrawSkillFxLights(const LightPen &pen, float ox, float oy, float now)
{
	for (SkillFx &f : SkillFxs) {
		const float age = now - f.born;
		if (age < 0 || age > f.life || f.kind == SkillFxKind::Heat)
			continue;
		const float u = age / f.life;
		const float cx = f.x + ox, cy = f.y + oy;
		const int ix = static_cast<int>(cx), iy = static_cast<int>(cy);
		switch (f.kind) {
		case SkillFxKind::Flash: {
			const float env = (u < 0.15F ? u / 0.15F : std::pow(1.0F - (u - 0.15F) / 0.85F, 1.6F)) * f.k;
			Pool(pen, ix, iy, f.r, f.r / 2, f.rgb, 0.5F * env);
			Dot(pen, ix, iy - f.h, 5, SG_PACK(255, 250, 235), 0.9F * env);
			Pool(pen, ix, iy - f.h, f.r / 3, f.r / 3, f.rgb, 0.45F * env);
			break;
		}
		case SkillFxKind::Ring: {
			const float R = static_cast<float>(f.r) * (0.1F + 0.9F * std::sqrt(u));
			const float a = (1.0F - u) * 0.8F * f.k;
			for (int s = 0; s < 72; s++) {
				const float ang = static_cast<float>(s) / 72.0F * TwoPi;
				Dot(pen, ix + static_cast<int>(std::cos(ang) * R), iy + static_cast<int>(std::sin(ang) * R * 0.5F), 1, f.rgb, a);
			}
			break;
		}
		case SkillFxKind::Pillar: {
			const float env = u < 0.12F ? u / 0.12F : 1.0F - (u - 0.12F) / 0.88F;
			for (int y = 0; y < f.h; y++) {
				const float t = static_cast<float>(y) / static_cast<float>(f.h);
				const int half = 6 + static_cast<int>(4.0F * (1.0F - t));
				for (int dx = -half; dx <= half; dx++) {
					const float e = 1.0F - static_cast<float>(std::abs(dx)) / static_cast<float>(half + 1);
					pen.addF(ix + dx, iy - y, f.rgb, e * e * (1.0F - t) * env * 0.7F * f.k);
				}
			}
			Pool(pen, ix, iy, 60, 30, f.rgb, 0.45F * env * f.k);
			break;
		}
		case SkillFxKind::Beam: {
			const float env = (1.0F - u) * f.k;
			const float x2 = f.x2 + ox, y2 = f.y2 + oy;
			const float dx = x2 - cx, dy = y2 - cy;
			const int steps = static_cast<int>(std::max(std::abs(dx), std::abs(dy))) + 1;
			for (int i = 0; i <= steps; i++) {
				const float t = static_cast<float>(i) / static_cast<float>(steps);
				const int px = static_cast<int>(cx + dx * t), py = static_cast<int>(cy + dy * t) - f.h;
				const float k = env * (0.4F + 0.6F * t);
				pen.addF(px, py, SG_PACK(255, 248, 225), k * 0.8F);
				pen.addF(px, py - 1, f.rgb, k * 0.6F);
				pen.addF(px, py + 1, f.rgb, k * 0.6F);
				pen.addF(px, py - 2, f.rgb, k * 0.25F);
				pen.addF(px, py + 2, f.rgb, k * 0.25F);
			}
			break;
		}
		case SkillFxKind::Gather:
			Pool(pen, ix, iy, 30 + static_cast<int>(40.0F * u), 15 + static_cast<int>(20.0F * u), f.rgb, 0.5F * u * f.k);
			break;
		case SkillFxKind::Lightning:
			if (!f.sparked && PreviewNow < 0) {
				f.sparked = true;
				EmitSparks(f.x, f.y, 6.0F, 16, SG_PACK(255, 245, 220), 110.0F);
			}
			DrawLightning(pen, f, cx, cy, now);
			break;
		default:
			break;
		}
	}
}

/** The heat ripples: the picture bent in a ring running out (before the light). */
void DrawSkillFxHeat(const Surface &out, float ox, float oy, float now)
{
	static std::vector<uint32_t> Copy;
	for (const SkillFx &f : SkillFxs) {
		if (f.kind != SkillFxKind::Heat)
			continue;
		const float age = now - f.born;
		if (age < 0 || age > f.life)
			continue;
		const float u = age / f.life;
		const float R = static_cast<float>(f.r) * (0.15F + 0.85F * u);
		const float band = 16.0F;
		const float amp = 5.0F * f.k * (1.0F - u);
		const int cx = static_cast<int>(f.x + ox), cy = static_cast<int>(f.y + oy);
		const int x0 = std::max(0, static_cast<int>(cx - R - band)), x1 = std::min(out.w() - 1, static_cast<int>(cx + R + band));
		const int y0 = std::max(0, static_cast<int>(cy - (R + band) * 0.5F - 30)), y1 = std::min(out.h() - 1, static_cast<int>(cy + (R + band) * 0.5F));
		if (x1 <= x0 || y1 <= y0)
			continue;
		const int W = x1 - x0 + 1, H = y1 - y0 + 1;
		Copy.resize(static_cast<size_t>(W) * H);
		for (int y = 0; y < H; y++)
			std::memcpy(&Copy[static_cast<size_t>(y) * W], out.at<uint32_t>(x0, y0 + y), static_cast<size_t>(W) * 4);
		for (int y = 0; y < H; y++) {
			uint32_t *row = out.at<uint32_t>(x0, y0 + y);
			const float dy = static_cast<float>(y0 + y - cy) * 2.0F;
			for (int x = 0; x < W; x++) {
				const float dx = static_cast<float>(x0 + x - cx);
				const float d = std::sqrt(dx * dx + dy * dy) + 0.001F;
				const float off = d - R;
				if (off <= -band || off >= band)
					continue;
				const float w = std::cos(off / band * Pi * 0.5F);
				const float push = std::sin(off / band * Pi) * amp * w;
				int sx = x - static_cast<int>(dx / d * push);
				int sy = y - static_cast<int>(dy / d * push * 0.5F);
				sx = sx < 0 ? 0 : (sx >= W ? W - 1 : sx);
				sy = sy < 0 ? 0 : (sy >= H ? H - 1 : sy);
				row[x] = Copy[static_cast<size_t>(sy) * W + sx];
			}
		}
	}
}

/** The lasting light: Charge's trail, a brand's and a conversion's glow, the Blessed Hammers' light. */
void DrawSkillStates(const LightPen &pen, const View &v, float now)
{
	if (MyPlayer != nullptr && MyPlayer->plractive && IsFuriousChargeDashing()) {
		float wx, wy;
		HeroWorld(*MyPlayer, wx, wy);
		const Displacement step = Displacement(MyPlayer->_pdir);
		float dx = static_cast<float>(step.deltaX - step.deltaY) * 32.0F, dy = static_cast<float>(step.deltaX + step.deltaY) * 16.0F;
		const float len = std::max(1.0F, std::sqrt(dx * dx + dy * dy));
		dx /= len;
		dy /= len;
		const int cx = static_cast<int>(wx + v.ox), cy = static_cast<int>(wy + v.oy) - 30;
		for (int s = 0; s < 80; s++) {
			const float t = 1.0F - static_cast<float>(s) / 80.0F;
			const int px = cx - static_cast<int>(dx * static_cast<float>(s)), py = cy - static_cast<int>(dy * static_cast<float>(s));
			pen.addF(px, py, SG_PACK(255, 240, 200), t * 0.7F);
			pen.addF(px, py - 1, SG_PACK(244, 204, 96), t * 0.55F);
			pen.addF(px, py + 1, SG_PACK(244, 204, 96), t * 0.55F);
			pen.addF(px, py - 2, SG_PACK(244, 204, 96), t * 0.25F);
			pen.addF(px, py + 2, SG_PACK(244, 204, 96), t * 0.25F);
		}
		Pool(pen, cx, cy + 30, 50, 25, SG_PACK(244, 204, 96), 0.3F);
	}
	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		const int id = ActiveMonsters[i];
		if (id < 0 || static_cast<size_t>(id) >= MaxMonsters)
			continue;
		const Monster &monster = Monsters[id];
		if (!MonsterShows(monster))
			continue;
		const bool branded = (Rfa12SkillMarkers(monster) & (1U << static_cast<unsigned>(SkillMarker::OathBranded))) != 0;
		const bool converted = IsMonsterConverted(monster);
		if (!branded && !converted)
			continue;
		float wx, wy;
		MonsterWorld(monster, wx, wy);
		const int cx = static_cast<int>(wx + v.ox), cy = static_cast<int>(wy + v.oy);
		const float breathe = 0.8F + 0.2F * std::sin(now * 2.2F + static_cast<float>(id));
		if (branded)
			Pool(pen, cx, cy, 40, 20, SG_PACK(206, 182, 255), 0.4F * breathe);
		if (converted) {
			Pool(pen, cx, cy, 42, 21, SG_PACK(150, 230, 110), 0.35F * breathe);
			for (int k = 0; k < 6; k++) {
				const uint32_t s = Hash(static_cast<uint32_t>(id), static_cast<uint32_t>(k), 7101);
				const float f = Frac(now * (0.4F + Unit(s) * 0.3F) + Unit(s >> 8));
				pen.addF(cx + static_cast<int>((Unit(s >> 4) - 0.5F) * 34.0F), cy - 8 - static_cast<int>(f * 56.0F), SG_PACK(150, 230, 110), std::sin(f * Pi) * 0.7F);
			}
		}
	}
	for (const Missile &m : Missiles) {
		if (m._mitype != MissileID::BlessedHammer || !InMap(m.position.tile))
			continue;
		const int cx = static_cast<int>(WorldX(m.position.tile) + static_cast<float>(m.position.offset.deltaX) + v.ox);
		const int cy = static_cast<int>(WorldY(m.position.tile) + static_cast<float>(m.position.offset.deltaY) + v.oy);
		Pool(pen, cx, cy, 28, 14, SG_PACK(244, 204, 96), 0.38F);
		const float k = 0.45F * (0.75F + 0.25F * std::sin(now * 9.0F));
		for (int dx = -18; dx <= 18; dx++) {
			const float f = 1.0F - static_cast<float>(std::abs(dx)) / 19.0F;
			pen.addF(cx + dx, cy - 26, SG_PACK(255, 230, 160), f * f * k);
		}
	}
}

} // namespace

void SkillLightFlash(Point tile, uint32_t rgb, float strength, int radius, int height, float delay)
{
	if (SkillLightOn())
		PushSkillFx(SkillFxKind::Flash, tile, tile, rgb, strength, radius, height, 0.35F, delay);
}

void SkillLightRing(Point tile, uint32_t rgb, int radius, float delay)
{
	if (SkillLightOn())
		PushSkillFx(SkillFxKind::Ring, tile, tile, rgb, 1.0F, radius, 0, 0.45F, delay);
}

void SkillLightPillar(Point tile, uint32_t rgb, int height)
{
	if (SkillLightOn())
		PushSkillFx(SkillFxKind::Pillar, tile, tile, rgb, 1.0F, 16, height, 0.6F, 0.0F);
}

void SkillLightHeat(Point tile, int radius, float strength)
{
	if (SkillLightOn())
		PushSkillFx(SkillFxKind::Heat, tile, tile, 0, strength, radius, 0, 0.5F, 0.0F);
}

void SkillLightBeam(Point from, Point to, uint32_t rgb, float seconds)
{
	if (SkillLightOn())
		PushSkillFx(SkillFxKind::Beam, from, to, rgb, 1.0F, 0, 26, seconds, 0.0F);
}

void SkillLightSparks(Point tile, uint32_t rgb, int count)
{
	if (SkillLightOn() && InMap(tile))
		EmitSparks(WorldX(tile), WorldY(tile), 28.0F, count, rgb, 90.0F);
}

void SkillLightMotes(Point tile, uint32_t rgb, int count)
{
	if (SkillLightOn() && InMap(tile))
		EmitMotes(WorldX(tile), WorldY(tile), 10.0F, count, rgb, 45.0F);
}

void SkillLightning(Point tile, uint32_t rgb, float strength)
{
	if (DrawnLightningOn())
		PushSkillFx(SkillFxKind::Lightning, tile, tile, rgb, strength, 0, 0, 0.6F, 0.0F);
}

void SkillLightGather(Point tile, uint32_t rgb, float seconds)
{
	if (DrawnLightningOn())
		PushSkillFx(SkillFxKind::Gather, tile, tile, rgb, 1.0F, 0, 0, std::max(0.05F, seconds), 0.0F);
}

bool DrawnLightningOn()
{
	return *sgOptions.Oracool.drawnLightning;
}

int SkillLightCount()
{
	return static_cast<int>(SkillFxs.size());
}

void DrawSkillLightPreview(const Surface &out, Point tileScreen, Point tile, float seconds)
{
	PreviewNow = seconds;
	LightPen pen;
	pen.out = &out;
	pen.splats = nullptr;
	pen.w = out.w();
	pen.h = out.h();
	const float ox = static_cast<float>(tileScreen.x) - WorldX(tile);
	const float oy = static_cast<float>(tileScreen.y) - WorldY(tile);
	DrawSkillFxHeat(out, ox, oy, seconds);
	DrawSkillFxLights(pen, ox, oy, seconds);
	PreviewNow = -1;
}

// ==== Tests' questions ===========================================================================================================

bool IsBossForGlow(const Monster &monster)
{
	if (IsRiftGuardian(monster))
		return true;
	if (IsAnyOf(monster.type().type, MT_SKING, MT_CLEAVER, MT_DIABLO, MT_NAKRUL))
		return true;
	return IsAnyOf(monster.uniqueType, UniqueMonsterType::SkeletonKing, UniqueMonsterType::Butcher, UniqueMonsterType::Lazarus, UniqueMonsterType::NaKrul);
}

bool IsEliteForGlow(const Monster &monster)
{
	if (IsBossForGlow(monster))
		return false;
	return monster.lesserAffix != LesserUniqueAffix::None || monster.isUnique();
}

bool IsArmouredForGlow(const Monster &monster)
{
	const _monster_id t = monster.type().type;
	if ((t >= MT_WSKELAX && t <= MT_XSKELAX) || (t >= MT_WSKELBW && t <= MT_XSKELSD) || (t >= MT_NBLACK && t <= MT_RBLACK))
		return true;
	return IsAnyOf(t, MT_SKING, MT_GOLEM, MT_GARGOYLE, MT_BLOODCLW, MT_DEATHW);
}

float CorpseCooling(float age)
{
	// Fresh for five seconds, then a minute to cool all the way.
	return Clamp01((age - 5.0F) / 60.0F);
}

float SecondsSinceHeroHurt()
{
	return Now() - HurtAt;
}

float SecondsSinceLevelUp()
{
	return Now() - LevelUpAt;
}

int SceneGlowParticleCount()
{
	return static_cast<int>(Particles.size());
}

// ==== Once a frame ================================================================================================================

void ForgetSceneGlow()
{
	Particles.clear();
	Flashes.clear();
	FloorItems.fill(FloorItem {});
	ObjectStates.fill(ObjectState {});
	MonsterLastTile.fill(Point {});
	BossNear = 0;
	Scorched.assign(static_cast<size_t>(MAXDUNX) * MAXDUNY, 0);
	ColdFlights.clear();
	ColdFlightsNext.clear();
	Homes.clear();
	HomesLevel = 0xFFFFFFFFU;
	CorpseAges.assign(static_cast<size_t>(MAXDUNX) * MAXDUNY, CorpseAge {});
	LevelEnteredAt = Now();
	HurtAt = -100;
	PotionAt = -100;
	LevelUpAt = -100;
	HeroLastLevel = -1;
	Orbs.fill(OrbState {});
	SkillFxs.clear();
	NextForgeStrike = 0; // the scene clock wraps (audit round 1: the strikes stopped for ten hours)
	CorpsesSeeded = false;
}

void TickSceneGlow(const Surface &visible, Point firstTile, Point firstTileBuffer)
{
	(void)visible;
	(void)firstTile;
	(void)firstTileBuffer;
	if (TickLevel != LevelStamp()) {
		TickLevel = LevelStamp();
		ForgetSceneGlow();
	}
	BeginAuraGlowFrame(); // the rings drawn this frame note themselves for their glow (oracool/aura_ground.h)
	const float now = Now();
	float dt = LastTick < 0 || now < LastTick ? 0.0F : now - LastTick;
	dt = dt > 0.25F ? 0.25F : dt;
	LastTick = now;
	TickParticles(dt);
	Flashes.erase(std::remove_if(Flashes.begin(), Flashes.end(), [now](const Flash &f) { return now - f.born > f.life || now < f.born; }), Flashes.end());
	if (!CorpsesSeeded)
		SeedCorpses();
	else if (*sgOptions.Oracool.corpseSettle)
		DateNewCorpses();
	SkillFxs.erase(std::remove_if(SkillFxs.begin(), SkillFxs.end(), [now](const SkillFx &f) { return now - f.born > f.life + 0.05F; }), SkillFxs.end());
	TickHero();
	TickFloorItems();
	TickObjects();
	TickMonsters(dt);
	TickMissiles();
	TickTown();
}

// ==== Hits ==============================================================================================================================

void NoteMonsterHitGlow(const Monster &monster, int damage)
{
	if (damage <= 0 || leveltype == DTYPE_TOWN || !InMap(monster.position.tile) || LitAt(monster.position.tile) <= 0.05F)
		return;
	const Player *hero = MyPlayer;
	// A hard blow splashes the floor: a sixth of his life or more.
	if (*sgOptions.Oracool.bloodTrails && monster.maxHitPoints > 0 && damage * 6 >= monster.maxHitPoints) {
		const bool bone = monster.data().monsterClass == MonsterClass::Undead;
		float ax = 0, ay = 0;
		if (hero != nullptr) {
			// Thrown away from the hero.
			ax = static_cast<float>(monster.position.tile.x - hero->position.tile.x);
			ay = static_cast<float>(monster.position.tile.y - hero->position.tile.y);
			const float d = std::sqrt(ax * ax + ay * ay);
			if (d > 0) {
				ax /= d;
				ay /= d;
			}
		}
		AddFloorMark(static_cast<float>(monster.position.tile.x) + ax * 0.45F + (Rand() - 0.5F) * 0.3F,
		    static_cast<float>(monster.position.tile.y) + ay * 0.45F + (Rand() - 0.5F) * 0.3F, bone ? FloorMarkKind::BoneDust : FloorMarkKind::Blood,
		    0.16F + Rand() * 0.08F);
	}
	// A melee blow on armour, bone or stone: sparks. Melee: the hero mid-swing beside him.
	if (*sgOptions.Oracool.armourSparks && hero != nullptr && hero->_pmode == PM_ATTACK && Chebyshev(hero->position.tile, monster.position.tile) <= 1
	    && IsArmouredForGlow(monster)) {
		float x, y;
		MonsterWorld(monster, x, y);
		EmitSparks(x, y, 34.0F, 10, SG_PACK(255, 236, 170), 110.0F);
		PushFlash(x, y, 34.0F, SG_PACK(255, 240, 200), 0.5F, 0.12F);
	}
}

void DrawHeroHurtFlash(const Surface &out, const Player &player, Point spriteBottomLeft, ClxSprite sprite)
{
	if (!*sgOptions.Oracool.heroHurtFlash || out.isIndexed() || &player != MyPlayer)
		return;
	const float age = Now() - HurtAt;
	constexpr float Length = 0.18F;
	if (age < 0 || age >= Length)
		return;
	if (!HurtBuilt) {
		HurtTable.fill(SG_PACK(255, 40, 30));
		HurtBuilt = true;
	}
	ClxDrawRgbMapAlpha(out, spriteBottomLeft, sprite, HurtTable.data(), static_cast<int>((1.0F - age / Length) * 150.0F * (0.5F + 0.5F * HurtAmount)));
}

void DrawCorpseSettle(const Surface &out, Point tile, Point spriteBottomLeft, ClxSprite sprite, int lightTableIndex)
{
	if (!*sgOptions.Oracool.corpseSettle || out.isIndexed() || !InMap(tile))
		return;
	if (CorpseAges.size() != static_cast<size_t>(MAXDUNX) * MAXDUNY)
		CorpseAges.assign(static_cast<size_t>(MAXDUNX) * MAXDUNY, CorpseAge {});
	CorpseAge &age = CorpseAges[static_cast<size_t>(tile.x) * MAXDUNY + tile.y];
	const int8_t id = dCorpse[tile.x][tile.y];
	const float now = Now();
	if (age.id != id) {
		age.id = id; // a new body (the ones already here were seeded old)
		age.born = now;
	}
	const float k = CorpseCooling(now - age.born);
	if (k <= 0)
		return;
	if (!CoolBuilt) {
		CoolTable.fill(SG_PACK(58, 60, 66));
		CoolBuilt = true;
	}
	const float light = 1.0F - Clamp01(static_cast<float>(lightTableIndex) / static_cast<float>(LightsMax));
	const int alpha = static_cast<int>(k * 92.0F * (0.35F + 0.65F * light));
	if (alpha > 0)
		ClxDrawRgbMapAlpha(out, spriteBottomLeft, sprite, CoolTable.data(), alpha);
}

const std::vector<HomeLamp> &TownHomeLamps()
{
	Lamps.clear();
	// Only after dark, with the windows (audit round 1: at dusk the ground glowed before a window showed).
	if (leveltype != DTYPE_TOWN || !TownIsDark())
		return Lamps;
	for (const Home &home : Homes) {
		const float x = (static_cast<float>(home.wall.x) + static_cast<float>(home.front.x)) * 0.5F;
		const float y = (static_cast<float>(home.wall.y) + static_cast<float>(home.front.y)) * 0.5F;
		if (home.forge && *sgOptions.Oracool.townForge)
			Lamps.push_back({ x, y, 5.0F, 1.0F, SG_PACK(255, 130, 50) });
		else if (*sgOptions.Oracool.litHomes)
			Lamps.push_back({ x, y, 4.0F, 0.8F, SG_PACK(255, 190, 110) });
	}
	return Lamps;
}

// ==== The orbs ============================================================================================================================

float OrbSlosh(int key, int x, int revealTop)
{
	if (!*sgOptions.Oracool.orbSlosh)
		return 0;
	OrbState &o = OrbFor(key);
	const float now = Now();
	if (x < 0) {
		const float dt = o.lastAt < 0 || now < o.lastAt ? 0.0F : std::min(0.25F, now - o.lastAt);
		o.lastAt = now;
		o.energy *= std::exp(-dt * 1.8F);
		if (o.lastTop >= 0)
			o.energy = std::min(5.0F, o.energy + static_cast<float>(std::abs(revealTop - o.lastTop)) * 0.9F);
		o.lastTop = revealTop;
		return 0;
	}
	if (o.energy < 0.05F)
		return 0;
	return o.energy * std::sin(static_cast<float>(x) * 0.15F - now * 9.0F) * std::cos(now * 4.3F);
}

float OrbGlint(int key, int x, int width)
{
	if (!*sgOptions.Oracool.orbSlosh || width <= 0)
		return 0;
	const float now = Now();
	const float centre = (0.5F + 0.38F * std::sin(now * 0.33F + static_cast<float>(key % 97))) * static_cast<float>(width);
	const float d = (static_cast<float>(x) - centre) / 7.0F;
	return Clamp01(1.0F - d * d);
}

// ==== On the floor ============================================================================================================================

void DrawSceneGlowGround(const Surface &out, Point firstTile, Point firstTileBuffer)
{
	if (out.isIndexed() || out.w() <= 0 || out.h() <= 0 || leveltype == DTYPE_TOWN)
		return;
	const View v = MakeView(out, firstTile, firstTileBuffer);
	const float now = Now();
	const int w = out.w();
	const int h = out.h();
	const bool elites = *sgOptions.Oracool.eliteAuras;
	const bool bosses = *sgOptions.Oracool.bossPresence;
	if (!elites && !bosses)
		return;
	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		const int id = ActiveMonsters[i];
		if (id < 0 || static_cast<size_t>(id) >= MaxMonsters)
			continue;
		const Monster &monster = Monsters[id];
		if (!MonsterShows(monster))
			continue;
		float wx, wy;
		MonsterWorld(monster, wx, wy);
		const int cx = static_cast<int>(wx + v.ox);
		const int cy = static_cast<int>(wy + v.oy);
		if (cx < -140 || cy < -80 || cx > w + 140 || cy > h + 80)
			continue;
		const float lit = 0.35F + 0.65F * LitAt(monster.position.tile);
		if (bosses && IsBossForGlow(monster)) {
			// A darkness pooling round him, its edge stirring.
			// Each pixel's angle and distance are fixed for the pool's size, so they are tables built once, and the stirring
			// edge is a table of 256 angles a frame (audit round 1: atan2 and sin a pixel cost ~1.5 ms a boss in Debug).
			constexpr int Rx = 110, Ry = 55, Bw = 2 * Rx + 1, Bh = 2 * Ry + 1;
			static std::vector<uint8_t> Angle;
			static std::vector<uint16_t> Dist; // d2 x 65535, 65535 outside
			if (Angle.empty()) {
				Angle.resize(static_cast<size_t>(Bw) * Bh);
				Dist.resize(static_cast<size_t>(Bw) * Bh);
				for (int dy = -Ry; dy <= Ry; dy++) {
					for (int dx = -Rx; dx <= Rx; dx++) {
						const float fx = static_cast<float>(dx) / static_cast<float>(Rx);
						const float fy = static_cast<float>(dy) / static_cast<float>(Ry);
						const float d2 = fx * fx + fy * fy;
						const size_t i = static_cast<size_t>(dy + Ry) * Bw + static_cast<size_t>(dx + Rx);
						float a = std::atan2(fy, fx) / TwoPi;
						a = a < 0 ? a + 1.0F : a;
						Angle[i] = static_cast<uint8_t>(static_cast<int>(a * 256.0F) & 255);
						Dist[i] = static_cast<uint16_t>(d2 >= 1.0F ? 65535 : static_cast<int>(d2 * 65535.0F));
					}
				}
			}
			// Per angle: 1 / stir^2, in 1/4096ths.
			std::array<int, 256> inv {};
			for (int a = 0; a < 256; a++) {
				const float stir = 0.85F + 0.15F * std::sin(static_cast<float>(a) / 256.0F * TwoPi * 5.0F + now * 1.3F);
				inv[static_cast<size_t>(a)] = static_cast<int>(4096.0F / (stir * stir));
			}
			const uint8_t *angle = Angle.data();
			const uint16_t *dist = Dist.data();
			const int *invT = inv.data();
			for (int dy = -Ry; dy <= Ry; dy++) {
				const int py = cy + dy;
				if (py < 0 || py >= h)
					continue;
				uint32_t *row = out.at<uint32_t>(0, py);
				const size_t rowAt = static_cast<size_t>(dy + Ry) * Bw;
				const int x0 = cx - Rx < 0 ? -cx : -Rx;
				const int x1 = cx + Rx >= w ? w - 1 - cx : Rx;
				for (int dx = x0; dx <= x1; dx++) {
					const size_t i = rowAt + static_cast<size_t>(dx + Rx);
					const int d = dist[i];
					if (d == 65535)
						continue;
					// edge = 1 - d2 / stir^2, in 256ths.
					const int edge = 256 - static_cast<int>((static_cast<int64_t>(d) * invT[angle[i]]) >> 20);
					if (edge <= 0)
						continue;
					const int a = (edge * 120) >> 8;
					const uint32_t c = row[cx + dx];
					row[cx + dx] = SG_MIX(c, 12, 2, 6, a);
				}
			}
			continue;
		}
		if (elites && IsEliteForGlow(monster)) {
			static constexpr uint32_t Colours[] = { SG_PACK(255, 200, 80), SG_PACK(255, 70, 60), SG_PACK(90, 170, 255), SG_PACK(80, 230, 140),
				SG_PACK(200, 110, 255) };
			const uint32_t rgb = monster.lesserAffix != LesserUniqueAffix::None ? Colours[static_cast<size_t>(monster.lesserAffix) % 5] : Colours[0];
			// A turning ring of dashes on the floor, a fainter one inside it turning the other way.
			// The steps' angles are tables; each ring turns by one sine and cosine a frame (angle addition), not three trig
			// calls a step (audit round 1).
			struct RingTable {
				std::array<float, 72> c, s, dc, ds;
			};
			static RingTable Tables[2];
			static bool tablesBuilt = false;
			if (!tablesBuilt) {
				for (int ring = 0; ring < 2; ring++) {
					const int steps = ring == 0 ? 72 : 48;
					for (int k = 0; k < steps; k++) {
						const float a = static_cast<float>(k) / static_cast<float>(steps) * TwoPi;
						const float m = ring == 0 ? 6.0F : 4.0F;
						Tables[ring].c[static_cast<size_t>(k)] = std::cos(a);
						Tables[ring].s[static_cast<size_t>(k)] = std::sin(a);
						Tables[ring].dc[static_cast<size_t>(k)] = std::cos(a * m);
						Tables[ring].ds[static_cast<size_t>(k)] = std::sin(a * m);
					}
				}
				tablesBuilt = true;
			}
			for (int ring = 0; ring < 2; ring++) {
				const float rx = ring == 0 ? 28.0F : 19.0F;
				const float spin = (ring == 0 ? 0.6F : -0.9F) * now;
				const int steps = ring == 0 ? 72 : 48;
				const float cs = std::cos(spin), sn = std::sin(spin);
				const float m = ring == 0 ? 6.0F : 4.0F; // the dashes turn with their ring (audit round 2)
				const float dcs = std::cos(spin * m), dsn = std::sin(spin * m);
				const RingTable &tab = Tables[ring];
				for (int k = 0; k < steps; k++) {
					const size_t i = static_cast<size_t>(k);
					const float dash = tab.ds[i] * dcs + tab.dc[i] * dsn; // sin(m (a + spin))
					if (dash < 0.1F)
						continue;
					const float ca = tab.c[i] * cs - tab.s[i] * sn; // cos(a + spin)
					const float sa = tab.s[i] * cs + tab.c[i] * sn; // sin(a + spin)
					const int px = cx + static_cast<int>(ca * rx);
					const int py = cy + static_cast<int>(sa * rx * 0.5F);
					const int al = static_cast<int>(dash * (ring == 0 ? 150.0F : 90.0F) * lit);
					Blend(out, px, py, rgb, al);
					Blend(out, px + 1, py, rgb, al / 2);
				}
			}
		}
	}
}

// ==== Dust =================================================================================================================================

void DrawSceneGlowBodies(const Surface &out, Point firstTile, Point firstTileBuffer)
{
	if (out.isIndexed() || out.w() <= 0 || out.h() <= 0)
		return;
	const View v = MakeView(out, firstTile, firstTileBuffer);
	const float now = Now();
	DrawSkillFxHeat(out, v.ox, v.oy, now); // the skill light's heat ripples (before the light)
	const int w = out.w();
	const int h = out.h();
	for (const Particle &p : Particles) {
		if (p.kind != Kind::Dust)
			continue;
		const float age = Clamp01((now - p.born) / p.life);
		const int px = static_cast<int>(p.x + v.ox);
		const int py = static_cast<int>(p.y + v.oy - p.z);
		if (px < -6 || py < -6 || px > w + 6 || py > h + 6)
			continue;
		const int a = static_cast<int>((1.0F - age) * (age < 0.15F ? age / 0.15F : 1.0F) * 110.0F);
		const int r = 1 + static_cast<int>(age * 4.0F);
		for (int dy = -r; dy <= r; dy++)
			for (int dx = -r; dx <= r; dx++)
				if (dx * dx + dy * dy <= r * r)
					Blend(out, px + dx, py + dy, p.rgb, a * (r * r + 1 - dx * dx - dy * dy) / (r * r + 1));
	}
}

// ==== Light ==================================================================================================================================

void DrawSceneGlowLights(const Surface &out, Point firstTile, Point firstTileBuffer)
{
	if (out.isIndexed() || out.w() <= 0 || out.h() <= 0)
		return;
	const View v = MakeView(out, firstTile, firstTileBuffer);
	const LightPen pen = MakePen(out);
	const float now = Now();
	const bool town = leveltype == DTYPE_TOWN;

	// ---- The aura rings' glow (oracool/aura_ground.h) ----------------------------------------------------------------------
	DrawAuraGlow(out);

	// ---- The heroes: casting, charging, levelling, drinking --------------------------------------------------------------
	for (const Player &player : Players) {
		if (!player.plractive || !player.isOnActiveLevel() || !InMap(player.position.tile))
			continue;
		float wx, wy;
		HeroWorld(player, wx, wy);
		const int cx = static_cast<int>(wx + v.ox);
		const int cy = static_cast<int>(wy + v.oy);
		if (!OnScreen(v, static_cast<float>(cx), static_cast<float>(cy), 140))
			continue;
		const float lit = 0.4F + 0.6F * LitAt(player.position.tile);
		// Whirlwind's and Strafe's arcs (oracool/whirl_arcs.h): the near half of the ring and the floor's light, as light.
		if (*sgOptions.Oracool.whirlwindArcs && oracool::IsSpinning(player))
			DrawWhirlArcs(out, { cx, cy }, now, /*front=*/true, /*floor=*/true);
		if (player._pmode == PM_SPELL && player.AnimInfo.numberOfFrames > 0) {
			const uint32_t rgb = SpellColour(player.executedSpell.spellId);
			const int frame = player.AnimInfo.currentFrame;
			const int release = std::max<int>(player._pSFNum, 1);
			const float rise = Clamp01(static_cast<float>(frame + 1) / static_cast<float>(release + 1));
			if (*sgOptions.Oracool.castingGlow)
				Pool(pen, cx, cy, 70, 35, rgb, 0.30F * rise * lit);
			if (*sgOptions.Oracool.spellCharge) {
				const int hx = cx + (player._pdir == Direction::West || player._pdir == Direction::SouthWest || player._pdir == Direction::NorthWest ? -6 : 6);
				const int hy = cy - 46;
				if (frame < release) {
					// Motes spiralling in to the hands.
					for (int i = 0; i < 12; i++) {
						const float f = Frac(now * 1.9F + static_cast<float>(i) / 12.0F);
						const float r = 30.0F * (1.0F - f);
						const float a = static_cast<float>(i) * 2.4F + now * 5.0F + f * 3.0F;
						pen.addF(hx + static_cast<int>(std::cos(a) * r), hy + static_cast<int>(std::sin(a) * r * 0.7F), rgb, (0.3F + 0.7F * f) * 0.8F * rise);
					}
					Dot(pen, hx, hy, 3, rgb, 0.35F * rise);
				} else if (frame <= release + 1) {
					Dot(pen, hx, hy, 6, rgb, 0.9F);
					Pool(pen, hx, hy, 22, 22, rgb, 0.35F);
				}
			}
		}
		if (&player != MyPlayer)
			continue;
		// The level-up: a column of gold, a ring running out across the floor.
		const float lAge = now - LevelUpAt;
		if (*sgOptions.Oracool.levelUpBurst && lAge >= 0 && lAge < 1.8F) {
			const float env = lAge < 0.15F ? lAge / 0.15F : 1.0F - (lAge - 0.15F) / 1.65F;
			constexpr uint32_t Gold = SG_PACK(255, 214, 120);
			const int top = cy - 220;
			Splat *next = pen.splats != nullptr ? ScratchFor(static_cast<size_t>(221) * 33) : nullptr; // rows x widest row
			for (int py = std::max(top, 0); py <= std::min(cy, v.h - 1); py++) {
				const float t = static_cast<float>(cy - py) / 220.0F; // 0 at the feet .. 1 at the top
				const float fade = (1.0F - t) * (t < 0.08F ? t / 0.08F : 1.0F);
				const int half = 10 + static_cast<int>(6.0F * std::sin(t * 9.0F - now * 6.0F));
				const float invHalf = 1.0F / static_cast<float>(half + 1);
				const float rowK = fade * env * 0.55F * 256.0F;
				// Written inline, row by row, when nothing collects splats (audit round 1: three calls a pixel).
				uint32_t *row = pen.splats == nullptr ? out.at<uint32_t>(0, py) : nullptr;
				for (int dx = -half; dx <= half; dx++) {
					const int px = cx + dx;
					if (px < 0 || px >= v.w)
						continue;
					const float f = 1.0F - static_cast<float>(dx < 0 ? -dx : dx) * invHalf;
					const int k = static_cast<int>(f * f * rowK);
					const int ar = (SG_R(Gold) * k) >> 8, ag = (SG_G(Gold) * k) >> 8, ab = (SG_B(Gold) * k) >> 8;
					if ((ar | ag | ab) <= 0)
						continue;
					if (row != nullptr) {
						const uint32_t c = row[px];
						row[px] = SG_ADD(c, ar, ag, ab);
					} else {
						next->x = static_cast<float>(px);
						next->y = static_cast<float>(py);
						next->r = static_cast<float>(SG_SAT(ar)) * (1.0F / 255.0F);
						next->g = static_cast<float>(SG_SAT(ag)) * (1.0F / 255.0F);
						next->b = static_cast<float>(SG_SAT(ab)) * (1.0F / 255.0F);
						next->cover = 0.0F;
						next++;
					}
				}
			}
			if (pen.splats != nullptr)
				AppendScratch(pen.splats, next);
			const float ring = lAge * 140.0F;
			const int steps = 96;
			for (int k = 0; k < steps; k++) {
				const float a = static_cast<float>(k) / static_cast<float>(steps) * TwoPi;
				const int px = cx + static_cast<int>(std::cos(a) * ring);
				const int py = cy + static_cast<int>(std::sin(a) * ring * 0.5F);
				Dot(pen, px, py, 1, Gold, env * 0.7F * (1.0F - Clamp01(lAge / 1.2F)));
			}
			Pool(pen, cx, cy, 90, 45, Gold, 0.45F * env);
		}
		// A gulp: motes swirling up round him, red for life, blue for mana.
		const float pAge = now - PotionAt;
		if (*sgOptions.Oracool.potionEffects && pAge >= 0 && pAge < 1.1F) {
			const float env = 1.0F - pAge / 1.1F;
			for (int pass = 0; pass < 2; pass++) {
				if ((pass == 0 && !PotionLife) || (pass == 1 && !PotionMana))
					continue;
				const uint32_t rgb = pass == 0 ? SG_PACK(255, 70, 60) : SG_PACK(80, 130, 255);
				for (int i = 0; i < 16; i++) {
					const float a = static_cast<float>(i) / 16.0F * TwoPi + pAge * 7.0F + static_cast<float>(pass) * 0.2F;
					const float r = 20.0F * (1.0F - pAge * 0.4F);
					const float z = pAge * 70.0F + static_cast<float>(i % 4) * 8.0F;
					pen.addF(cx + static_cast<int>(std::cos(a) * r), cy - static_cast<int>(z) + static_cast<int>(std::sin(a) * r * 0.5F), rgb, env * 0.9F);
				}
				Pool(pen, cx, cy, 40, 20, rgb, 0.25F * env);
			}
		}
	}

	// ---- Nova's balls lighting the floor (user, 2026-10-12: "Do these circularly scattering spells light the floor under
	// them?" - they did not: dozens at once, too many for the engine's 64 lights, so a small pool each here instead) --------
	if (*sgOptions.Oracool.novaLight) {
		for (const Missile &m : Missiles) {
			if (!IsAnyOf(m._mitype, MissileID::NovaBall, MissileID::MiniNovaBall) || !InMap(m.position.tile))
				continue;
			const int cx = static_cast<int>(WorldX(m.position.tile) + static_cast<float>(m.position.offset.deltaX) + v.ox);
			const int cy = static_cast<int>(WorldY(m.position.tile) + static_cast<float>(m.position.offset.deltaY) + v.oy);
			if (!OnScreen(v, static_cast<float>(cx), static_cast<float>(cy), 30))
				continue;
			const uint32_t trail = MissileTrailColour(static_cast<int>(m._mitype));
			Pool(pen, cx, cy, 20, 10, trail != 0 ? trail : SG_PACK(150, 190, 255), m._mitype == MissileID::MiniNovaBall ? 0.26F : 0.32F);
		}
	}

	// ---- Skill light: the one-shot flashes, rings, pillars, beams, bolts; and the lasting glows ----------------------------
	DrawSkillFxLights(pen, v.ox, v.oy, now);
	if (SkillLightOn())
		DrawSkillStates(pen, v, now);

	// ---- Flashes (chests, pickups, sparks' strikes) ---------------------------------------------------------------------
	for (const Flash &f : Flashes) {
		const float age = now - f.born;
		if (age < 0 || age > f.life)
			continue;
		const float env = (age < 0.04F ? age / 0.04F : 1.0F - (age - 0.04F) / (f.life - 0.04F)) * f.strength;
		const int cx = static_cast<int>(f.x + v.ox);
		const int cy = static_cast<int>(f.y + v.oy);
		Pool(pen, cx, cy, 56, 28, f.rgb, 0.45F * env);
		Dot(pen, cx, cy - static_cast<int>(f.z), 5, f.rgb, 0.8F * env);
	}

	// ---- Particles: sparks and motes -----------------------------------------------------------------------------------------
	for (const Particle &p : Particles) {
		if (p.kind == Kind::Dust)
			continue;
		const float age = Clamp01((now - p.born) / p.life);
		const int px = static_cast<int>(p.x + v.ox);
		const int py = static_cast<int>(p.y + v.oy - p.z);
		const float k = (1.0F - age) * (p.kind == Kind::Spark ? 1.0F : 0.8F);
		pen.addF(px, py, p.rgb, k);
		if (p.kind == Kind::Spark) {
			// A short streak back along its flight.
			pen.addF(px - static_cast<int>(p.vx * 0.012F), py + static_cast<int>(p.vz * 0.012F), p.rgb, k * 0.5F);
			pen.addF(px - static_cast<int>(p.vx * 0.024F), py + static_cast<int>(p.vz * 0.024F), p.rgb, k * 0.25F);
		} else {
			pen.addF(px + 1, py, p.rgb, k * 0.35F);
			pen.addF(px, py + 1, p.rgb, k * 0.35F);
		}
	}

	// ---- Floor items: glints -----------------------------------------------------------------------------------------------------
	if (*sgOptions.Oracool.pickupGlints) {
		for (uint8_t i = 0; i < ActiveItemCount; i++) {
			const int index = ActiveItems[i];
			if (index < 0 || index > MAXITEMS)
				continue;
			const Item &item = Items[index];
			if (item._iAnimFlag || !InMap(item.position))
				continue;
			const uint32_t rgb = FloorItems[static_cast<size_t>(index)].rgb;
			const float lit = LitAt(item.position);
			if (rgb == 0 || lit <= 0.1F || (dFlags[item.position.x][item.position.y] & DungeonFlag::Lit) != DungeonFlag::Lit)
				continue;
			const uint32_t s = Hash(item._iSeed, static_cast<uint32_t>(index), 6001);
			const float period = 2.6F + Unit(s) * 2.6F;
			const float phase = Frac(now / period + Unit(s >> 8));
			constexpr float Glint = 0.07F;
			if (phase > Glint)
				continue;
			const float k = std::sin(phase / Glint * Pi) * lit;
			const int px = static_cast<int>(WorldX(item.position) + v.ox) + static_cast<int>((Unit(s >> 4) - 0.5F) * 14.0F);
			const int py = static_cast<int>(WorldY(item.position) + v.oy) - 6 - static_cast<int>(Unit(s >> 12) * 6.0F);
			Star(pen, px, py, 4, rgb, k);
		}
	}

	// ---- Objects: shrines and fountains ----------------------------------------------------------------------------------------------
	if (*sgOptions.Oracool.shrineGlow && !town) {
		for (int i = 0; i < ActiveObjectCount; i++) {
			const int id = ActiveObjects[i];
			if (id < 0 || id >= MAXOBJECTS)
				continue;
			const Object &object = Objects[id];
			uint32_t rgb = 0;
			if ((object.IsShrine() || IsAnyOf(object._otype, OBJ_GOATSHRINE, OBJ_CAULDRON)) && object._oSelFlag != 0)
				rgb = SG_PACK(170, 200, 255);
			else if (object._otype == OBJ_BLOODFTN)
				rgb = SG_PACK(255, 60, 50);
			else if (object._otype == OBJ_PURIFYINGFTN)
				rgb = SG_PACK(90, 160, 255);
			else if (object._otype == OBJ_MURKYFTN)
				rgb = SG_PACK(130, 200, 110);
			else if (object._otype == OBJ_TEARFTN)
				rgb = SG_PACK(200, 230, 255);
			if (rgb == 0 || !InMap(object.position))
				continue;
			const float lit = LitAt(object.position);
			if (lit <= 0.05F)
				continue;
			const int cx = static_cast<int>(WorldX(object.position) + v.ox);
			const int cy = static_cast<int>(WorldY(object.position) + v.oy);
			if (!OnScreen(v, static_cast<float>(cx), static_cast<float>(cy), 80))
				continue;
			const float breathe = 0.65F + 0.35F * std::sin(now * 1.7F + static_cast<float>(id));
			Pool(pen, cx, cy, 50, 25, rgb, 0.22F * breathe * (0.4F + 0.6F * lit));
			for (int k = 0; k < 6; k++) {
				const uint32_t s = Hash(static_cast<uint32_t>(id), static_cast<uint32_t>(k), 6101);
				const float f = Frac(now * (0.35F + Unit(s) * 0.25F) + Unit(s >> 8));
				const int px = cx + static_cast<int>((Unit(s >> 4) - 0.5F) * 30.0F + std::sin(now * 2.0F + static_cast<float>(k)) * 3.0F);
				const int py = cy - 10 - static_cast<int>(f * 50.0F);
				pen.addF(px, py, rgb, std::sin(f * Pi) * 0.7F * lit);
			}
		}
	}

	// ---- The exits: stairs and entrances breathing -------------------------------------------------------------------------------------
	if (*sgOptions.Oracool.exitGlow) {
		for (int i = 0; i < numtrigs && i < MAXTRIGGERS; i++) {
			const TriggerStruct &trig = trigs[i];
			const Point at { trig.position.x, trig.position.y };
			if (!InMap(at))
				continue;
			const bool down = IsAnyOf(trig._tmsg, WM_DIABNEXTLVL, WM_DIABTOWNWARP, WM_DIABSETLVL);
			const uint32_t rgb = down ? SG_PACK(120, 150, 255) : SG_PACK(255, 196, 120);
			const int cx = static_cast<int>(WorldX(at) + v.ox);
			const int cy = static_cast<int>(WorldY(at) + v.oy);
			if (!OnScreen(v, static_cast<float>(cx), static_cast<float>(cy), 80))
				continue;
			if (!town && (dFlags[at.x][at.y] & DungeonFlag::Explored) != DungeonFlag::Explored)
				continue;
			const float breathe = 0.6F + 0.4F * std::sin(now * 1.6F + static_cast<float>(i));
			Pool(pen, cx, cy, 46, 23, rgb, 0.24F * breathe);
			for (int k = 0; k < 4; k++) {
				const uint32_t s = Hash(static_cast<uint32_t>(i), static_cast<uint32_t>(k), 6201);
				const float f = Frac(now * (0.3F + Unit(s) * 0.2F) + Unit(s >> 8));
				pen.addF(cx + static_cast<int>((Unit(s >> 4) - 0.5F) * 34.0F), cy - static_cast<int>(f * 40.0F), rgb, std::sin(f * Pi) * 0.5F);
			}
		}
	}

	// ---- Monsters: burning and frozen ---------------------------------------------------------------------------------------------------
	if (*sgOptions.Oracool.statusLights && !town) {
		for (size_t i = 0; i < ActiveMonsterCount; i++) {
			const int id = ActiveMonsters[i];
			if (id < 0 || static_cast<size_t>(id) >= MaxMonsters)
				continue;
			const Monster &monster = Monsters[id];
			if (!MonsterShows(monster))
				continue;
			const bool burning = MonsterBurning(monster);
			const bool frozen = IsMonsterFrozen(monster);
			if (!burning && !frozen)
				continue;
			float wx, wy;
			MonsterWorld(monster, wx, wy);
			const int cx = static_cast<int>(wx + v.ox);
			const int cy = static_cast<int>(wy + v.oy);
			if (burning) {
				const float flick = 0.7F + 0.3F * std::sin(now * 17.0F + static_cast<float>(id)) * std::sin(now * 5.3F + static_cast<float>(id) * 2.0F);
				Pool(pen, cx, cy, 52, 26, SG_PACK(255, 130, 40), 0.32F * flick);
			}
			if (frozen)
				Pool(pen, cx, cy, 40, 20, SG_PACK(150, 210, 255), 0.22F);
		}
	}

	// ---- Tristram: the homes' windows after dark, the forge ---------------------------------------------------------------------------
	if (town) {
		const bool dark = *sgOptions.Oracool.dayNightCycle && TownIsDark();
		for (const Home &home : Homes) {
			const int wx = static_cast<int>(WorldX(home.wall) + v.ox);
			const int wy = static_cast<int>(WorldY(home.wall) + v.oy);
			if (!OnScreen(v, static_cast<float>(wx), static_cast<float>(wy), 120))
				continue;
			const uint32_t s = Hash(static_cast<uint32_t>(home.wall.x), static_cast<uint32_t>(home.wall.y), 6301);
			const float flick = 0.85F + 0.15F * std::sin(now * (5.0F + Unit(s) * 3.0F) + Unit(s >> 8) * 6.0F);
			if (home.forge && *sgOptions.Oracool.townForge) {
				// The forge's mouth: a deep orange glow on the ground and the wall, breathing with the bellows.
				const float bellows = 0.75F + 0.25F * std::sin(now * 1.4F);
				const int fx = static_cast<int>((WorldX(home.wall) + WorldX(home.front)) * 0.5F + v.ox);
				const int fy = static_cast<int>((WorldY(home.wall) + WorldY(home.front)) * 0.5F + v.oy);
				Pool(pen, fx, fy, 60, 30, SG_PACK(255, 120, 40), (dark ? 0.42F : 0.20F) * bellows * flick);
				Pool(pen, fx, fy - 30, 18, 14, SG_PACK(255, 160, 70), (dark ? 0.5F : 0.25F) * bellows * flick);
				continue;
			}
			if (!dark || !*sgOptions.Oracool.litHomes)
				continue;
			// A warm window high on the wall, its glow spilling round it.
			const int top = wy - 62;
			for (int dy = 0; dy < 12; dy++)
				for (int dx = -4; dx <= 4; dx++)
					pen.addF(wx + dx, top + dy, SG_PACK(255, 190, 110), (dy == 0 || std::abs(dx) == 4 ? 0.35F : 0.7F) * flick);
			Pool(pen, wx, top + 6, 20, 16, SG_PACK(255, 170, 90), 0.22F * flick);
		}
	}

	// ---- Lens glints: the brightest flames and fire spells throw a streak ---------------------------------------------------------------
	if (*sgOptions.Oracool.lensGlints && !town) {
		const float cxT = ((static_cast<float>(v.w) * 0.5F - v.ox) / 32.0F + (static_cast<float>(v.h) * 0.5F - v.oy) / 16.0F) * 0.5F;
		const float cyT = ((static_cast<float>(v.h) * 0.5F - v.oy) / 16.0F - (static_cast<float>(v.w) * 0.5F - v.ox) / 32.0F) * 0.5F;
		// The level's lights five times a second, not every frame (audit round 1: the list is gathered and sorted per call,
		// and two other passes ask for it too). A flame does not move; a fire spell's glint lags a fifth of a second at most.
		static std::array<GpuLight, 24> lights {};
		static size_t count = 0;
		static float gatheredAt = -100;
		static uint32_t gatheredLevel = 0;
		if (now - gatheredAt > 0.2F || now < gatheredAt || gatheredLevel != LevelStamp()) {
			gatheredAt = now;
			gatheredLevel = LevelStamp();
			count = GpuLightsNear({ static_cast<int>(cxT), static_cast<int>(cyT) }, lights.data(), lights.size());
		}
		int drawn = 0;
		for (size_t i = 0; i < count && drawn < 8; i++) {
			const GpuLight &l = lights[i];
			if (l.kind == 0 || l.intensity < 0.5F)
				continue;
			// Flames, and spells of fire only (audit round 1: lightning and cold streaked too).
			if (l.kind == 2 && !(l.r > l.g * 1.25F && l.r > l.b * 1.6F))
				continue;
			const Point tile { static_cast<int>(l.x + 0.5F), static_cast<int>(l.y + 0.5F) };
			if (!InMap(tile) || (dFlags[tile.x][tile.y] & DungeonFlag::Lit) != DungeonFlag::Lit)
				continue;
			const int sx = static_cast<int>(WorldX(l.x, l.y) + v.ox);
			const int sy = static_cast<int>(WorldY(l.x, l.y) + v.oy) - (l.kind == 1 ? 40 : 22);
			if (!OnScreen(v, static_cast<float>(sx), static_cast<float>(sy), 0))
				continue;
			drawn++;
			const uint32_t rgb = SG_PACK(SG_SAT(static_cast<int>(l.r * 200.0F) + 55), SG_SAT(static_cast<int>(l.g * 200.0F) + 55), SG_SAT(static_cast<int>(l.b * 200.0F) + 55));
			const float twinkle = 0.75F + 0.25F * std::sin(now * 3.1F + l.x * 1.7F + l.y);
			const float k = std::min(1.0F, l.intensity) * twinkle * 0.45F;
			const int len = 26 + static_cast<int>(18.0F * std::min(1.0F, l.intensity));
			for (int dx = -len; dx <= len; dx++) {
				const float f = 1.0F - static_cast<float>(std::abs(dx)) / static_cast<float>(len + 1);
				pen.addF(sx + dx, sy, rgb, f * f * k);
				if (std::abs(dx) < len / 2) {
					pen.addF(sx + dx, sy - 1, rgb, f * f * k * 0.3F);
					pen.addF(sx + dx, sy + 1, rgb, f * f * k * 0.3F);
				}
			}
		}
	}
}

// ==== Over the finished world ===============================================================================================================

void ApplySceneGlowAfterWorld(const Surface &world)
{
	if (world.isIndexed() || world.w() <= 0 || world.h() <= 0)
		return;
	const float now = Now();
	const float hurtAge = now - HurtAt;
	const float hurt = *sgOptions.Oracool.heroHurtFlash && hurtAge >= 0 && hurtAge < 0.45F ? (1.0F - hurtAge / 0.45F) * (1.0F - hurtAge / 0.45F) * HurtAmount : 0.0F;
	const float boss = *sgOptions.Oracool.bossPresence ? BossNear : 0.0F;
	if (hurt <= 0.01F && boss <= 0.01F)
		return;
	const int w = world.w();
	const int h = world.h();
	const int band = std::max(8, std::min(w, h) / 5);
	// The falloff by distance from the nearest edge: 0 inside the band's inner line, 256 at the edge - kept until the band
	// changes. Each row is three spans with no per-pixel calls (audit round 1: std::min and a checked index a pixel over the
	// whole screen cost ~6 ms a frame in Debug through a boss fight).
	static std::vector<int> Fall;
	if (static_cast<int>(Fall.size()) != band) {
		Fall.resize(static_cast<size_t>(band));
		for (int i = 0; i < band; i++) {
			const float f = 1.0F - static_cast<float>(i) / static_cast<float>(band);
			Fall[static_cast<size_t>(i)] = static_cast<int>(f * f * 256.0F);
		}
	}
	const int *fall = Fall.data();
	const int hurtA = static_cast<int>(hurt * 150.0F);
	const int bossA = static_cast<int>(boss * 90.0F);
	const int sideBand = band * 2 <= w ? band : w / 2;
	for (int y = 0; y < h; y++) {
		uint32_t *row = world.at<uint32_t>(0, y);
		const int ey = y < h - 1 - y ? y : h - 1 - y;
		const int fy = ey < band ? fall[ey] : 0;
		// The left band, the middle (only where the top or bottom band reaches), the right band.
		for (int x = 0; x < sideBand; x++) {
			const int fx = fall[x];
			const int f = fx > fy ? fx : fy;
			uint32_t c = row[x];
			if (bossA > 0)
				c = SG_MIX(c, 6, 0, 4, (bossA * f) >> 8);
			if (hurtA > 0)
				c = SG_MIX(c, 150, 0, 0, (hurtA * f) >> 8);
			row[x] = c;
		}
		if (fy > 0) {
			const int bossF = (bossA * fy) >> 8;
			const int hurtF = (hurtA * fy) >> 8;
			if (bossF > 0 || hurtF > 0) {
				for (int x = sideBand; x < w - sideBand; x++) {
					uint32_t c = row[x];
					if (bossF > 0)
						c = SG_MIX(c, 6, 0, 4, bossF);
					if (hurtF > 0)
						c = SG_MIX(c, 150, 0, 0, hurtF);
					row[x] = c;
				}
			}
		}
		for (int x = w - sideBand > sideBand ? w - sideBand : sideBand; x < w; x++) {
			const int fx = fall[w - 1 - x];
			const int f = fx > fy ? fx : fy;
			uint32_t c = row[x];
			if (bossA > 0)
				c = SG_MIX(c, 6, 0, 4, (bossA * f) >> 8);
			if (hurtA > 0)
				c = SG_MIX(c, 150, 0, 0, (hurtA * f) >> 8);
			row[x] = c;
		}
	}
}

} // namespace devilution::oracool
