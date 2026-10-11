/**
 * @file oracool/scene_feel.cpp
 *
 * The fifth graphics batch (oracool/scene_feel.h). Visual only: it reads the game, never writes it, and its random numbers
 * are its own. The pixel loops do their channel arithmetic in macros, as scene_fx.cpp and scene_life.cpp do (the Debug
 * build is compiled with /Ob0).
 */
#include "oracool/scene_feel.h"

#include <algorithm>
#include <functional>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include "diablo.h"
#include "interfac.h"
#include "engine/render/clx_render.hpp"
#include "engine/render/scrollrt.h"
#include "engine/surface.hpp"
#include "items.h"
#include "levels/gendung.h"
#include "lighting.h"
#include "missiles.h"
#include "monster.h"
#include "options.h"
#include "oracool/ground_fx.h"
#include "oracool/post_fx.h"
#include "oracool/rift.h"
#include "oracool/scene_fx.h"
#include "oracool/scene_life.h"
#include "oracool/splats.h"
#include "player.h"

#ifdef _MSC_VER
#pragma runtime_checks("", off)
#pragma optimize("gt", on)
#endif

#define SF_R(c) (static_cast<int>(((c) >> 16) & 0xFF))
#define SF_G(c) (static_cast<int>(((c) >> 8) & 0xFF))
#define SF_B(c) (static_cast<int>((c) & 0xFF))
#define SF_PACK(r, g, b) ((static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b))
#define SF_MIX(c, r, g, b, a) \
	SF_PACK(SF_R(c) + ((((r)-SF_R(c)) * (a)) >> 8), SF_G(c) + ((((g)-SF_G(c)) * (a)) >> 8), SF_B(c) + ((((b)-SF_B(c)) * (a)) >> 8))
#define SF_SAT(v) ((v) > 255 ? 255 : (v))
#define SF_ADD(c, r, g, b) SF_PACK(SF_SAT(SF_R(c) + (r)), SF_SAT(SF_G(c) + (g)), SF_SAT(SF_B(c) + (b)))

namespace devilution::oracool {

namespace {

constexpr float Pi = 3.14159265F;

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

uint32_t Rng = 0x6C8E9CF5U;

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

Point TileAtWorld(float wx, float wy)
{
	const float u = wx / 32.0F;
	const float v = wy / 16.0F;
	return { static_cast<int>(std::floor((u + v) * 0.5F + 0.5F)), static_cast<int>(std::floor((v - u) * 0.5F + 0.5F)) };
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

void ViewTiles(const View &v, int &x0, int &x1, int &y0, int &y1)
{
	float minX = 1e9F, maxX = -1e9F, minY = 1e9F, maxY = -1e9F;
	const float cx[4] = { -96.0F, static_cast<float>(v.w) + 96.0F, -96.0F, static_cast<float>(v.w) + 96.0F };
	const float cy[4] = { -64.0F, -64.0F, static_cast<float>(v.h) + 160.0F, static_cast<float>(v.h) + 160.0F };
	for (int i = 0; i < 4; i++) {
		const float u = (cx[i] - v.ox) / 32.0F;
		const float w = (cy[i] - v.oy) / 16.0F;
		minX = std::min(minX, (u + w) * 0.5F);
		maxX = std::max(maxX, (u + w) * 0.5F);
		minY = std::min(minY, (w - u) * 0.5F);
		maxY = std::max(maxY, (w - u) * 0.5F);
	}
	x0 = std::max(0, static_cast<int>(std::floor(minX)));
	x1 = std::min(MAXDUNX - 1, static_cast<int>(std::ceil(maxX)));
	y0 = std::max(0, static_cast<int>(std::floor(minY)));
	y1 = std::min(MAXDUNY - 1, static_cast<int>(std::ceil(maxY)));
}

struct LightPen {
	const Surface *out = nullptr;
	std::vector<Splat> *splats = nullptr;
	int w = 0, h = 0;
	void add(int px, int py, int r, int g, int b) const
	{
		if (px < 0 || py < 0 || px >= w || py >= h || (r | g | b) <= 0)
			return;
		r = SF_SAT(r);
		g = SF_SAT(g);
		b = SF_SAT(b);
		if (splats != nullptr) {
			splats->push_back({ static_cast<float>(px), static_cast<float>(py), static_cast<float>(r) / 255.0F, static_cast<float>(g) / 255.0F,
			    static_cast<float>(b) / 255.0F, 0.0F });
			return;
		}
		uint32_t &c = *out->at<uint32_t>(px, py);
		c = SF_ADD(c, r, g, b);
	}
	void addF(int px, int py, uint32_t rgb, float k) const
	{
		if (k <= 0.004F)
			return;
		add(px, py, static_cast<int>(static_cast<float>(SF_R(rgb)) * k), static_cast<int>(static_cast<float>(SF_G(rgb)) * k), static_cast<int>(static_cast<float>(SF_B(rgb)) * k));
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

void Blend(const Surface &out, int x, int y, uint32_t rgb, int a)
{
	if (x < 0 || y < 0 || x >= out.w() || y >= out.h() || a <= 0)
		return;
	a = a > 256 ? 256 : a;
	uint32_t &c = *out.at<uint32_t>(x, y);
	c = SF_MIX(c, SF_R(rgb), SF_G(rgb), SF_B(rgb), a);
}

// ---- The screen map, once a frame ---------------------------------------------------------------------------------------

View FrameView;
bool FrameViewValid = false;
float LastTick = -1;
uint32_t TickLevel = 0;

// ---- Particles: blood, bone, dust, sparkles -------------------------------------------------------------------------------

enum class Kind : uint8_t {
	Blood,
	Bone,
	Dust,
	Sparkle,
};

struct Particle {
	float x, y, z;    // world pixels; z above the floor
	float vx, vy, vz; // per second
	float born, life;
	uint32_t rgb;
	Kind kind;
};

constexpr size_t MaxParticles = 600;
std::vector<Particle> Particles;

void Emit(const Particle &p)
{
	if (Particles.size() < MaxParticles)
		Particles.push_back(p);
}

void EmitSpray(Point tile, bool bone, int count)
{
	const float now = Now();
	const float x = WorldX(tile);
	const float y = WorldY(tile);
	for (int i = 0; i < count; i++) {
		const float a = Rand() * 6.283F;
		const float speed = 40.0F + Rand() * 90.0F;
		Particle p {};
		p.x = x + (Rand() - 0.5F) * 10.0F;
		p.y = y + (Rand() - 0.5F) * 5.0F;
		p.z = 26.0F + Rand() * 22.0F;
		p.vx = std::cos(a) * speed;
		p.vy = std::sin(a) * speed * 0.5F;
		p.vz = 40.0F + Rand() * 110.0F;
		p.born = now;
		p.life = 0.9F + Rand() * 0.4F;
		p.kind = bone ? Kind::Bone : Kind::Blood;
		p.rgb = bone ? SF_PACK(212, 204, 180) : SF_PACK(150 + static_cast<int>(Rand() * 50.0F), 6, 10);
		Emit(p);
	}
}

void EmitDust(float x, float y, int count, float spread)
{
	const float now = Now();
	for (int i = 0; i < count; i++) {
		const float a = Rand() * 6.283F;
		const float speed = spread * (0.5F + Rand() * 0.8F);
		Particle p {};
		p.x = x;
		p.y = y;
		p.z = 2.0F + Rand() * 4.0F;
		p.vx = std::cos(a) * speed;
		p.vy = std::sin(a) * speed * 0.5F;
		p.vz = 8.0F + Rand() * 14.0F;
		p.born = now;
		p.life = 0.55F + Rand() * 0.35F;
		p.kind = Kind::Dust;
		p.rgb = SF_PACK(150, 138, 118);
		Emit(p);
	}
}

void TickParticles(float dt)
{
	const float now = Now();
	for (Particle &p : Particles) {
		p.x += p.vx * dt;
		p.y += p.vy * dt;
		if (p.kind == Kind::Dust) {
			p.vx *= 1.0F - std::min(1.0F, dt * 3.0F);
			p.vy *= 1.0F - std::min(1.0F, dt * 3.0F);
			p.z += p.vz * dt;
			p.vz -= 10.0F * dt;
		} else if (p.kind == Kind::Sparkle) {
			p.z += p.vz * dt;
		} else {
			p.z += p.vz * dt;
			p.vz -= 420.0F * dt;
			if (p.z <= 0) {
				// On the floor: it stays a moment, then is gone (the floor marks hold the blood that lasts).
				p.z = 0;
				p.vx = p.vy = p.vz = 0;
			}
		}
	}
	Particles.erase(std::remove_if(Particles.begin(), Particles.end(), [now](const Particle &p) { return now - p.born > p.life || now < p.born; }), Particles.end());
}

// ---- Hits and deaths ------------------------------------------------------------------------------------------------------

std::array<float, MaxMonsters> HitAt {};
std::array<uint8_t, MaxMonsters> DeathSeen {}; // 0 alive; 1 dying, noted; 2 landed, puffed
std::array<uint32_t, 256> WhiteTable {};
bool WhiteBuilt = false;

bool BleedsBone(const Monster &monster)
{
	return monster.data().monsterClass == MonsterClass::Undead;
}

void TickDeaths()
{
	if (leveltype == DTYPE_TOWN)
		return;
	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		const int id = ActiveMonsters[i];
		if (id < 0 || static_cast<size_t>(id) >= MaxMonsters)
			continue;
		const Monster &monster = Monsters[id];
		uint8_t &seen = DeathSeen[static_cast<size_t>(id)];
		if (monster.mode != MonsterMode::Death) {
			seen = 0;
			continue;
		}
		if (!*sgOptions.Oracool.deathEffects || !InMap(monster.position.tile) || LitAt(monster.position.tile) <= 0.05F)
			continue;
		if (DissolvesOnDeath(monster))
			continue; // a guardian burning away leaves no body to bleed or land (audit round 1)
		if (seen == 0) {
			seen = 1;
			EmitSpray(monster.position.tile, BleedsBone(monster), 16);
		}
		const int frames = monster.animInfo.numberOfFrames;
		if (seen == 1 && frames > 1 && monster.animInfo.currentFrame >= frames - 2) {
			seen = 2;
			EmitDust(WorldX(monster.position.tile), WorldY(monster.position.tile), 12, 34.0F);
		}
	}
}

// ---- Drops ------------------------------------------------------------------------------------------------------------------

struct DropState {
	uint32_t seed = 0;
	bool tumbling = false;
};
std::array<DropState, MAXITEMS + 1> Drops {};

float TumbleHeight(const Item &item)
{
	const int n = item.AnimInfo.numberOfFrames;
	if (n <= 1)
		return 0;
	const float p = Clamp01(static_cast<float>(item.AnimInfo.currentFrame) / static_cast<float>(n - 1));
	return std::sin(p * Pi);
}

void TickDrops()
{
	const float now = Now();
	for (uint8_t i = 0; i < ActiveItemCount; i++) {
		const int index = ActiveItems[i];
		if (index < 0 || index > MAXITEMS)
			continue;
		const Item &item = Items[index];
		DropState &d = Drops[static_cast<size_t>(index)];
		if (d.seed != item._iSeed) {
			d.seed = item._iSeed;
			d.tumbling = item._iAnimFlag;
			continue;
		}
		if (item._iAnimFlag) {
			d.tumbling = true;
			// A rare one trails sparkles from the top of its arc.
			if (*sgOptions.Oracool.dropEffects && InMap(item.position) && LitAt(item.position) > 0.05F) {
				const LootLook look = LootLookFor(item);
				if (look.rgb != 0 && Rand() < 0.6F) {
					Particle p {};
					p.x = WorldX(item.position) + (Rand() - 0.5F) * 8.0F;
					p.y = WorldY(item.position) + (Rand() - 0.5F) * 4.0F;
					p.z = 18.0F + TumbleHeight(item) * 34.0F + Rand() * 8.0F;
					p.vz = -12.0F - Rand() * 10.0F;
					p.born = now;
					p.life = 0.35F + Rand() * 0.25F;
					p.kind = Kind::Sparkle;
					p.rgb = look.rgb;
					Emit(p);
				}
			}
			continue;
		}
		if (!d.tumbling)
			continue;
		d.tumbling = false;
		if (*sgOptions.Oracool.dropEffects && InMap(item.position) && LitAt(item.position) > 0.05F)
			EmitDust(WorldX(item.position), WorldY(item.position), 6, 22.0F);
	}
}

// ---- Impacts ------------------------------------------------------------------------------------------------------------------

struct Flying {
	const Missile *missile;
	MissileID type;
	Point start;
	Point tile;
	Point next; // the tile it was flying into
	float x, y; // last seen, world pixels
	uint32_t rgb;
	bool moving;
};
std::vector<Flying> Flights, FlightsNext;

struct Burst {
	float x, y;
	float born;
	uint32_t rgb;
	float strength;
};
std::vector<Burst> Bursts;
constexpr float BurstLife = 0.38F;

bool ByMissile(const Flying &a, const Flying &b)
{
	return std::less<const Missile *>()(a.missile, b.missile); // unrelated pointers ordered portably (audit round 1)
}

bool StruckSomething(Point tile, Point next)
{
	// Its own tile or the next one along its flight (audit round 2: a ring of eight held a wall in every corridor, and
	// the hero beside it): a monster there, or a wall it flew into. A player counts only when it is not ours.
	for (const Point t : { tile, next }) {
		if (!InMap(t))
			continue;
		if (dMonster[t.x][t.y] != 0 || TileHasAny(dPiece[t.x][t.y], TileProperties::Solid))
			return true;
		const int pid = dPlayer[t.x][t.y];
		if (pid != 0 && MyPlayer != nullptr && &Players[std::abs(pid) - 1] != MyPlayer)
			return true;
	}
	return false;
}

void PushBurst(float x, float y, uint32_t rgb, float strength)
{
	if (Bursts.size() >= 16)
		Bursts.erase(Bursts.begin());
	Bursts.push_back({ x, y, Now(), rgb, strength });
}

void TickImpacts()
{
	const float now = Now();
	Bursts.erase(std::remove_if(Bursts.begin(), Bursts.end(), [now](const Burst &b) { return now - b.born > BurstLife || now < b.born; }), Bursts.end());
	FlightsNext.clear();
	const bool on = *sgOptions.Oracool.impactLights;
	for (const Missile &m : Missiles) {
		const uint32_t rgb = MissileTrailColour(static_cast<int>(m._mitype));
		if (rgb == 0)
			continue;
		const Displacement v = m.position.velocity;
		const Point ahead = m.position.tile + Displacement { v.deltaX > 0 ? 1 : (v.deltaX < 0 ? -1 : 0), v.deltaY > 0 ? 1 : (v.deltaY < 0 ? -1 : 0) };
		Flying f { &m, m._mitype, m.position.start, m.position.tile, ahead, WorldX(m.position.tile) + static_cast<float>(m.position.offset.deltaX),
			WorldY(m.position.tile) + static_cast<float>(m.position.offset.deltaY), rgb, m.position.velocity != Displacement {} };
		// Stopped this frame after flying: it struck something (a fireball turns into its burst where it stands).
		const auto was = std::lower_bound(Flights.begin(), Flights.end(), f, ByMissile);
		if (on && was != Flights.end() && was->missile == &m && was->type == m._mitype && was->start == m.position.start && was->moving && !f.moving
		    && LitAt(m.position.tile) > 0.02F)
			PushBurst(f.x, f.y, rgb, GetMissileData(m._mitype).isArrow() ? 0.45F : 1.0F);
		FlightsNext.push_back(f);
	}
	std::sort(FlightsNext.begin(), FlightsNext.end(), ByMissile);
	// Gone while still flying: it struck home and was removed in the same tick.
	if (on) {
		for (const Flying &f : Flights) {
			if (!f.moving)
				continue;
			const auto now2 = std::lower_bound(FlightsNext.begin(), FlightsNext.end(), f, ByMissile);
			const bool alive = now2 != FlightsNext.end() && now2->missile == f.missile && now2->type == f.type && now2->start == f.start;
			// A hit, not its range running out (audit round 1: Charged Bolt flashed in empty air): someone, or a wall, beside it.
			if (!alive && StruckSomething(f.tile, f.next) && LitAt(f.tile) > 0.02F)
				PushBurst(f.x, f.y, f.rgb, f.type == MissileID::Arrow ? 0.4F : 0.8F);
		}
	}
	std::swap(Flights, FlightsNext);
}

// ---- The camera ---------------------------------------------------------------------------------------------------------------

float HeroLastX = 0, HeroLastY = 0;
Point HeroLastTile;
bool HeroLastValid = false;
int LeanW = 0, LeanH = 0; // the world surface the lean-in last worked on: the pointer's inverse needs its middle
float GlideX = 0, GlideY = 0, GlideAt = -100;
constexpr float GlideLength = 0.24F;
float Lean = 0; // the guardian's lean-in, 0..1

void TickCamera(float dt)
{
	if (MyPlayer == nullptr)
		return;
	const Player &player = *MyPlayer;
	const Displacement walk = player.isWalking() ? GetOffsetForWalking(player.AnimInfo, player._pdir) : Displacement {};
	const float x = WorldX(player.position.tile) + static_cast<float>(walk.deltaX);
	const float y = WorldY(player.position.tile) + static_cast<float>(walk.deltaY);
	if (HeroLastValid && *sgOptions.Oracool.smoothCamera && !player.isWalking()) {
		// A jump the walk never makes - three tiles or more while not walking (audit round 1: a frame hitch mid-walk
		// passed for a Teleport): a Teleport, a Leap, a phase. The picture eases from where it was, in screen pixels
		// (the zoom enlarges the jump), added to what is left of a glide under way, never more than 80 px.
		const Point t = player.position.tile;
		if (std::max(std::abs(t.x - HeroLastTile.x), std::abs(t.y - HeroLastTile.y)) >= 3) {
			const float zoom = std::max(1.0F, static_cast<float>(*sgOptions.Oracool.dungeonZoomLevel));
			const Displacement left = CameraGlideOffset();
			float gx = (x - HeroLastX) * zoom + static_cast<float>(left.deltaX);
			float gy = (y - HeroLastY) * zoom + static_cast<float>(left.deltaY);
			const float d = std::sqrt(gx * gx + gy * gy);
			if (d > 80.0F) {
				gx *= 80.0F / d;
				gy *= 80.0F / d;
			}
			GlideX = gx;
			GlideY = gy;
			GlideAt = Now();
		}
	}
	HeroLastX = x;
	HeroLastY = y;
	HeroLastTile = player.position.tile;
	HeroLastValid = true;
	// Lean in while a living rift guardian is near.
	bool near = false;
	if (*sgOptions.Oracool.smoothCamera && leveltype != DTYPE_TOWN) {
		for (size_t i = 0; i < ActiveMonsterCount && !near; i++) {
			const Monster &monster = Monsters[ActiveMonsters[i]];
			if (monster.mode == MonsterMode::Death || monster.hitPoints <= 0 || !IsRiftGuardian(monster))
				continue;
			const Point a = monster.position.tile;
			const Point b = player.position.tile;
			near = std::max(std::abs(a.x - b.x), std::abs(a.y - b.y)) <= 9;
		}
	}
	const float target = near ? 1.0F : 0.0F;
	const float step = dt / 1.2F;
	Lean = Lean < target ? std::min(target, Lean + step) : std::max(target, Lean - step);
}

// ---- Atmosphere ------------------------------------------------------------------------------------------------------------------

bool AtmosphereHere()
{
	return *sgOptions.Oracool.areaAtmosphere && leveltype != DTYPE_TOWN;
}

} // namespace

// ==== Once a frame ================================================================================================================

void TickSceneFeel(const Surface &visible, Point firstTile, Point firstTileBuffer)
{
	FrameViewValid = !visible.isIndexed();
	FrameView = MakeView(visible, firstTile, firstTileBuffer);
	if (TickLevel != LevelStamp()) {
		TickLevel = LevelStamp();
		ForgetSceneFeel();
	}
	const float now = Now();
	float dt = LastTick < 0 || now < LastTick ? 0.0F : now - LastTick;
	dt = dt > 0.25F ? 0.25F : dt;
	LastTick = now;
	TickParticles(dt);
	TickDeaths();
	TickDrops();
	TickImpacts();
	TickCamera(dt);
}

// ==== Hits ============================================================================================================================

void NoteMonsterHit(const Monster &monster, int damage)
{
	const size_t id = monster.getId();
	if (id >= MaxMonsters || damage <= 0)
		return;
	HitAt[id] = Now() == 0 ? 0.0001F : Now();
	// A hard blow sprays blood (bone chips from the undead): a fifth of his life or more in one hit.
	if (*sgOptions.Oracool.deathEffects && leveltype != DTYPE_TOWN && monster.maxHitPoints > 0 && damage * 5 >= monster.maxHitPoints
	    && InMap(monster.position.tile) && LitAt(monster.position.tile) > 0.05F)
		EmitSpray(monster.position.tile, BleedsBone(monster), 8);
}

float HitFlashStrength(float age)
{
	constexpr float Length = 0.12F;
	if (age < 0 || age >= Length)
		return 0;
	return 1.0F - age / Length;
}

void DrawMonsterHitFlash(const Surface &out, const Monster &monster, Point spriteBottomLeft, ClxSprite sprite)
{
	if (!*sgOptions.Oracool.hitFlash || out.isIndexed() || monster.mode == MonsterMode::Death)
		return;
	const size_t id = monster.getId();
	if (id >= MaxMonsters || HitAt[id] == 0)
		return;
	const float k = HitFlashStrength(Now() - HitAt[id]);
	if (k <= 0)
		return;
	if (!WhiteBuilt) {
		WhiteTable.fill(SF_PACK(255, 250, 240));
		WhiteBuilt = true;
	}
	ClxDrawRgbMapAlpha(out, spriteBottomLeft, sprite, WhiteTable.data(), static_cast<int>(k * 170.0F));
}

// ==== Before the light ==================================================================================================================

void DrawSceneFeelBodies(const Surface &out, Point firstTile, Point firstTileBuffer)
{
	if (out.isIndexed() || out.w() <= 0 || out.h() <= 0)
		return;
	const View v = MakeView(out, firstTile, firstTileBuffer);
	const float now = Now();
	const int w = out.w();
	const int h = out.h();

	// Lava light: a warm, flickering glow on whatever stands near the lava (the Caves, Hell, the Crypt).
	const uint8_t *liquid = (*sgOptions.Oracool.lavaLight && leveltype != DTYPE_TOWN) ? LiquidMapForGpu() : nullptr;
	if (liquid != nullptr) {
		int x0, x1, y0, y1;
		ViewTiles(v, x0, x1, y0, y1);
		const int tw = x1 - x0 + 1;
		const int th = y1 - y0 + 1;
		static std::vector<uint8_t> Glow;
		Glow.assign(static_cast<size_t>(std::max(tw, 0)) * static_cast<size_t>(std::max(th, 0)), 0);
		bool any = false;
		for (int tx = x0; tx <= x1; tx++) {
			for (int ty = y0; ty <= y1; ty++) {
				if (liquid[static_cast<size_t>(tx) * MAXDUNY + ty] != 1)
					continue;
				any = true;
				for (int dx = -2; dx <= 2; dx++) {
					for (int dy = -2; dy <= 2; dy++) {
						const int gx = tx + dx - x0, gy = ty + dy - y0;
						if (gx < 0 || gy < 0 || gx >= tw || gy >= th)
							continue;
						static constexpr int Falloff[5][5] = { { 6, 58, 79, 58, 6 }, { 58, 130, 167, 130, 58 }, { 79, 167, 255, 167, 79 },
							{ 58, 130, 167, 130, 58 }, { 6, 58, 79, 58, 6 } }; // (1 - d / 2.9) * 255, d from the middle (audit round 1: no sqrt)
						const int value = Falloff[dx + 2][dy + 2];
						uint8_t &g = Glow[static_cast<size_t>(gx) * th + gy];
						g = value > g ? static_cast<uint8_t>(value) : g;
					}
				}
			}
		}
		if (any) {
			// The flicker once per tile, into the field (audit round 1: two sines a cell).
			for (int gx = 0; gx < tw; gx++) {
				for (int gy = 0; gy < th; gy++) {
					uint8_t &g = Glow[static_cast<size_t>(gx) * th + gy];
					if (g == 0)
						continue;
					const int tx = gx + x0, ty = gy + y0;
					const float flicker = 0.78F + 0.22F * std::sin(now * 7.0F + static_cast<float>(tx * 3 + ty * 5)) * std::sin(now * 2.3F + static_cast<float>(ty));
					g = static_cast<uint8_t>(static_cast<float>(g) * 0.42F * flicker);
				}
			}
			constexpr int Cell = 8;
			for (int gy = 0; gy * Cell < h; gy++) {
				for (int gx = 0; gx * Cell < w; gx++) {
					// The tile a little below the cell: the glow rises from the floor onto what stands there.
					const float u = (static_cast<float>(gx * Cell + 4) - v.ox) / 32.0F;
					const float vv = (static_cast<float>(gy * Cell + 4) + 22.0F - v.oy) / 16.0F;
					const float fa = (u + vv) * 0.5F + 0.5F, fb = (vv - u) * 0.5F + 0.5F;
					int ttx = static_cast<int>(fa), tty = static_cast<int>(fb);
					ttx -= fa < static_cast<float>(ttx) ? 1 : 0; // floor, by hand
					tty -= fb < static_cast<float>(tty) ? 1 : 0;
					const int gtx = ttx - x0, gty = tty - y0;
					if (gtx < 0 || gty < 0 || gtx >= tw || gty >= th)
						continue;
					const int k = Glow[static_cast<size_t>(gtx) * th + gty];
					if (k == 0)
						continue;
					for (int fy = 0; fy < Cell; fy++) {
						const int y = gy * Cell + fy;
						if (y >= h)
							break;
						uint32_t *row = out.at<uint32_t>(0, y);
						for (int fx = 0; fx < Cell; fx++) {
							const int x = gx * Cell + fx;
							if (x >= w)
								break;
							const uint32_t c = row[x];
							if ((c & 0xFFFFFF) == 0)
								continue;
							const int r = SF_R(c), gg = SF_G(c), b = SF_B(c);
							row[x] = SF_PACK(SF_SAT(r + ((r * k) >> 8) + (k >> 4)), SF_SAT(gg + ((gg * k) >> 9) + (k >> 6)), SF_SAT(b + ((b * k) >> 11)));
						}
					}
				}
			}
		}
	}

	// Drop shadows: a tumbling item's shadow on the floor, smaller and fainter at the top of its arc.
	if (*sgOptions.Oracool.dropEffects) {
		for (uint8_t i = 0; i < ActiveItemCount; i++) {
			const int index = ActiveItems[i];
			if (index < 0 || index > MAXITEMS)
				continue;
			const Item &item = Items[index];
			if (!item._iAnimFlag || !InMap(item.position) || LitAt(item.position) <= 0.05F)
				continue;
			const float hgt = TumbleHeight(item);
			const int cx = static_cast<int>(WorldX(item.position) + v.ox);
			const int cy = static_cast<int>(WorldY(item.position) + v.oy) + 2;
			const float rx = 11.0F * (1.0F - 0.4F * hgt);
			const int a = static_cast<int>(110.0F * (1.0F - 0.55F * hgt));
			for (int dy = -4; dy <= 4; dy++) {
				for (int dx = -12; dx <= 12; dx++) {
					const float u = static_cast<float>(dx) / rx;
					const float vv = static_cast<float>(dy) / (rx * 0.42F);
					const float d = u * u + vv * vv;
					if (d < 1.0F)
						Blend(out, cx + dx, cy + dy, 0, static_cast<int>(static_cast<float>(a) * (1.0F - d)));
				}
			}
		}
	}

	// Blood, bone and dust - lit by the light passes after this, as the floor is.
	for (const Particle &p : Particles) {
		if (p.kind == Kind::Sparkle)
			continue;
		const float age = Clamp01((now - p.born) / p.life);
		const int px = static_cast<int>(p.x + v.ox);
		const int py = static_cast<int>(p.y + v.oy - p.z);
		if (px < -4 || py < -4 || px > w + 4 || py > h + 4)
			continue;
		if (p.kind == Kind::Dust) {
			const int a = static_cast<int>((1.0F - age) * (age < 0.15F ? age / 0.15F : 1.0F) * 120.0F);
			const int r = 1 + static_cast<int>(age * 3.0F);
			for (int dy = -r; dy <= r; dy++)
				for (int dx = -r; dx <= r; dx++)
					if (dx * dx + dy * dy <= r * r)
						Blend(out, px + dx, py + dy, p.rgb, a * (r * r + 1 - dx * dx - dy * dy) / (r * r + 1));
			continue;
		}
		const int a = static_cast<int>((age < 0.7F ? 1.0F : (1.0F - age) / 0.3F) * 230.0F);
		Blend(out, px, py, p.rgb, a);
		if (p.z > 0)
			Blend(out, px, py + 1, p.rgb, a / 2); // a drop in flight, a little streaked
	}
}

// ==== Light, beside the loot beams =========================================================================================================

void DrawSceneFeelLights(const Surface &out, Point firstTile, Point firstTileBuffer)
{
	if (out.isIndexed() || out.w() <= 0 || out.h() <= 0)
		return;
	const View v = MakeView(out, firstTile, firstTileBuffer);
	const LightPen pen = MakePen(out);
	const float now = Now();
	const int w = out.w();
	const int h = out.h();

	// Impact lights: a burst of the element's colour on the floor and the air around the strike.
	if (*sgOptions.Oracool.impactLights) {
		for (const Burst &b : Bursts) {
			const float age = now - b.born;
			if (age < 0 || age > BurstLife)
				continue;
			const float env = (age < 0.04F ? age / 0.04F : std::pow(1.0F - (age - 0.04F) / (BurstLife - 0.04F), 1.6F)) * b.strength;
			const int cx = static_cast<int>(b.x + v.ox);
			const int cy = static_cast<int>(b.y + v.oy);
			const int r = static_cast<int>(64.0F * (0.7F + 0.3F * b.strength));
			if (cx < -r || cy < -r || cx > w + r || cy > h + r)
				continue;
			const float invR = 1.0F / static_cast<float>(r);
			const float kr = static_cast<float>(SF_R(b.rgb)) * env * 0.55F, kg = static_cast<float>(SF_G(b.rgb)) * env * 0.55F,
			            kb = static_cast<float>(SF_B(b.rgb)) * env * 0.55F;
			for (int dy = -r / 2; dy <= r / 2; dy++) {
				const int py = cy + dy;
				if (py < 0 || py >= h)
					continue;
				uint32_t *row = pen.splats == nullptr ? out.at<uint32_t>(0, py) : nullptr;
				for (int dx = -r; dx <= r; dx++) {
					const int px = cx + dx;
					if (px < 0 || px >= w)
						continue;
					const float d = std::sqrt(static_cast<float>(dx * dx + dy * dy * 4)) * invR;
					if (d >= 1.0F)
						continue;
					const float f = (1.0F - d) * (1.0F - d);
					const int ar = static_cast<int>(kr * f), ag = static_cast<int>(kg * f), ab = static_cast<int>(kb * f);
					if ((ar | ag | ab) <= 0)
						continue;
					if (row != nullptr) {
						const uint32_t c = row[px];
						row[px] = SF_ADD(c, ar, ag, ab); // straight onto the picture (audit round 1: a call a pixel)
					} else {
						pen.add(px, py, ar, ag, ab);
					}
				}
			}
			// The air at the strike: a small bright core.
			for (int dy = -8; dy <= 8; dy++)
				for (int dx = -8; dx <= 8; dx++) {
					const float d = std::sqrt(static_cast<float>(dx * dx + dy * dy)) / 8.0F;
					if (d < 1.0F)
						pen.addF(cx + dx, cy - 16 + dy, b.rgb, (1.0F - d) * env * 0.8F);
				}
		}
	}

	// Sparkles behind a rare drop.
	for (const Particle &p : Particles) {
		if (p.kind != Kind::Sparkle)
			continue;
		const float age = Clamp01((now - p.born) / p.life);
		const int px = static_cast<int>(p.x + v.ox);
		const int py = static_cast<int>(p.y + v.oy - p.z);
		const float k = (1.0F - age) * 0.9F;
		pen.addF(px, py, p.rgb, k);
		pen.addF(px - 1, py, p.rgb, k * 0.4F);
		pen.addF(px + 1, py, p.rgb, k * 0.4F);
		pen.addF(px, py - 1, p.rgb, k * 0.4F);
		pen.addF(px, py + 1, p.rgb, k * 0.4F);
	}

	// Area atmosphere: what hangs in the air of each place, seen where there is light.
	if (AtmosphereHere()) {
		const float spanX = static_cast<float>(w + 80);
		const float spanY = static_cast<float>(h + 80);
		const auto wrapped = [&](uint32_t s, float driftX, float driftY, float &sx, float &sy) {
			float x = std::fmod(Unit(Hash(s, 2)) * 5000.0F + now * driftX + v.ox, spanX);
			float y = std::fmod(Unit(Hash(s, 3)) * 5000.0F + now * driftY + v.oy, spanY);
			sx = (x < 0 ? x + spanX : x) - 40.0F;
			sy = (y < 0 ? y + spanY : y) - 40.0F;
		};
		const auto litUnder = [&](float sx, float sy) { return LitAt(TileAtWorld(sx - v.ox, sy - v.oy + 30.0F)); };
		switch (leveltype) {
		case DTYPE_CATHEDRAL:
		case DTYPE_CATACOMBS: {
			// Dust motes turning in the torchlight.
			constexpr uint32_t Mote = SF_PACK(255, 236, 200);
			for (uint32_t i = 0; i < 90; i++) {
				const uint32_t s = Hash(i, 1101);
				float sx, sy;
				wrapped(s, (Unit(s) - 0.5F) * 6.0F, -2.0F - Unit(s >> 4) * 4.0F, sx, sy);
				sx += std::sin(now * (0.4F + Unit(s >> 8) * 0.5F) + Unit(s >> 12) * 6.28F) * 9.0F;
				const float lit = litUnder(sx, sy);
				const float twinkle = 0.55F + 0.45F * std::sin(now * (1.3F + Unit(s >> 16)) + static_cast<float>(i));
				pen.addF(static_cast<int>(sx), static_cast<int>(sy), Mote, lit * lit * twinkle * 0.45F);
			}
			break;
		}
		case DTYPE_CAVES: {
			// Drips from the cave roof, and their rings where they land.
			int x0, x1, y0, y1;
			ViewTiles(v, x0, x1, y0, y1);
			constexpr uint32_t Drop = SF_PACK(150, 190, 230);
			for (int tx = x0; tx <= x1; tx++) {
				for (int ty = y0; ty <= y1; ty++) {
					const uint32_t s = Hash(static_cast<uint32_t>(tx), static_cast<uint32_t>(ty), 1201);
					if (s % 23 != 0 || !Walkable({ tx, ty }))
						continue;
					const float lit = LitAt({ tx, ty });
					if (lit <= 0.05F)
						continue;
					const float cycle = Frac(now / (2.5F + Unit(s >> 6) * 3.0F) + Unit(s >> 11));
					const float cx = WorldX({ tx, ty }) + v.ox + (Unit(s >> 3) - 0.5F) * 30.0F;
					const float cy = WorldY({ tx, ty }) + v.oy + (Unit(s >> 9) - 0.5F) * 12.0F;
					if (cycle < 0.16F) {
						const float t = cycle / 0.16F;
						const int py = static_cast<int>(cy - 70.0F * (1.0F - t * t));
						pen.addF(static_cast<int>(cx), py, Drop, lit * 0.7F);
						pen.addF(static_cast<int>(cx), py - 1, Drop, lit * 0.35F);
					} else if (cycle < 0.32F) {
						const float t = (cycle - 0.16F) / 0.16F;
						const float r = 1.5F + t * 6.0F;
						const int steps = 14;
						for (int k = 0; k < steps; k++) {
							const float a = static_cast<float>(k) / static_cast<float>(steps) * 6.283F;
							pen.addF(static_cast<int>(cx + std::cos(a) * r), static_cast<int>(cy + std::sin(a) * r * 0.5F), Drop, lit * (1.0F - t) * 0.45F);
						}
					}
				}
			}
			break;
		}
		case DTYPE_HELL: {
			// Ash drifting down, and embers rising.
			for (uint32_t i = 0; i < 110; i++) {
				const uint32_t s = Hash(i, 1301);
				float sx, sy;
				wrapped(s, 6.0F + Unit(s) * 6.0F, 14.0F + Unit(s >> 4) * 12.0F, sx, sy);
				sx += std::sin(now * 0.7F + Unit(s >> 8) * 6.28F) * 6.0F;
				const float lit = 0.3F + 0.7F * litUnder(sx, sy);
				pen.addF(static_cast<int>(sx), static_cast<int>(sy), SF_PACK(150, 140, 136), lit * 0.35F);
			}
			for (uint32_t i = 0; i < 28; i++) {
				const uint32_t s = Hash(i, 1302);
				float sx, sy;
				wrapped(s, (Unit(s) - 0.5F) * 10.0F, -30.0F - Unit(s >> 4) * 26.0F, sx, sy);
				sx += std::sin(now * 2.1F + Unit(s >> 8) * 6.28F) * 4.0F;
				const float flick = 0.5F + 0.5F * std::sin(now * 9.0F + static_cast<float>(i) * 1.7F);
				pen.addF(static_cast<int>(sx), static_cast<int>(sy), SF_PACK(255, 130, 40), (0.35F + 0.4F * flick) * (0.4F + 0.6F * litUnder(sx, sy)));
			}
			break;
		}
		case DTYPE_CRYPT: {
			// Pale wisps drifting slowly through the halls, coming and going.
			for (uint32_t i = 0; i < 9; i++) {
				const uint32_t s = Hash(i, 1401);
				float sx, sy;
				wrapped(s, (Unit(s) - 0.5F) * 14.0F, (Unit(s >> 4) - 0.5F) * 8.0F, sx, sy);
				const float life = std::sin(now * (0.25F + Unit(s >> 8) * 0.2F) + Unit(s >> 12) * 6.28F);
				if (life <= 0)
					continue;
				const float k = life * 0.30F * (0.4F + 0.6F * litUnder(sx, sy));
				for (int dy = -7; dy <= 7; dy++)
					for (int dx = -7; dx <= 7; dx++) {
						const float d = std::sqrt(static_cast<float>(dx * dx + dy * dy)) / 7.0F;
						if (d < 1.0F)
							pen.addF(static_cast<int>(sx) + dx, static_cast<int>(sy) + dy, SF_PACK(160, 220, 255), k * (1.0F - d) * (1.0F - d));
					}
			}
			break;
		}
		case DTYPE_NEST: {
			// Spores floating up.
			for (uint32_t i = 0; i < 70; i++) {
				const uint32_t s = Hash(i, 1501);
				float sx, sy;
				wrapped(s, (Unit(s) - 0.5F) * 8.0F, -6.0F - Unit(s >> 4) * 8.0F, sx, sy);
				sx += std::sin(now * 0.9F + Unit(s >> 8) * 6.28F) * 5.0F;
				const float lit = litUnder(sx, sy);
				pen.addF(static_cast<int>(sx), static_cast<int>(sy), SF_PACK(200, 230, 110), (0.25F + 0.75F * lit) * 0.42F);
			}
			break;
		}
		default: break;
		}
	}

	// Window light in the Cathedral: soft beams slanting down from high on the walls that face a room, motes in them.
	if (*sgOptions.Oracool.windowLight && leveltype == DTYPE_CATHEDRAL) {
		int x0, x1, y0, y1;
		ViewTiles(v, x0, x1, y0, y1);
		constexpr uint32_t Beam = SF_PACK(255, 244, 214);
		// At most the ten nearest the hero (audit round 1: a wide view held thirty, each thousands of pixels).
		struct Window {
			int tx, ty, d;
		};
		static std::vector<Window> Windows;
		Windows.clear();
		const Point hero = MyPlayer != nullptr ? Point(MyPlayer->position.tile) : Point { (x0 + x1) / 2, (y0 + y1) / 2 };
		for (int tx = x0; tx <= x1; tx++)
			for (int ty = y0; ty <= y1; ty++)
				if (Hash(static_cast<uint32_t>(tx), static_cast<uint32_t>(ty), 1601) % 9 == 0 && InMap({ tx, ty }) && TileHasAny(dPiece[tx][ty], TileProperties::Solid))
					Windows.push_back({ tx, ty, std::abs(tx - hero.x) + std::abs(ty - hero.y) });
		// Ties broken by place, so the same ten hold from frame to frame (audit round 2).
		std::sort(Windows.begin(), Windows.end(), [](const Window &a, const Window &b) {
			return a.d != b.d ? a.d < b.d : (a.tx != b.tx ? a.tx < b.tx : a.ty < b.ty);
		});
		int beams = 0;
		for (const Window &win : Windows) {
			if (beams >= 10)
				break;
			const int tx = win.tx, ty = win.ty;
			{
				const uint32_t s = Hash(static_cast<uint32_t>(tx), static_cast<uint32_t>(ty), 1601);
				// The room it faces: the tile south-east or south-west of the wall (the walls drawn facing the viewer).
				const bool east = (s >> 8) & 1;
				const Point step = east ? Point { 1, 0 } : Point { 0, 1 };
				const Point floor1 { tx + step.x, ty + step.y };
				const Point floor2 { tx + step.x * 3, ty + step.y * 3 };
				if (!Walkable(floor1) || !Walkable(floor2) || !HasAnyOf(dFlags[floor1.x][floor1.y], DungeonFlag::Explored))
					continue;
				const float lit = std::max(LitAt(floor1), 0.25F);
				const float topX = WorldX({ tx, ty }) + v.ox;
				const float topY = WorldY({ tx, ty }) + v.oy - 110.0F;
				const float botX = WorldX(floor2) + v.ox;
				const float botY = WorldY(floor2) + v.oy;
				if (std::max(topX, botX) < -60 || std::min(topX, botX) > static_cast<float>(w) + 60 || botY < 0 || topY > static_cast<float>(h))
					continue;
				beams++;
				const float breathe = 0.85F + 0.15F * std::sin(now * 0.35F + Unit(s >> 12) * 6.28F);
				const int yA = std::max(0, static_cast<int>(topY));
				const int yB = std::min(h - 1, static_cast<int>(botY));
				for (int y = yA; y <= yB; y++) {
					const float t = (static_cast<float>(y) - topY) / (botY - topY);
					const float cx = topX + (botX - topX) * t;
					const float half = 9.0F + 16.0F * t;
					const float fall = (t < 0.15F ? t / 0.15F : 1.0F) * (1.0F - 0.55F * t);
					const int xa = std::max(0, static_cast<int>(cx - half));
					const int xb = std::min(w - 1, static_cast<int>(cx + half));
					const float peak = fall * breathe * lit * 0.11F;
					const float invHalf = 1.0F / half;
					uint32_t *row = pen.splats == nullptr ? out.at<uint32_t>(0, y) : nullptr;
					for (int x = xa; x <= xb; x++) {
						const float off = static_cast<float>(x) - cx;
						const float across = 1.0F - (off < 0 ? -off : off) * invHalf;
						const float k = across * across * peak;
						const int ar = static_cast<int>(255.0F * k), ag = static_cast<int>(244.0F * k), ab = static_cast<int>(214.0F * k);
						if ((ar | ag | ab) <= 0)
							continue;
						if (row != nullptr) {
							const uint32_t c = row[x];
							row[x] = SF_ADD(c, ar, ag, ab); // straight onto the picture (audit round 1)
						} else {
							pen.add(x, y, ar, ag, ab);
						}
					}
				}
				for (uint32_t m = 0; m < 4; m++) {
					const float t = Frac(now * 0.05F + Unit(Hash(s, m, 7)));
					const float cx = topX + (botX - topX) * t + std::sin(now * 0.8F + static_cast<float>(m)) * 6.0F;
					const float cy = topY + (botY - topY) * t;
					pen.addF(static_cast<int>(cx), static_cast<int>(cy), Beam, lit * 0.5F * std::sin(t * Pi));
				}
			}
		}
	}
}

// ==== After the world ===================================================================================================================

Point WorldPointerFor(Point screen)
{
	const Displacement shift = ScreenShakeOffset();
	float x = static_cast<float>(screen.x - shift.deltaX);
	float y = static_cast<float>(screen.y - shift.deltaY);
	if (FrameViewValid && Lean > 0.002F && LeanW > 0 && LeanH > 0 && y < static_cast<float>(LeanH)) {
		const float k = 1.0F + 0.05F * Smooth(Lean);
		const float cx = static_cast<float>(LeanW) * 0.5F;
		const float cy = static_cast<float>(LeanH) * 0.5F;
		x = cx + (x - cx) / k;
		y = cy + (y - cy) / k;
	}
	return { static_cast<int>(std::floor(x + 0.5F)), static_cast<int>(std::floor(y + 0.5F)) };
}

Displacement CameraGlideOffset()
{
	if (!*sgOptions.Oracool.smoothCamera)
		return {};
	const float age = Now() - GlideAt;
	if (age < 0 || age >= GlideLength)
		return {};
	// Eased out: most of the way at once, the last of it slowly.
	const float t = age / GlideLength;
	const float left = (1.0F - t) * (1.0F - t);
	return { static_cast<int>(std::floor(GlideX * left + 0.5F)), static_cast<int>(std::floor(GlideY * left + 0.5F)) };
}

void ApplySceneFeelAfterWorld(const Surface &world)
{
	LeanW = world.w();
	LeanH = world.h();
	if (world.isIndexed() || !FrameViewValid || Lean <= 0.002F || MyPlayer == nullptr || world.w() <= 0 || world.h() <= 0)
		return;
	// The lean-in: the finished world enlarged up to 5% about the screen's middle, sampled bilinearly.
	const float k = 1.0F + 0.05F * Smooth(Lean);
	const int w = world.w();
	const int h = world.h();
	static std::vector<uint32_t> Copy;
	Copy.resize(static_cast<size_t>(w) * h);
	for (int y = 0; y < h; y++)
		std::memcpy(&Copy[static_cast<size_t>(y) * w], world.at<uint32_t>(0, y), static_cast<size_t>(w) * 4);
	const float cx = static_cast<float>(w) * 0.5F;
	const float cy = static_cast<float>(h) * 0.5F;
	// Per column, once: the two source columns and the weight (audit round 1: the per-pixel channel split cost ~15 ms in
	// Debug for the whole fight). The blends below work on the packed word, red and blue together.
	static std::vector<int> Col0, Col1, ColW;
	Col0.resize(static_cast<size_t>(w));
	Col1.resize(static_cast<size_t>(w));
	ColW.resize(static_cast<size_t>(w));
	for (int x = 0; x < w; x++) {
		const float sx = cx + (static_cast<float>(x) - cx) / k;
		int ix = static_cast<int>(sx);
		ix = ix < 0 ? 0 : (ix > w - 1 ? w - 1 : ix);
		Col0[static_cast<size_t>(x)] = ix;
		Col1[static_cast<size_t>(x)] = ix + 1 < w ? ix + 1 : w - 1;
		ColW[static_cast<size_t>(x)] = static_cast<int>((sx - static_cast<float>(ix)) * 256.0F) & 0xFF;
	}
	const int *col0 = Col0.data();
	const int *col1 = Col1.data();
	const int *colW = ColW.data();
	const uint32_t *src = Copy.data();
	for (int y = 0; y < h; y++) {
		const float sy = cy + (static_cast<float>(y) - cy) / k;
		int iy = static_cast<int>(sy);
		iy = iy < 0 ? 0 : (iy > h - 1 ? h - 1 : iy);
		const uint32_t fy = static_cast<uint32_t>((sy - static_cast<float>(iy)) * 256.0F) & 0xFF;
		const int iy1 = iy + 1 < h ? iy + 1 : h - 1;
		const uint32_t *r0 = src + static_cast<size_t>(iy) * w;
		const uint32_t *r1 = src + static_cast<size_t>(iy1) * w;
		uint32_t *dst = world.at<uint32_t>(0, y);
		for (int x = 0; x < w; x++) {
			const uint32_t fx = static_cast<uint32_t>(colW[x]);
			const uint32_t a = r0[col0[x]], b = r0[col1[x]], c = r1[col0[x]], d = r1[col1[x]];
			// Lerp a-b and c-d by fx, then the two by fy: red and blue in one word, green in another.
			const uint32_t topRB = (((a & 0xFF00FFU) * (256 - fx) + (b & 0xFF00FFU) * fx) >> 8) & 0xFF00FFU;
			const uint32_t topG = (((a & 0x00FF00U) * (256 - fx) + (b & 0x00FF00U) * fx) >> 8) & 0x00FF00U;
			const uint32_t botRB = (((c & 0xFF00FFU) * (256 - fx) + (d & 0xFF00FFU) * fx) >> 8) & 0xFF00FFU;
			const uint32_t botG = (((c & 0x00FF00U) * (256 - fx) + (d & 0x00FF00U) * fx) >> 8) & 0x00FF00U;
			dst[x] = ((((topRB * (256 - fy) + botRB * fy) >> 8) & 0xFF00FFU) | (((topG * (256 - fy) + botG * fy) >> 8) & 0x00FF00U));
		}
	}
}

// ==== The iris ==============================================================================================================================

bool IrisFadeActive()
{
	// The game's own fade only - not the loading screen's (audit round 1: it opened and closed like an iris too).
	return *sgOptions.Oracool.levelTransition && gbRunGame && !IsShowingProgress();
}

int IrisFadeAt(int x, int y, int w, int h, int fade)
{
	if (!IrisFadeActive() || w <= 0 || h <= 0)
		return -1;
	if (fade >= 256)
		return 256;
	if (fade <= 0)
		return 0;
	// The circle's radius grows with the fade from nothing to past the corners; a soft edge an eighth of the screen wide.
	const float dx = static_cast<float>(x) - static_cast<float>(w) * 0.5F;
	const float dy = static_cast<float>(y) - static_cast<float>(h) * 0.5F;
	const float corner = std::sqrt(static_cast<float>(w * w + h * h)) * 0.5F;
	const float edge = corner * 0.25F;
	const float radius = static_cast<float>(fade) / 256.0F * (corner + edge);
	const float d = std::sqrt(dx * dx + dy * dy);
	const float inside = Clamp01((radius - d) / edge);
	return static_cast<int>(Smooth(inside) * 256.0F);
}

void IrisFadeRow(int y, int w, int h, int fade, uint16_t *out)
{
	// Solid runs where the row is wholly inside or outside the circle; per pixel only across the soft band, and the right
	// half the mirror of the left (audit round 1: a sqrt a pixel was ~10 ms a frame in Debug).
	const float dy = static_cast<float>(y) - static_cast<float>(h) * 0.5F;
	const float corner = std::sqrt(static_cast<float>(w * w + h * h)) * 0.5F;
	const float edge = corner * 0.25F;
	const float radius = static_cast<float>(fade) / 256.0F * (corner + edge);
	const float inner = radius - edge;
	const float dy2 = dy * dy;
	const float halfW = static_cast<float>(w) * 0.5F;
	const float outerX2 = radius * radius - dy2;
	const float innerX2 = inner > 0 ? inner * inner - dy2 : -1.0F;
	const float invEdge = 1.0F / edge;
	const int half = (w + 1) / 2;
	for (int x = 0; x < half; x++) {
		const float dx = halfW - static_cast<float>(x) - 0.5F;
		const float dx2 = dx * dx;
		uint16_t v;
		if (dx2 >= outerX2)
			v = 0;
		else if (dx2 <= innerX2)
			v = 256;
		else {
			float inside = (radius - std::sqrt(dx2 + dy2)) * invEdge;
			inside = inside < 0 ? 0 : (inside > 1 ? 1 : inside);
			v = static_cast<uint16_t>(inside * inside * (3.0F - 2.0F * inside) * 256.0F);
		}
		out[x] = v;
		out[w - 1 - x] = v;
	}
}

bool DampFloorsHere()
{
	return *sgOptions.Oracool.wetFloors && (leveltype == DTYPE_CAVES || leveltype == DTYPE_NEST);
}

void ForgetSceneFeel()
{
	Particles.clear();
	HitAt.fill(0);
	DeathSeen.fill(0);
	Drops.fill({});
	Flights.clear();
	Bursts.clear();
	HeroLastValid = false;
	GlideAt = -100;
	Lean = 0;
	LastTick = -1;
}

int SceneFeelParticleCount()
{
	return static_cast<int>(Particles.size());
}

} // namespace devilution::oracool

#undef SF_R
#undef SF_G
#undef SF_B
#undef SF_PACK
#undef SF_MIX
#undef SF_SAT
#undef SF_ADD

#ifdef _MSC_VER
#pragma optimize("", on)
#pragma runtime_checks("", restore)
#endif
